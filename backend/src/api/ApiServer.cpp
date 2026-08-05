#include "ApiServer.hpp"
#include "Jwt.hpp"
#include <crow.h>
#include <iostream>

namespace tradingbot {

ApiServer::ApiServer(std::shared_ptr<RiskManager> riskManager,
                      std::shared_ptr<Database> database,
                      AuthConfig authConfig,
                      int port)
    : riskManager_(std::move(riskManager)),
      database_(std::move(database)),
      authConfig_(std::move(authConfig)),
      port_(port) {}

void ApiServer::broadcast(const std::string& jsonMessage) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto* client : clients_) {
        client->send_text(jsonMessage);
    }
}

bool ApiServer::checkAuth(const crow::request& req) const {
    std::string authHeader = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (authHeader.rfind(prefix, 0) != 0) return false;
    std::string token = authHeader.substr(prefix.size());
    return Jwt::verify(token, authConfig_.jwtSecret).has_value();
}

void ApiServer::run() {
    crow::SimpleApp app;

    // --- Auth ---
    // Seule route publique : échange identifiants contre un JWT (24h).
    // Le mot de passe n'est jamais comparé en clair, seulement son hash.
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "{\"error\":\"corps JSON invalide\"}");

        std::string username = body["username"].s();
        std::string password = body["password"].s();
        std::string hashed = Jwt::sha256Hex(password);

        if (username != authConfig_.username || hashed != authConfig_.passwordSha256) {
            return crow::response(401, "{\"error\":\"identifiants invalides\"}");
        }

        std::string token = Jwt::sign({{"sub", username}}, authConfig_.jwtSecret, 24 * 3600);
        crow::json::wvalue res;
        res["token"] = token;
        return crow::response(200, res);
    });

    // --- REST (protégé JWT) ---
    CROW_ROUTE(app, "/api/status")
    ([this](const crow::request& req) {
        if (!checkAuth(req)) return crow::response(401, "{\"error\":\"non autorisé\"}");
        crow::json::wvalue res;
        res["equity"] = riskManager_->currentEquity();
        res["drawdown_pct"] = riskManager_->currentDrawdownPct();
        res["halted"] = riskManager_->isHalted();
        return crow::response(200, res);
    });

    // Arrêt d'urgence déclenché depuis le dashboard.
    CROW_ROUTE(app, "/api/halt").methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req) {
        if (!checkAuth(req)) return crow::response(401, "{\"error\":\"non autorisé\"}");
        riskManager_->haltTrading("Arrêt manuel depuis le dashboard");
        return crow::response(200, "{\"status\":\"halted\"}");
    });

    CROW_ROUTE(app, "/api/resume").methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req) {
        if (!checkAuth(req)) return crow::response(401, "{\"error\":\"non autorisé\"}");
        riskManager_->resumeTrading();
        return crow::response(200, "{\"status\":\"resumed\"}");
    });

    // Historique d'équité pour le graphique du dashboard.
    CROW_ROUTE(app, "/api/equity_history")
    ([this](const crow::request& req) {
        if (!checkAuth(req)) return crow::response(401, "{\"error\":\"non autorisé\"}");
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
        return crow::response(200, res);
    });

    // Derniers trades pour le tableau du dashboard.
    CROW_ROUTE(app, "/api/trades")
    ([this](const crow::request& req) {
        if (!checkAuth(req)) return crow::response(401, "{\"error\":\"non autorisé\"}");
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
        return crow::response(200, res);
    });

    // --- WebSocket (flux temps réel vers le dashboard) ---
    // Le WebSocket ne porte pas d'en-tête Authorization après le handshake,
    // donc le token est passé en query string (?token=...) et vérifié dans
    // onaccept, avant même d'accepter la mise à niveau de la connexion.
    CROW_ROUTE(app, "/ws")
        .websocket(&app)
        .onaccept([this](const crow::request& req, void**) {
            std::string token = req.url_params.get("token") ? req.url_params.get("token") : "";
            if (!Jwt::verify(token, authConfig_.jwtSecret).has_value()) {
                std::cout << "[ApiServer] Connexion WebSocket refusée (token invalide/absent)\n";
                return false;
            }
            return true;
        })
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
