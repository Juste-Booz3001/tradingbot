#pragma once
#include "RiskManager.hpp"
#include "Database.hpp"
#include <memory>
#include <set>
#include <mutex>

namespace crow { namespace websocket { struct connection; } }

namespace tradingbot {

// Sert le dashboard : REST pour la config/lecture, WebSocket pour le flux
// temps réel (prix, positions, équité) poussé vers le frontend React.
class ApiServer {
public:
    ApiServer(std::shared_ptr<RiskManager> riskManager,
              std::shared_ptr<Database> database,
              int port = 8080);
    void run();

    // Diffuse un message JSON à tous les clients WebSocket connectés
    // (appelé par le moteur à chaque tick / mise à jour de position).
    void broadcast(const std::string& jsonMessage);

private:
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<Database> database_;
    int port_;
    std::mutex clientsMutex_;
    std::set<crow::websocket::connection*> clients_;
};

} // namespace tradingbot
