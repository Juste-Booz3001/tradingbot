#pragma once
#include <string>
#include <chrono>
#include <cstdint>

namespace tradingbot {

enum class MarketType { Crypto, Stocks, Forex };
enum class OrderSide { Buy, Sell };
enum class OrderStatus { Pending, Filled, PartiallyFilled, Cancelled, Rejected };

struct Tick {
    std::string symbol;
    MarketType market;
    double bid{};
    double ask{};
    double last{};
    double volume{};
    std::chrono::system_clock::time_point timestamp;
};

struct Signal {
    std::string symbol;
    MarketType market;
    OrderSide side;
    double confidence{};   // 0.0 - 1.0, utilisé par le Risk Manager pour le sizing
    std::string strategyName;
};

struct Order {
    std::string id;
    std::string symbol;
    MarketType market;
    OrderSide side;
    double quantity{};
    double price{};        // 0 = ordre au marché
    double fillPrice{};    // prix réellement exécuté, renseigné par le connecteur
    double stopLoss{};
    double takeProfit{};
    OrderStatus status{OrderStatus::Pending};
    std::chrono::system_clock::time_point createdAt;
};

struct Position {
    std::string symbol;
    MarketType market;
    OrderSide side;
    double quantity{};
    double entryPrice{};
    double stopLoss{};
    double takeProfit{};
    double unrealizedPnl{};
};

// Profil de risque propre à chaque classe d'actif — voir RiskManager.
struct MarketRiskProfile {
    MarketType market;
    double maxRiskPerTradePct{};   // % du capital risqué par trade
    double defaultStopLossPct{};   // distance du stop-loss en % du prix d'entrée
    double maxDailyDrawdownPct{};  // au-delà, le bot arrête ce marché pour la journée
    double maxExposurePct{};       // % max du capital exposé simultanément sur ce marché
};

} // namespace tradingbot
