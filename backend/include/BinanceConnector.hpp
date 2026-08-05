#pragma once
#include "IMarketConnector.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <set>
#include <mutex>

namespace tradingbot {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

// Connecteur crypto réel (Binance) : flux de prix en WebSocket (stream combiné,
// wss://stream.binance.com:9443/stream) et passage d'ordres en REST signé HMAC-SHA256.
// Reconnexion automatique avec backoff exponentiel en cas de coupure.
class BinanceConnector : public IMarketConnector {
public:
    BinanceConnector(std::string apiKey, std::string apiSecret, bool testnet = true);
    ~BinanceConnector() override;

    void connect() override;
    void disconnect() override;
    bool isConnected() const override { return connected_; }

    void subscribe(const std::string& symbol) override;
    void onTick(TickCallback cb) override { tickCallback_ = std::move(cb); }
    void onOrderUpdate(OrderUpdateCallback cb) override { orderCallback_ = std::move(cb); }

    std::string placeOrder(const Order& order) override;
    void cancelOrder(const std::string& orderId) override;
    double getAssetBalance(const std::string& asset) override;

    MarketType marketType() const override { return MarketType::Crypto; }
    std::string exchangeName() const override { return "binance"; }

private:
    void runIoLoop();
    void doConnect();
    void doRead();
    void handleMessage(const std::string& payload);
    void sendSubscribe(const std::string& streamName);
    void scheduleReconnect();
    std::string sign(const std::string& queryString) const;

    std::string apiKey_;
    std::string apiSecret_;
    bool testnet_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> shouldRun_{false};
    int reconnectDelayMs_{1000};

    net::io_context ioContext_;
    ssl::context sslContext_{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> ws_;
    beast::flat_buffer buffer_;
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> workGuard_;
    std::unique_ptr<std::thread> ioThread_;

    std::mutex subsMutex_;
    std::set<std::string> pendingSubscriptions_; // en attente de connexion
    int subscribeMsgId_{1};

    TickCallback tickCallback_;
    OrderUpdateCallback orderCallback_;
};

} // namespace tradingbot

