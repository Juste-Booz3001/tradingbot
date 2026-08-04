#include "Database.hpp"
#include <pqxx/pqxx>
#include <iostream>
#include <algorithm>

namespace tradingbot {

Database::Database(const std::string& connectionString) {
    conn_ = std::make_unique<pqxx::connection>(connectionString);
    if (!conn_->is_open()) {
        throw std::runtime_error("Impossible de se connecter à la base de données");
    }
    std::cout << "[Database] Connecté : " << conn_->dbname() << "\n";
}

Database::~Database() = default;

void Database::insertTrade(const Order& order, double fillPrice, double pnl) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO trades (order_id, symbol, market, side, quantity, fill_price, pnl, created_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, now())",
            order.id, order.symbol, static_cast<int>(order.market),
            order.side == OrderSide::Buy ? "buy" : "sell",
            order.quantity, fillPrice, pnl);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[Database] insertTrade échoué : " << e.what() << "\n";
    }
}

void Database::insertTick(const Tick& tick) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO market_data (symbol, market, bid, ask, last, volume, ts) "
            "VALUES ($1, $2, $3, $4, $5, $6, now())",
            tick.symbol, static_cast<int>(tick.market),
            tick.bid, tick.ask, tick.last, tick.volume);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[Database] insertTick échoué : " << e.what() << "\n";
    }
}

void Database::updateEquitySnapshot(double equity, double drawdownPct) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO equity_curve (equity, drawdown_pct, ts) VALUES ($1, $2, now())",
            equity, drawdownPct);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[Database] updateEquitySnapshot échoué : " << e.what() << "\n";
    }
}

void Database::logAlert(const std::string& level, const std::string& message) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO alerts_log (level, message, ts) VALUES ($1, $2, now())",
            level, message);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[Database] logAlert échoué : " << e.what() << "\n";
    }
}

std::vector<Order> Database::loadOpenPositions() {
    std::vector<Order> result;
    // TODO: SELECT * FROM positions WHERE status = 'open', mapper vers Order.
    return result;
}

std::vector<EquityPoint> Database::getEquityHistory(int limitPoints) {
    std::vector<EquityPoint> result;
    try {
        pqxx::work txn(*conn_);
        auto rows = txn.exec_params(
            "SELECT ts, equity, drawdown_pct FROM equity_curve "
            "ORDER BY ts DESC LIMIT $1", limitPoints);
        for (const auto& row : rows) {
            result.push_back(EquityPoint{
                row["ts"].c_str(),
                row["equity"].as<double>(),
                row["drawdown_pct"].as<double>()
            });
        }
        std::reverse(result.begin(), result.end()); // ordre chronologique
    } catch (const std::exception& e) {
        std::cerr << "[Database] getEquityHistory échoué : " << e.what() << "\n";
    }
    return result;
}

std::vector<TradeRecord> Database::getRecentTrades(int limit) {
    std::vector<TradeRecord> result;
    try {
        pqxx::work txn(*conn_);
        auto rows = txn.exec_params(
            "SELECT symbol, side, quantity, fill_price, pnl, created_at "
            "FROM trades ORDER BY created_at DESC LIMIT $1", limit);
        for (const auto& row : rows) {
            result.push_back(TradeRecord{
                row["symbol"].c_str(),
                row["side"].c_str(),
                row["quantity"].as<double>(),
                row["fill_price"].as<double>(),
                row["pnl"].as<double>(),
                row["created_at"].c_str()
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] getRecentTrades échoué : " << e.what() << "\n";
    }
    return result;
}

} // namespace tradingbot
