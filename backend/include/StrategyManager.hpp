#pragma once
#include "IStrategy.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

namespace tradingbot {

// Fait tourner plusieurs stratégies en parallèle, chacune activable/
// désactivable indépendamment (via l'API REST, depuis le dashboard). Une
// stratégie désactivée n'est simplement pas interrogée sur les ticks —
// son état interne (MA, RSI...) se fige jusqu'à sa réactivation.
class StrategyManager {
public:
    struct Info {
        std::string name;
        std::string description;
        bool enabled;
    };

    void addStrategy(std::unique_ptr<IStrategy> strategy, std::string description, bool enabledByDefault = true);

    // Évalue toutes les stratégies ACTIVÉES pour ce tick. Retourne tous les
    // signaux déclenchés (normalement 0 ou 1, mais rien n'empêche deux
    // stratégies actives de signaler sur le même tick).
    std::vector<Signal> onTick(const Tick& tick, const std::vector<Tick>& history);

    std::vector<Info> list() const;

    // Retourne false si aucune stratégie ne porte ce nom.
    bool setEnabled(const std::string& name, bool enabled);

private:
    struct Entry {
        std::string name;
        std::string description;
        std::unique_ptr<IStrategy> strategy;
        std::atomic<bool> enabled;
    };

    mutable std::mutex mutex_;
    // unique_ptr<Entry> : std::atomic<bool> n'est ni copiable ni déplaçable,
    // donc les Entry doivent vivre à une adresse fixe sur le tas plutôt que
    // directement dans le vector (qui pourrait les réallouer/déplacer).
    std::vector<std::unique_ptr<Entry>> entries_;
};

} // namespace tradingbot
