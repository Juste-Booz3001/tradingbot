#include "OrderExecutor.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

namespace tradingbot {

namespace {
constexpr double kQtyEpsilon = 1e-9;

// Déduit l'actif de base d'un symbole spot (ex. "BTCUSDT" -> "BTC").
// Heuristique par suffixe connu — suffisant pour Binance, à remplacer par
// un mapping explicite symbole -> actif si d'autres exchanges/marchés
// (actions, forex) sont branchés un jour sur ce même OrderExecutor.
std::string extractBaseAsset(const std::string& symbol) {
    static const std::vector<std::string> knownQuotes = {
        "USDT", "BUSD", "USDC", "FDUSD", "EUR", "GBP", "TRY", "BTC", "ETH", "BNB"
    };
    for (const auto& quote : knownQuotes) {
        if (symbol.size() > quote.size() &&
            symbol.compare(symbol.size() - quote.size(), quote.size(), quote) == 0) {
            return symbol.substr(0, symbol.size() - quote.size());
        }
    }
    return symbol;
}
}

OrderExecutor::OrderExecutor(std::shared_ptr<IMarketConnector> connector)
    : connector_(std::move(connector)) {
    // Certains connecteurs (ex. BinanceConnector sur un ordre MARKET) remontent
    // le fill de façon synchrone dans placeOrder() ; d'autres le feront plus
    // tard via ce callback (ordre LIMIT qui s'exécute en différé, exchange
    // asynchrone type websocket de confirmations). Dans les deux cas on
    // centralise le traitement ici.
    if (connector_) {
        connector_->onOrderUpdate([this](const Order& update) { onOrderUpdate(update); });
    }
}

std::string OrderExecutor::execute(const Order& order) {
    // TODO: écrire l'ordre en base AVANT l'envoi (write-ahead log) afin de
    // pouvoir le retrouver au redémarrage même si le process crash juste
    // après l'appel réseau.

    int attempt = 0;
    int backoffMs = 500;
    while (attempt < maxRetries_) {
        try {
            std::string orderId = connector_->placeOrder(order);
            pendingOrders_[orderId] = order;
            return orderId;
        } catch (const std::exception& e) {
            std::cerr << "[OrderExecutor] Échec tentative " << (attempt + 1)
                      << "/" << maxRetries_ << " : " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
            backoffMs *= 2;
            ++attempt;
        }
    }
    throw std::runtime_error("OrderExecutor: échec après " + std::to_string(maxRetries_) + " tentatives");
}

void OrderExecutor::onOrderUpdate(const Order& update) {
    pendingOrders_[update.id] = update;
    // TODO: persister la mise à jour en base (table trades / positions).

    if (update.status == OrderStatus::Filled || update.status == OrderStatus::PartiallyFilled) {
        applyFill(update);
    }
}

void OrderExecutor::applyFill(const Order& update) {
    std::lock_guard<std::mutex> lock(positionsMutex_);

    auto it = openPositions_.find(update.symbol);

    // Cas 1 : pas de position ouverte sur ce symbole -> on en ouvre une.
    if (it == openPositions_.end()) {
        Position pos;
        pos.symbol = update.symbol;
        pos.market = update.market;
        pos.side = update.side;
        pos.quantity = update.quantity;
        pos.entryPrice = update.fillPrice > 0.0 ? update.fillPrice : update.price;
        pos.stopLoss = update.stopLoss;
        pos.takeProfit = update.takeProfit;
        pos.unrealizedPnl = 0.0;
        openPositions_[update.symbol] = pos;
        if (onPositionChanged_) onPositionChanged_(pos);
        return;
    }

    Position& pos = it->second;
    double fillPrice = update.fillPrice > 0.0 ? update.fillPrice : update.price;

    // Cas 2 : même sens -> on agrandit la position (moyenne du prix d'entrée).
    if (update.side == pos.side) {
        double newQty = pos.quantity + update.quantity;
        pos.entryPrice = (pos.entryPrice * pos.quantity + fillPrice * update.quantity) / newQty;
        pos.quantity = newQty;
        if (onPositionChanged_) onPositionChanged_(pos);
        return;
    }

    // Cas 3 : sens opposé -> on réduit ou on clôture la position, avec PnL
    // réalisé sur la portion effectivement close.
    double closingQty = std::min(pos.quantity, update.quantity);
    double direction = (pos.side == OrderSide::Buy) ? 1.0 : -1.0;
    double realizedPnl = (fillPrice - pos.entryPrice) * closingQty * direction;

    if (onPositionClosed_) onPositionClosed_(update, closingQty, realizedPnl);

    double remainingOnPosition = pos.quantity - closingQty;
    double remainingOnFill = update.quantity - closingQty;

    if (remainingOnPosition > kQtyEpsilon) {
        // Position réduite mais toujours ouverte, même sens, même entrée.
        pos.quantity = remainingOnPosition;
        if (onPositionChanged_) onPositionChanged_(pos);
    } else if (remainingOnFill > kQtyEpsilon) {
        // Le fill dépassait la taille de la position : elle se retourne
        // (ex. on était long 1 BTC, on vend 1.5 BTC -> on finit short 0.5 BTC).
        Position flipped;
        flipped.symbol = update.symbol;
        flipped.market = update.market;
        flipped.side = update.side;
        flipped.quantity = remainingOnFill;
        flipped.entryPrice = fillPrice;
        flipped.unrealizedPnl = 0.0;
        openPositions_[update.symbol] = flipped;
        if (onPositionChanged_) onPositionChanged_(flipped);
    } else {
        // Clôture exacte : plus aucune exposition sur ce symbole.
        openPositions_.erase(it);
    }
}

void OrderExecutor::updateUnrealizedPnl(const std::string& symbol, double currentPrice) {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    auto it = openPositions_.find(symbol);
    if (it == openPositions_.end()) return;

    Position& pos = it->second;
    double direction = (pos.side == OrderSide::Buy) ? 1.0 : -1.0;
    pos.unrealizedPnl = (currentPrice - pos.entryPrice) * pos.quantity * direction;

    if (onPositionChanged_) onPositionChanged_(pos);
}

std::optional<Position> OrderExecutor::getPosition(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    auto it = openPositions_.find(symbol);
    if (it == openPositions_.end()) return std::nullopt;
    return it->second;
}

void OrderExecutor::reconcileFromDatabase(const std::vector<Position>& dbPositions) {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    std::cout << "[OrderExecutor] Réconciliation d'état au démarrage : "
              << dbPositions.size() << " position(s) ouverte(s) en base\n";

    for (const auto& pos : dbPositions) {
        openPositions_[pos.symbol] = pos;

        if (connector_) {
            try {
                std::string asset = extractBaseAsset(pos.symbol);
                double exchangeBalance = connector_->getAssetBalance(asset);

                if (exchangeBalance >= 0.0) {
                    double diff = std::abs(exchangeBalance - pos.quantity);
                    // Tolérance 0.1% (arrondis/frais) avec un plancher pour les petites quantités.
                    double tolerance = std::max(pos.quantity * 0.001, 1e-6);
                    if (diff > tolerance) {
                        std::string msg =
                            "Divergence détectée sur " + pos.symbol +
                            " : DB=" + std::to_string(pos.quantity) +
                            " exchange=" + std::to_string(exchangeBalance) +
                            " (actif vérifié: " + asset + ") — "
                            "à vérifier manuellement avant de reprendre le trading automatique.";
                        std::cerr << "[OrderExecutor] /!\\ " << msg << "\n";
                        if (onAlert_) onAlert_("critical", msg);
                    } else {
                        std::cout << "[OrderExecutor] " << pos.symbol
                                  << " réconciliée avec l'exchange (écart dans la tolérance)\n";
                    }
                }
            } catch (const std::exception& e) {
                std::string msg = "Impossible de vérifier le solde exchange pour " +
                                   pos.symbol + " : " + e.what();
                std::cerr << "[OrderExecutor] " << msg << "\n";
                if (onAlert_) onAlert_("warning", msg);
            }
        }

        if (onPositionChanged_) onPositionChanged_(pos);
    }
}

} // namespace tradingbot
