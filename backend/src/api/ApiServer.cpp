#include "ApiServer.hpp"
#include <crow.h>
#include <iostream>

namespace tradingbot {

ApiServer::ApiServer(std::shared_ptr<RiskManager> riskManager,
                      std::shared_ptr<Database> database,
                      int port)
    : riskManager_(std::move(riskManager)), database_(std::move(database)), port_(port) {}

void ApiServer::broadcast(const std::string& jsonMessage) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto* client : clients_) {
        client->send_text(jsonMessage);
    }
}

void ApiServer::run() {
    crow::SimpleApp app;

    // --- REST ---
    CROW_ROUTE(app, "/api/status")
    ([this]() {
        crow::json::wvalue res;
        res["equity"] = riskManager_->currentEquity();
        res["drawdown_pct"] = riskManager_->currentDrawdownPct();
        res["halted"] = riskManager_->isHalted();
        return res;
    });

    // Arrêt d'urgence déclenché depuis le dashboard.
    CROW_ROUTE(app, "/api/halt").methods(crow::HTTPMethod::Post)
    ([this]() {
        riskManager_->haltTrading("Arrêt manuel depuis le dashboard");
        return crow::response(200, "{\"status\":\"halted\"}");
    });

    CROW_ROUTE(app, "/api/resume").methods(crow::HTTPMethod::Post)
    ([this]() {
        riskManager_->resumeTrading();
        return crow::response(200, "{\"status\":\"resumed\"}");
    });

    // Historique d'équité pour le graphique du dashboard.
    CROW_ROUTE(app, "/api/equity_history")
    ([this]() {
        auto history = database_->getEquityHistory(500);
        crow::json::wvalue::list points;
        for (const auto& p : history) {
            crow::json::wvalue point;
            point["ts"] = p.timestamp;
            point["equity"] = p.equity;
            point["drawdown_pct"] = p.drawdownPct;
            points.push_back(std::move(point));
        }
        crow::json::wvalue res;
        res["points"] = std::move(points);
        return res;
    });

    // Derniers trades pour le tableau du dashboard.
    CROW_ROUTE(app, "/api/trades")
    ([this]() {
        auto trades = database_->getRecentTrades(100);
        crow::json::wvalue::list rows;
        for (const auto& t : trades) {
            crow::json::wvalue row;
            row["symbol"] = t.symbol;
            row["side"] = t.side;
            row["quantity"] = t.quantity;
            row["fill_price"] = t.fillPrice;
            row["pnl"] = t.pnl;
            row["ts"] = t.timestamp;
            rows.push_back(std::move(row));
        }
        crow::json::wvalue res;
        res["trades"] = std::move(rows);
        return res;
    });

    // --- WebSocket (flux temps réel vers le dashboard) ---
    CROW_ROUTE(app, "/ws")
        .websocket(&app)
        .onopen([this](crow::websocket::connection& conn) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.insert(&conn);
            std::cout << "[ApiServer] Client dashboard connecté (" << clients_.size() << " total)\n";
        })
        .onclose([this](crow::websocket::connection& conn, const std::string&, uint16_t) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.erase(&conn);
        });

    std::cout << "[ApiServer] Démarrage sur http://localhost:" << port_
              << " (WebSocket sur /ws)\n";
    app.port(port_).multithreaded().run();
}

} // namespace tradingbot
