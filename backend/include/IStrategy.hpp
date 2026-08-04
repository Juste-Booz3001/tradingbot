#pragma once
#include "Types.hpp"
#include <optional>
#include <vector>

namespace tradingbot {

// Toute stratégie brancheable sur le moteur implémente cette interface.
// Elle ne prend AUCUNE décision de sizing ou de risque : elle émet un signal,
// et c'est le RiskManager qui décide s'il est exécuté, et à quelle taille.
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // Appelé à chaque nouveau tick de marché. Retourne un signal si les
    // conditions de la stratégie sont réunies, sinon std::nullopt.
    virtual std::optional<Signal> onTick(const Tick& tick,
                                          const std::vector<Tick>& recentHistory) = 0;

    virtual std::string name() const = 0;
};

// Exemple minimal : croisement de moyennes mobiles (à remplacer par votre logique).
class MovingAverageCrossStrategy : public IStrategy {
public:
    MovingAverageCrossStrategy(int fastPeriod, int slowPeriod)
        : fastPeriod_(fastPeriod), slowPeriod_(slowPeriod) {}

    std::optional<Signal> onTick(const Tick& tick,
                                  const std::vector<Tick>& history) override {
        if (static_cast<int>(history.size()) < slowPeriod_) return std::nullopt;

        double fastSum = 0, slowSum = 0;
        for (int i = 0; i < fastPeriod_; ++i)
            fastSum += history[history.size() - 1 - i].last;
        for (int i = 0; i < slowPeriod_; ++i)
            slowSum += history[history.size() - 1 - i].last;

        double fastMa = fastSum / fastPeriod_;
        double slowMa = slowSum / slowPeriod_;

        // Signal simple : croisement haussier / baissier.
        // /!\ Squelette pédagogique — à backtester avant tout usage réel.
        if (fastMa > slowMa * 1.001) {
            return Signal{tick.symbol, tick.market, OrderSide::Buy, 0.5, name()};
        }
        if (fastMa < slowMa * 0.999) {
            return Signal{tick.symbol, tick.market, OrderSide::Sell, 0.5, name()};
        }
        return std::nullopt;
    }

    std::string name() const override { return "MA_Cross_" + std::to_string(fastPeriod_) + "_" + std::to_string(slowPeriod_); }

private:
    int fastPeriod_;
    int slowPeriod_;
};

} // namespace tradingbot
