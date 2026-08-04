#include "RiskManager.hpp"
#include <iostream>
#include <algorithm>

namespace tradingbot {

RiskManager::RiskManager(double startingCapital)
    : startingCapital_(startingCapital), equity_(startingCapital), peakEquity_(startingCapital) {}

void RiskManager::setProfile(MarketType market, const MarketRiskProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    profiles_[static_cast<int>(market)] = profile;
}

bool RiskManager::dailyLimitBreached(MarketType market) const {
    auto profileIt = profiles_.find(static_cast<int>(market));
    auto pnlIt = dailyPnlByMarket_.find(static_cast<int>(market));
    if (profileIt == profiles_.end() || pnlIt == dailyPnlByMarket_.end()) return false;

    double lossPct = -pnlIt->second / startingCapital_ * 100.0;
    return lossPct >= profileIt->second.maxDailyDrawdownPct;
}

std::optional<Order> RiskManager::evaluate(const Signal& signal, double currentPrice) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (halted_) {
        std::cout << "[RiskManager] Trade refusé — bot en arrêt (" << haltReason_ << ")\n";
        return std::nullopt;
    }

    auto it = profiles_.find(static_cast<int>(signal.market));
    if (it == profiles_.end()) {
        std::cout << "[RiskManager] Aucun profil de risque pour ce marché — trade refusé\n";
        return std::nullopt;
    }
    const MarketRiskProfile& profile = it->second;

    if (dailyLimitBreached(signal.market)) {
        std::cout << "[RiskManager] Drawdown quotidien max atteint pour ce marché — trade refusé\n";
        return std::nullopt;
    }

    double currentExposure = exposureByMarket_[static_cast<int>(signal.market)];
    if (currentExposure >= profile.maxExposurePct) {
        std::cout << "[RiskManager] Exposition max atteinte pour ce marché — trade refusé\n";
        return std::nullopt;
    }

    // Position sizing : le capital risqué par trade est un % fixe de l'équité,
    // pondéré par la confiance du signal (0.0 - 1.0). Ne JAMAIS utiliser une
    // taille de position fixe en unités — toujours en % du capital.
    double riskAmount = equity_ * (profile.maxRiskPerTradePct / 100.0) * signal.confidence;
    double stopDistance = currentPrice * (profile.defaultStopLossPct / 100.0);
    if (stopDistance <= 0.0) return std::nullopt;

    double quantity = riskAmount / stopDistance;

    Order order;
    order.symbol = signal.symbol;
    order.market = signal.market;
    order.side = signal.side;
    order.quantity = quantity;
    order.price = 0.0; // ordre au marché par défaut
    order.stopLoss = (signal.side == OrderSide::Buy)
        ? currentPrice - stopDistance
        : currentPrice + stopDistance;
    order.takeProfit = (signal.side == OrderSide::Buy)
        ? currentPrice + stopDistance * 1.5   // ratio risque/récompense 1:1.5 par défaut
        : currentPrice - stopDistance * 1.5;
    order.createdAt = std::chrono::system_clock::now();

    return order;
}

void RiskManager::recordTradeResult(MarketType market, double pnl) {
    std::lock_guard<std::mutex> lock(mutex_);
    equity_ += pnl;
    peakEquity_ = std::max(peakEquity_, equity_);
    dailyPnlByMarket_[static_cast<int>(market)] += pnl;

    double drawdownPct = (peakEquity_ - equity_) / peakEquity_ * 100.0;
    if (drawdownPct >= 20.0) { // seuil global de sécurité, ajustable
        haltTrading("Drawdown global >= 20% depuis le pic d'équité");
    }
}

void RiskManager::haltTrading(const std::string& reason) {
    halted_ = true;
    haltReason_ = reason;
    std::cout << "[RiskManager] ARRÊT D'URGENCE : " << reason << "\n";
}

void RiskManager::resumeTrading() {
    halted_ = false;
    haltReason_.clear();
}

double RiskManager::currentDrawdownPct() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (peakEquity_ <= 0.0) return 0.0;
    return (peakEquity_ - equity_) / peakEquity_ * 100.0;
}

} // namespace tradingbot
