#include "Database.hpp"

// La libpqxx packagée par Ubuntu (noble, 7.8.1) est compilée sans le support
// std::source_location, mais ses en-têtes le détectent et l'activent dès que
// le code consommateur compile en C++20 — ce qui casse le linkage (symboles
// de pqxx::conversion_error/conversion_overrun introuvables dans le .so).
// On force la détection à "non disponible" avant d'inclure pqxx pour rester
// compatible avec le binaire système. À retirer si la distro corrige son
// paquet, ou si vous compilez pqxx vous-même depuis les sources.
#include <version>
#ifdef __cpp_lib_source_location
#undef __cpp_lib_source_location
#endif

#include <pqxx/pqxx>
#include <iostream>
#include <algorithm>

// exec_params() reste l'API la plus largement compatible entre les
// différents packagings de libpqxx 7.8.x (certains, comme celui de Kali,
// la marquent dépréciée un peu plus tôt que d'autres au profit de
// exec(query, pqxx::params{...}), pas encore disponible partout). On
// silence donc ce warning précis plutôt que de risquer une API absente
// ailleurs — à revoir une fois libpqxx 7.9+ généralisé.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

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

void Database::upsertOpenPosition(const Position& pos) {
    try {
        pqxx::work txn(*conn_);
        // Une seule position ouverte par symbole à la fois (cohérent avec
        // OrderExecutor::openPositions_, qui est aussi indexé par symbole).
        auto existing = txn.exec_params(
            "SELECT id FROM positions WHERE symbol = $1 AND status = 'open' LIMIT 1",
            pos.symbol);

        if (existing.empty()) {
            txn.exec_params(
                "INSERT INTO positions (symbol, market, side, quantity, entry_price, "
                "stop_loss, take_profit, status, opened_at) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, 'open', now())",
                pos.symbol, static_cast<int>(pos.market),
                pos.side == OrderSide::Buy ? "buy" : "sell",
                pos.quantity, pos.entryPrice, pos.stopLoss, pos.takeProfit);
        } else {
            txn.exec_params(
                "UPDATE positions SET side = $2, quantity = $3, entry_price = $4, "
                "stop_loss = $5, take_profit = $6 WHERE id = $1",
                existing[0]["id"].as<long long>(),
                pos.side == OrderSide::Buy ? "buy" : "sell",
                pos.quantity, pos.entryPrice, pos.stopLoss, pos.takeProfit);
        }
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[Database] upsertOpenPosition échoué : " << e.what() << "\n";
    }
}

void Database::closePosition(const std::string& symbol) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE positions SET status = 'closed', closed_at = now() "
            "WHERE symbol = $1 AND status = 'open'",
            symbol);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[Database] closePosition échoué : " << e.what() << "\n";
    }
}

std::vector<Position> Database::loadOpenPositions() {
    std::vector<Position> result;
    try {
        pqxx::work txn(*conn_);
        auto rows = txn.exec(
            "SELECT symbol, market, side, quantity, entry_price, stop_loss, take_profit "
            "FROM positions WHERE status = 'open'");
        for (const auto& row : rows) {
            Position pos;
            pos.symbol = row["symbol"].c_str();
            pos.market = static_cast<MarketType>(row["market"].as<int>());
            pos.side = row["side"].c_str() == std::string("buy") ? OrderSide::Buy : OrderSide::Sell;
            pos.quantity = row["quantity"].as<double>();
            pos.entryPrice = row["entry_price"].as<double>();
            pos.stopLoss = row["stop_loss"].is_null() ? 0.0 : row["stop_loss"].as<double>();
            pos.takeProfit = row["take_profit"].is_null() ? 0.0 : row["take_profit"].as<double>();
            pos.unrealizedPnl = 0.0; // recalculé au premier tick suivant via updateUnrealizedPnl
            result.push_back(pos);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] loadOpenPositions échoué : " << e.what() << "\n";
    }
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
