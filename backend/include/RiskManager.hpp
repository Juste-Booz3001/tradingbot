#pragma once
#include "Types.hpp"
#include <optional>
#include <unordered_map>
#include <mutex>

namespace tradingbot {

// Module central de sécurité. AUCUN ordre ne part sans passer par ici.
// Convertit un Signal (intention de trade) en Order (taille, stop, take-profit)
// concrets — ou refuse le trade si un garde-fou est déclenché.
class RiskManager {
public:
    explicit RiskManager(double startingCapital);

    void setProfile(MarketType market, const MarketRiskProfile& profile);

    // Retourne un ordre prêt à exécuter, ou std::nullopt si le trade est refusé.
    std::optional<Order> evaluate(const Signal& signal, double currentPrice);

    // À appeler après chaque clôture de position pour mettre à jour l'équité
    // et le suivi de drawdown.
    void recordTradeResult(MarketType market, double pnl);

    // Kill switch global — ferme toute nouvelle prise de position.
    void haltTrading(const std::string& reason);
    void resumeTrading();
    bool isHalted() const { return halted_; }

    double currentEquity() const { return equity_; }
    double currentDrawdownPct() const;

private:
    bool dailyLimitBreached(MarketType market) const;

    mutable std::mutex mutex_;
    double startingCapital_;
    double equity_;
    double peakEquity_;
    bool halted_{false};
    std::string haltReason_;

    std::unordered_map<int, MarketRiskProfile> profiles_; // clé = static_cast<int>(MarketType)
    std::unordered_map<int, double> dailyPnlByMarket_;
    std::unordered_map<int, double> exposureByMarket_;
};

} // namespace tradingbot
