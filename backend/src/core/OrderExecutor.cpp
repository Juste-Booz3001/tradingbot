#include "OrderExecutor.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace tradingbot {

OrderExecutor::OrderExecutor(std::shared_ptr<IMarketConnector> connector)
    : connector_(std::move(connector)) {}

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
}

void OrderExecutor::reconcileFromDatabase() {
    // TODO: au démarrage, relire les positions ouvertes en base et les
    // comparer à l'état réel côté exchange (via connector_) pour détecter
    // toute divergence causée par un crash précédent.
    std::cout << "[OrderExecutor] Réconciliation d'état au démarrage (squelette)\n";
}

} // namespace tradingbot
