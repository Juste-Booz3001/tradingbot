#include "Types.hpp"
#include "IStrategy.hpp"
#include "RiskManager.hpp"
#include "OrderExecutor.hpp"
#include "BinanceConnector.hpp"
#include "ApiServer.hpp"
#include "Database.hpp"
#include "Jwt.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <memory>
#include <unordered_map>

// Nécessite nlohmann/json (https://github.com/nlohmann/json) — header-only,
// à placer dans backend/include/nlohmann/json.hpp ou installer via le
// gestionnaire de paquets système (libnlohmann-json3-dev sur Ubuntu).
#include <nlohmann/json.hpp>

using namespace tradingbot;

namespace {
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <chemin_vers_config.json>\n";
        return 1;
    }

    std::ifstream configFile(argv[1]);
    if (!configFile) {
        std::cerr << "Impossible d'ouvrir le fichier de config : " << argv[1] << "\n";
        return 1;
    }
    nlohmann::json config;
    configFile >> config;

    bool paperTrading = config.value("paper_trading", true);
    double startingCapital = config.value("starting_capital", 10000.0);

    std::cout << "== TradingBot ==\n";
    std::cout << "Mode : " << (paperTrading ? "PAPER TRADING (simulation)" : "ARGENT RÉEL") << "\n";
    std::cout << "Capital de départ : " << startingCapital << "\n";

    if (!paperTrading) {
        std::cout << "\n/!\\ Vous êtes sur le point de trader avec de l'argent réel.\n";
        std::cout << "Assurez-vous d'avoir backtesté et paper-tradé cette configuration\n";
        std::cout << "pendant plusieurs mois avant de continuer.\n";
    }

    // --- Authentification du dashboard ---
    auto authCfg = config.value("auth", nlohmann::json::object());
    std::string authUsername = authCfg.value("username", std::string("admin"));
    std::string authPasswordSha256 = authCfg.value("password_sha256", std::string(""));
    std::string jwtSecret = authCfg.value("jwt_secret", std::string(""));

    if (authPasswordSha256.empty() || jwtSecret.empty()) {
        std::cerr << "\n/!\\ config.auth.password_sha256 ou config.auth.jwt_secret est vide.\n";
        std::cerr << "Le dashboard ne pourra pas s'authentifier tant que ces valeurs ne sont\n";
        std::cerr << "pas renseignées. Voir config/config.example.json pour la marche à suivre.\n\n";
    }
    AuthConfig authConfig{authUsername, authPasswordSha256, jwtSecret};

    // --- Initialisation du Risk Manager avec un profil par marché ---
    auto riskManager = std::make_shared<RiskManager>(startingCapital);

    riskManager->setProfile(MarketType::Crypto, MarketRiskProfile{
        MarketType::Crypto,
        /*maxRiskPerTradePct=*/ 1.0,
        /*defaultStopLossPct=*/ 3.0,   // plus large : crypto plus volatile
        /*maxDailyDrawdownPct=*/ 5.0,
        /*maxExposurePct=*/ 30.0
    });

    riskManager->setProfile(MarketType::Stocks, MarketRiskProfile{
        MarketType::Stocks,
        /*maxRiskPerTradePct=*/ 1.0,
        /*defaultStopLossPct=*/ 1.5,
        /*maxDailyDrawdownPct=*/ 4.0,
        /*maxExposurePct=*/ 40.0
    });

    riskManager->setProfile(MarketType::Forex, MarketRiskProfile{
        MarketType::Forex,
        /*maxRiskPerTradePct=*/ 1.0,
        /*defaultStopLossPct=*/ 0.5,   // plus serré : forex moins volatil
        /*maxDailyDrawdownPct=*/ 3.0,
        /*maxExposurePct=*/ 50.0
    });

    // --- Base de données (créée tôt : le callback de ticks en a besoin) ---
    auto dbConfig = config.value("database", nlohmann::json::object());
    std::string connStr =
        "host=" + dbConfig.value("host", std::string("localhost")) +
        " port=" + std::to_string(dbConfig.value("port", 5432)) +
        " dbname=" + dbConfig.value("dbname", std::string("tradingbot")) +
        " user=" + dbConfig.value("user", std::string("postgres")) +
        " password=" + dbConfig.value("password", std::string(""));
    auto database = std::make_shared<Database>(connStr);

    // Point de départ visible immédiatement dans la courbe d'équité du
    // dashboard, sans attendre la clôture du premier trade (sinon le
    // graphique reste vide pendant potentiellement des heures).
    database->updateEquitySnapshot(riskManager->currentEquity(), riskManager->currentDrawdownPct());

    // --- API web (créée tôt aussi : run() est appelé bien plus bas, mais
    // l'objet doit déjà exister pour que le callback de ticks puisse
    // diffuser aux clients WebSocket dès qu'un prix arrive) ---
    int apiPort = config.value("api_port", 8080);
    ApiServer apiServer(riskManager, database, authConfig, paperTrading, apiPort);

    // --- Connecteur crypto ---
    auto binanceConnector = std::make_shared<BinanceConnector>(
        config.value("binance_api_key", ""),
        config.value("binance_api_secret", ""),
        /*testnet=*/ paperTrading
    );

    auto strategy = std::make_unique<MovingAverageCrossStrategy>(9, 21);
    auto executor = std::make_shared<OrderExecutor>(binanceConnector);

    // --- Câblage persistance + diffusion temps réel ---
    // À chaque changement de position (ouverture, agrandissement, réduction,
    // recalcul de PnL non réalisé sur tick) : on persiste l'état en base
    // (pour survivre à un crash) et on pousse au dashboard.
    executor->setOnPositionChanged([&](const Position& pos) {
        database->upsertOpenPosition(pos);

        nlohmann::json msg = {
            {"type", "position_update"},
            {"symbol", pos.symbol},
            {"side", pos.side == OrderSide::Buy ? "buy" : "sell"},
            {"quantity", pos.quantity},
            {"entry_price", pos.entryPrice},
            {"unrealized_pnl", pos.unrealizedPnl},
            {"stop_loss", pos.stopLoss},
            {"take_profit", pos.takeProfit}
        };
        apiServer.broadcast(msg.dump());
    });

    // À la clôture (totale ou partielle) d'une position : trade persisté en
    // base, RiskManager mis à jour (équité + drawdown), et courbe d'équité
    // diffusée en direct au dashboard.
    executor->setOnPositionClosed([&](const Order& fill, double closingQty, double realizedPnl) {
        database->insertTrade(fill, fill.fillPrice, realizedPnl);
        database->closePosition(fill.symbol);

        riskManager->recordTradeResult(fill.market, realizedPnl);
        double equity = riskManager->currentEquity();
        double drawdown = riskManager->currentDrawdownPct();
        database->updateEquitySnapshot(equity, drawdown);

        nlohmann::json equityMsg = {
            {"type", "equity_update"},
            {"equity", equity},
            {"drawdown_pct", drawdown},
            {"ts", nowMs()}
        };
        apiServer.broadcast(equityMsg.dump());

        // Prévient le dashboard que la position a disparu — sans ça, le
        // dernier position_update reçu reste affiché indéfiniment.
        nlohmann::json closedMsg = {
            {"type", "position_closed"},
            {"symbol", fill.symbol}
        };
        apiServer.broadcast(closedMsg.dump());

        std::cout << "[main] Position " << fill.symbol << " clôturée (qty="
                  << closingQty << ", pnl=" << realizedPnl << ")\n";
    });

    // Divergences détectées à la réconciliation (ou futures alertes op.) :
    // journalisées en base pour être consultables après coup.
    executor->setOnAlert([&](const std::string& level, const std::string& message) {
        database->logAlert(level, message);
    });

    // --- Réconciliation au démarrage : ne JAMAIS repartir sur un état mémoire
    // vide en silence si des positions étaient ouvertes avant un crash. ---
    executor->reconcileFromDatabase(database->loadOpenPositions());

    // Un historique séparé par symbole : mélanger les prix BTC et ETH dans
    // le même buffer fausserait complètement la moyenne mobile de chacun.
    std::unordered_map<std::string, std::vector<Tick>> historyBySymbol;
    binanceConnector->onTick([&](const Tick& tick) {
        auto& history = historyBySymbol[tick.symbol];
        history.push_back(tick);
        if (history.size() > 500) history.erase(history.begin());

        // Diffuse le prix en direct au dashboard (graphique temps réel).
        nlohmann::json tickMsg = {
            {"type", "tick"},
            {"symbol", tick.symbol},
            {"price", tick.last},
            {"bid", tick.bid},
            {"ask", tick.ask},
            {"volume", tick.volume},
            {"ts", std::chrono::duration_cast<std::chrono::milliseconds>(
                       tick.timestamp.time_since_epoch()).count()}
        };
        apiServer.broadcast(tickMsg.dump());
        database->insertTick(tick);

        // Recalcule le PnL non réalisé de la position ouverte sur ce symbole
        // (si elle existe) et notifie le dashboard via onPositionChanged_.
        executor->updateUnrealizedPnl(tick.symbol, tick.last);

        auto signal = strategy->onTick(tick, history);
        if (signal) {
            std::cout << "[Strategy] Signal " << (signal->side == OrderSide::Buy ? "ACHAT" : "VENTE")
                      << " sur " << signal->symbol << " (confiance=" << signal->confidence
                      << ", stratégie=" << signal->strategyName << ")\n";

            auto order = riskManager->evaluate(*signal, tick.last);
            if (order) {
                std::cout << "[RiskManager] Ordre validé : "
                          << (order->side == OrderSide::Buy ? "ACHAT" : "VENTE")
                          << " qty=" << order->quantity << " @ ~" << tick.last
                          << " (SL=" << order->stopLoss << ", TP=" << order->takeProfit << ")\n";
                executor->execute(*order);
            } else {
                std::cout << "[RiskManager] Signal rejeté (bot en pause, limite de drawdown "
                             "quotidien ou exposition max déjà atteinte sur ce marché)\n";
            }
        }
    });

    binanceConnector->connect();

    // Liste de symboles crypto configurable (config.json: "crypto_symbols").
    // À défaut, BTCUSDT seul pour rester compatible avec une config existante.
    auto cryptoSymbols = config.value("crypto_symbols", nlohmann::json::array({"BTCUSDT"}));
    for (const auto& s : cryptoSymbols) {
        binanceConnector->subscribe(s.get<std::string>());
    }

    apiServer.run(); // bloquant

    binanceConnector->disconnect();
    return 0;
}
