#include "Types.hpp"
#include "IStrategy.hpp"
#include "RiskManager.hpp"
#include "OrderExecutor.hpp"
#include "BinanceConnector.hpp"
#include "ApiServer.hpp"
#include "Database.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <memory>

// Nécessite nlohmann/json (https://github.com/nlohmann/json) — header-only,
// à placer dans backend/include/nlohmann/json.hpp ou installer via le
// gestionnaire de paquets système (libnlohmann-json3-dev sur Ubuntu).
#include <nlohmann/json.hpp>

using namespace tradingbot;

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

    // --- API web (créée tôt aussi : run() est appelé bien plus bas, mais
    // l'objet doit déjà exister pour que le callback de ticks puisse
    // diffuser aux clients WebSocket dès qu'un prix arrive) ---
    int apiPort = config.value("api_port", 8080);
    ApiServer apiServer(riskManager, database, apiPort);

    // --- Connecteur crypto ---
    auto binanceConnector = std::make_shared<BinanceConnector>(
        config.value("binance_api_key", ""),
        config.value("binance_api_secret", ""),
        /*testnet=*/ paperTrading
    );

    auto strategy = std::make_unique<MovingAverageCrossStrategy>(9, 21);
    auto executor = std::make_shared<OrderExecutor>(binanceConnector);
    executor->reconcileFromDatabase();

    std::vector<Tick> history;
    binanceConnector->onTick([&](const Tick& tick) {
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

        auto signal = strategy->onTick(tick, history);
        if (signal) {
            auto order = riskManager->evaluate(*signal, tick.last);
            if (order) {
                executor->execute(*order);
            }
        }
    });

    binanceConnector->connect();
    binanceConnector->subscribe("BTCUSDT");

    apiServer.run(); // bloquant

    binanceConnector->disconnect();
    return 0;
}
