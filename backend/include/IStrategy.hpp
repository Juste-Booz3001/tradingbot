#pragma once
#include "Types.hpp"
#include <optional>
#include <vector>
#include <unordered_map>
#include <utility>
#include <string>
#include <algorithm>

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
        if (static_cast<int>(history.size()) < slowPeriod_) {
            lastMaBySymbol_.erase(tick.symbol);
            return std::nullopt;
        }

        double fastSum = 0, slowSum = 0;
        for (int i = 0; i < fastPeriod_; ++i)
            fastSum += history[history.size() - 1 - i].last;
        for (int i = 0; i < slowPeriod_; ++i)
            slowSum += history[history.size() - 1 - i].last;

        double fastMa = fastSum / fastPeriod_;
        double slowMa = slowSum / slowPeriod_;
        // Une seule instance de stratégie est partagée entre tous les symboles
        // (voir main.cpp) : indexer par symbole évite de mélanger les MA du
        // BTC avec celles de l'ETH quand plusieurs paires tournent en parallèle.
        lastMaBySymbol_[tick.symbol] = {fastMa, slowMa};

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

    // Dernières moyennes mobiles calculées (fast, slow) pour CE symbole — pour
    // affichage/diffusion au dashboard. std::nullopt tant que l'historique est
    // insuffisant (< slowPeriod_ ticks) pour ce symbole précis.
    std::optional<std::pair<double, double>> lastMovingAverages(const std::string& symbol) const {
        auto it = lastMaBySymbol_.find(symbol);
        if (it == lastMaBySymbol_.end()) return std::nullopt;
        return it->second;
    }

    int fastPeriod() const { return fastPeriod_; }
    int slowPeriod() const { return slowPeriod_; }

private:
    int fastPeriod_;
    int slowPeriod_;
    std::unordered_map<std::string, std::pair<double, double>> lastMaBySymbol_;
};

// Stratégie de retournement (mean-reversion) basée sur le RSI (Relative
// Strength Index, méthode de Wilder). Contrairement au croisement de
// moyennes mobiles (qui suit les tendances et reste silencieux en marché
// qui range), le RSI détecte les extrêmes de sur-achat/survente et peut
// déclencher des signaux même quand le prix oscille dans un couloir serré —
// les deux stratégies sont donc complémentaires plutôt que redondantes.
// /!\ Squelette pédagogique — à backtester avant tout usage réel.
class RsiMeanReversionStrategy : public IStrategy {
public:
    explicit RsiMeanReversionStrategy(int period = 14, double oversold = 30.0, double overbought = 70.0)
        : period_(period), oversold_(oversold), overbought_(overbought) {}

    std::optional<Signal> onTick(const Tick& tick,
                                  const std::vector<Tick>& history) override {
        if (static_cast<int>(history.size()) < period_ + 1) {
            lastRsiBySymbol_.erase(tick.symbol);
            return std::nullopt;
        }

        double gainSum = 0.0, lossSum = 0.0;
        int start = static_cast<int>(history.size()) - period_;
        for (int i = start; i < static_cast<int>(history.size()); ++i) {
            double diff = history[i].last - history[i - 1].last;
            if (diff > 0) gainSum += diff;
            else lossSum -= diff;
        }
        double avgGain = gainSum / period_;
        double avgLoss = lossSum / period_;
        double rsi = (avgLoss == 0.0) ? 100.0 : 100.0 - (100.0 / (1.0 + avgGain / avgLoss));
        lastRsiBySymbol_[tick.symbol] = rsi;

        if (rsi < oversold_) {
            // Plus le RSI est enfoncé sous le seuil de survente, plus la
            // confiance envoyée au Risk Manager (donc la taille de position) est élevée.
            double confidence = std::min(1.0, 0.3 + (oversold_ - rsi) / oversold_);
            return Signal{tick.symbol, tick.market, OrderSide::Buy, confidence, name()};
        }
        if (rsi > overbought_) {
            double confidence = std::min(1.0, 0.3 + (rsi - overbought_) / (100.0 - overbought_));
            return Signal{tick.symbol, tick.market, OrderSide::Sell, confidence, name()};
        }
        return std::nullopt;
    }

    std::string name() const override { return "RSI_" + std::to_string(period_); }

    // Dernier RSI calculé pour ce symbole (0-100) — pour affichage/diffusion.
    std::optional<double> lastRsi(const std::string& symbol) const {
        auto it = lastRsiBySymbol_.find(symbol);
        if (it == lastRsiBySymbol_.end()) return std::nullopt;
        return it->second;
    }

    double oversoldThreshold() const { return oversold_; }
    double overboughtThreshold() const { return overbought_; }

private:
    int period_;
    double oversold_;
    double overbought_;
    std::unordered_map<std::string, double> lastRsiBySymbol_;
};

} // namespace tradingbot
