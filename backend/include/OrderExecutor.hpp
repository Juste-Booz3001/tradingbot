#pragma once
#include "Types.hpp"
#include "IMarketConnector.hpp"
#include <memory>
#include <unordered_map>
#include <string>

namespace tradingbot {

// Exécute les ordres validés par le RiskManager, gère les retries et
// journalise chaque étape (persisté avant l'envoi — write-ahead — pour
// permettre une réconciliation fiable après un crash).
class OrderExecutor {
public:
    explicit OrderExecutor(std::shared_ptr<IMarketConnector> connector);

    // Envoie l'ordre, avec retry (backoff exponentiel, 3 tentatives par défaut).
    std::string execute(const Order& order);

    void onOrderUpdate(const Order& update);

    // Reconstruit l'état des positions à partir de la DB au démarrage,
    // pour ne pas dépendre d'un état mémoire perdu lors d'un crash.
    void reconcileFromDatabase();

private:
    std::shared_ptr<IMarketConnector> connector_;
    std::unordered_map<std::string, Order> pendingOrders_;
    int maxRetries_{3};
};

} // namespace tradingbot
