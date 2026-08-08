#pragma once
#include "RiskManager.hpp"
#include "Database.hpp"
#include "StrategyManager.hpp"
#include <memory>
#include <set>
#include <mutex>
#include <string>

namespace crow { namespace websocket { struct connection; } class request; }

namespace tradingbot {

// Identifiants admin + secret de signature JWT, lus depuis config.json
// (bloc "auth"). Le mot de passe n'est jamais stocké en clair : seul son
// hash SHA-256 l'est, comparé à chaque tentative de connexion.
struct AuthConfig {
    std::string username;
    std::string passwordSha256;
    std::string jwtSecret;
};

// Sert le dashboard : REST pour la config/lecture, WebSocket pour le flux
// temps réel (prix, positions, équité) poussé vers le frontend React.
// Toutes les routes sauf /api/login exigent un JWT valide (Authorization:
// Bearer <token>, ou ?token=<token> pour le WebSocket qui ne porte pas
// d'en-têtes HTTP après le handshake).
class ApiServer {
public:
    ApiServer(std::shared_ptr<RiskManager> riskManager,
              std::shared_ptr<Database> database,
              std::shared_ptr<StrategyManager> strategyManager,
              AuthConfig authConfig,
              bool paperTrading,
              int port = 8080);
    void run();

    // Diffuse un message JSON à tous les clients WebSocket connectés
    // (appelé par le moteur à chaque tick / mise à jour de position).
    void broadcast(const std::string& jsonMessage);

private:
    bool checkAuth(const crow::request& req) const;

    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<Database> database_;
    std::shared_ptr<StrategyManager> strategyManager_;
    AuthConfig authConfig_;
    bool paperTrading_;
    int port_;
    std::mutex clientsMutex_;
    std::set<crow::websocket::connection*> clients_;
};

} // namespace tradingbot
