#pragma once
#include "Types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace pqxx { class connection; }

namespace tradingbot {

struct EquityPoint {
    std::string timestamp;
    double equity{};
    double drawdownPct{};
};

struct TradeRecord {
    std::string symbol;
    std::string side;
    double quantity{};
    double fillPrice{};
    double pnl{};
    std::string timestamp;
};

class Database {
public:
    explicit Database(const std::string& connectionString);
    ~Database();

    void insertTrade(const Order& order, double fillPrice, double pnl);
    void insertTick(const Tick& tick);
    void updateEquitySnapshot(double equity, double drawdownPct);
    void logAlert(const std::string& level, const std::string& message);

    // Écrites à chaque changement de position (OrderExecutor::onPositionChanged_)
    // pour que l'état survive à un redémarrage/crash du process.
    void upsertOpenPosition(const Position& pos);
    void closePosition(const std::string& symbol);

    // Relit les positions encore marquées 'open' en base — utilisé au
    // démarrage par OrderExecutor::reconcileFromDatabase pour reconstruire
    // l'état mémoire sans dépendre de ce qui existait avant le crash.
    std::vector<Position> loadOpenPositions();

    // Utilisés par l'API pour alimenter le dashboard (Streamlit ou autre).
    std::vector<EquityPoint> getEquityHistory(int limitPoints = 500);
    std::vector<TradeRecord> getRecentTrades(int limit = 100);

private:
    std::unique_ptr<pqxx::connection> conn_;
};

} // namespace tradingbot

