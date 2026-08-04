#pragma once
#include "Types.hpp"
#include <functional>
#include <string>

namespace tradingbot {

// Chaque exchange/broker implémente cette interface. Le moteur ne dépend
// jamais directement de Binance, IBKR, OANDA, etc. — uniquement de ceci.
class IMarketConnector {
public:
    using TickCallback = std::function<void(const Tick&)>;
    using OrderUpdateCallback = std::function<void(const Order&)>;

    virtual ~IMarketConnector() = default;

    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    virtual void subscribe(const std::string& symbol) = 0;
    virtual void onTick(TickCallback cb) = 0;
    virtual void onOrderUpdate(OrderUpdateCallback cb) = 0;

    // Retourne l'id d'ordre exchange, ou lève une exception en cas d'échec.
    virtual std::string placeOrder(const Order& order) = 0;
    virtual void cancelOrder(const std::string& orderId) = 0;

    virtual MarketType marketType() const = 0;
    virtual std::string exchangeName() const = 0;
};

} // namespace tradingbot
