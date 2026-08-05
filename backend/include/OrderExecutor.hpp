#pragma once
#include "Types.hpp"
#include "IMarketConnector.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot {

// Exécute les ordres validés par le RiskManager, gère les retries et
// journalise chaque étape (persisté avant l'envoi — write-ahead — pour
// permettre une réconciliation fiable après un crash).
//
// Tient aussi l'état des positions ouvertes : un fill qui ouvre ou augmente
// une position met à jour la position ; un fill de sens opposé la réduit ou
// la clôture et déclenche le callback onPositionClosed_ (pour que le
// RiskManager enregistre le PnL réalisé).
class OrderExecutor {
public:
    using PositionChangedCallback = std::function<void(const Position&)>;
    // fill: l'ordre (sens opposé) qui a déclenché la clôture/réduction ;
    // closingQty : quantité effectivement clôturée par ce fill (peut être
    // inférieure à fill.quantity en cas de retournement de position) ;
    // realizedPnl : PnL réalisé sur cette portion.
    using PositionClosedCallback =
        std::function<void(const Order& fill, double closingQty, double realizedPnl)>;
    // Émis quand la réconciliation détecte une divergence DB/exchange, ou
    // pour toute autre alerte opérationnelle future.
    using AlertCallback = std::function<void(const std::string& level, const std::string& message)>;

    explicit OrderExecutor(std::shared_ptr<IMarketConnector> connector);

    // Envoie l'ordre, avec retry (backoff exponentiel, 3 tentatives par défaut).
    std::string execute(const Order& order);

    // Appelé par le connecteur (directement, ou via callback asynchrone selon
    // l'exchange) dès qu'un ordre est exécuté (total ou partiel).
    void onOrderUpdate(const Order& update);

    // À appeler à chaque tick pour recalculer le PnL non réalisé de la
    // position ouverte sur ce symbole, et notifier le dashboard.
    void updateUnrealizedPnl(const std::string& symbol, double currentPrice);

    std::optional<Position> getPosition(const std::string& symbol) const;

    // Notifié à chaque ouverture/mise à jour de position (nouveau fill, ou
    // simple recalcul de PnL non réalisé sur tick).
    void setOnPositionChanged(PositionChangedCallback cb) { onPositionChanged_ = std::move(cb); }
    // Notifié quand une position se ferme complètement, avec le PnL réalisé.
    void setOnPositionClosed(PositionClosedCallback cb) { onPositionClosed_ = std::move(cb); }
    void setOnAlert(AlertCallback cb) { onAlert_ = std::move(cb); }

    // Reconstruit l'état des positions à partir des lignes 'open' de la DB
    // (passées en paramètre — voir Database::loadOpenPositions), puis compare
    // chaque quantité au solde réel côté exchange via connector_->getAssetBalance.
    // Une divergence au-delà de la tolérance déclenche onAlert_ (niveau
    // "critical") mais ne modifie PAS automatiquement la position : à vérifier
    // manuellement avant de reprendre le trading, comme le rappelle le README.
    void reconcileFromDatabase(const std::vector<Position>& dbPositions);

private:
    void applyFill(const Order& update);

    std::shared_ptr<IMarketConnector> connector_;
    std::unordered_map<std::string, Order> pendingOrders_;
    int maxRetries_{3};

    mutable std::mutex positionsMutex_;
    std::unordered_map<std::string, Position> openPositions_; // clé = symbol

    PositionChangedCallback onPositionChanged_;
    PositionClosedCallback onPositionClosed_;
    AlertCallback onAlert_;
};

} // namespace tradingbot
