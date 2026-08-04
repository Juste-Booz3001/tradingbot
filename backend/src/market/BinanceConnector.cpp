#include "BinanceConnector.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace tradingbot {

using json = nlohmann::json;

namespace {
// Convertit "BTCUSDT" -> "btcusdt" (Binance veut les noms de stream en minuscules).
std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Callback libcurl pour accumuler le corps de réponse dans un std::string.
size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
} // namespace

BinanceConnector::BinanceConnector(std::string apiKey, std::string apiSecret, bool testnet)
    : apiKey_(std::move(apiKey)), apiSecret_(std::move(apiSecret)), testnet_(testnet) {
    sslContext_.set_default_verify_paths();
    sslContext_.set_verify_mode(ssl::verify_peer);
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

BinanceConnector::~BinanceConnector() {
    disconnect();
    curl_global_cleanup();
}

void BinanceConnector::connect() {
    shouldRun_ = true;
    workGuard_ = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(
        ioContext_.get_executor());
    ioThread_ = std::make_unique<std::thread>([this] { runIoLoop(); });
    doConnect();
}

void BinanceConnector::disconnect() {
    shouldRun_ = false;
    connected_ = false;
    if (ws_) {
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
    }
    workGuard_.reset();
    ioContext_.stop();
    if (ioThread_ && ioThread_->joinable()) ioThread_->join();
}

void BinanceConnector::runIoLoop() {
    while (shouldRun_) {
        try {
            ioContext_.run();
        } catch (const std::exception& e) {
            std::cerr << "[BinanceConnector] Exception dans la boucle io: " << e.what() << "\n";
        }
        if (shouldRun_) {
            // io_context s'est arrêté (déconnexion) mais on doit continuer à tourner.
            ioContext_.restart();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void BinanceConnector::doConnect() {
    try {
        std::string host = testnet_ ? "stream.testnet.binance.vision" : "stream.binance.com";
        std::string port = "9443";

        tcp::resolver resolver(ioContext_);
        auto const results = resolver.resolve(host, port);

        ws_ = std::make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(
            ioContext_, sslContext_);

        // SNI — requis par la plupart des serveurs TLS modernes, sinon le handshake échoue.
        if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
        }

        auto ep = beast::get_lowest_layer(*ws_).connect(results);
        std::string hostHeader = host + ':' + std::to_string(ep.port());

        ws_->next_layer().handshake(ssl::stream_base::client);
        ws_->set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
            req.set(beast::http::field::user_agent, "tradingbot/1.0");
        }));
        ws_->handshake(hostHeader, "/stream");

        connected_ = true;
        reconnectDelayMs_ = 1000;
        std::cout << "[BinanceConnector] Connecté (" << (testnet_ ? "testnet" : "production") << ")\n";

        // Rejoue les abonnements demandés avant que la connexion soit prête.
        {
            std::lock_guard<std::mutex> lock(subsMutex_);
            for (const auto& stream : pendingSubscriptions_) {
                sendSubscribe(stream);
            }
        }

        doRead();
    } catch (const std::exception& e) {
        std::cerr << "[BinanceConnector] Échec de connexion : " << e.what() << "\n";
        connected_ = false;
        scheduleReconnect();
    }
}

void BinanceConnector::doRead() {
    ws_->async_read(buffer_, [this](beast::error_code ec, std::size_t) {
        if (ec) {
            std::cerr << "[BinanceConnector] Erreur de lecture WebSocket : " << ec.message() << "\n";
            connected_ = false;
            scheduleReconnect();
            return;
        }
        std::string payload = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        handleMessage(payload);
        if (shouldRun_) doRead();
    });
}

void BinanceConnector::handleMessage(const std::string& payload) {
    try {
        json msg = json::parse(payload);
        // Format du stream combiné : {"stream":"btcusdt@ticker","data":{...}}
        const json& data = msg.contains("data") ? msg["data"] : msg;
        if (!data.contains("e")) return; // pas un événement de ticker connu

        std::string eventType = data.value("e", "");
        if (eventType != "24hrTicker") return;

        Tick tick;
        tick.symbol = data.value("s", "");
        tick.market = MarketType::Crypto;
        tick.bid = std::stod(data.value("b", "0"));
        tick.ask = std::stod(data.value("a", "0"));
        tick.last = std::stod(data.value("c", "0"));
        tick.volume = std::stod(data.value("v", "0"));
        tick.timestamp = std::chrono::system_clock::now();

        if (tickCallback_) tickCallback_(tick);
    } catch (const std::exception& e) {
        std::cerr << "[BinanceConnector] Message ignoré (parsing) : " << e.what() << "\n";
    }
}

void BinanceConnector::sendSubscribe(const std::string& streamName) {
    json req = {
        {"method", "SUBSCRIBE"},
        {"params", json::array({streamName})},
        {"id", subscribeMsgId_++}
    };
    std::string payload = req.dump();
    ws_->async_write(net::buffer(payload), [](beast::error_code ec, std::size_t) {
        if (ec) std::cerr << "[BinanceConnector] Échec d'envoi SUBSCRIBE : " << ec.message() << "\n";
    });
}

void BinanceConnector::subscribe(const std::string& symbol) {
    std::string streamName = toLowerCopy(symbol) + "@ticker";
    std::lock_guard<std::mutex> lock(subsMutex_);
    pendingSubscriptions_.insert(streamName);
    if (connected_ && ws_) {
        sendSubscribe(streamName);
    }
    // Sinon, l'abonnement sera rejoué automatiquement une fois connect() abouti.
}

void BinanceConnector::scheduleReconnect() {
    if (!shouldRun_) return;
    std::cout << "[BinanceConnector] Reconnexion dans " << reconnectDelayMs_ << " ms...\n";
    auto timer = std::make_shared<net::steady_timer>(ioContext_);
    timer->expires_after(std::chrono::milliseconds(reconnectDelayMs_));
    timer->async_wait([this, timer](beast::error_code) {
        if (!shouldRun_) return;
        doConnect();
    });
    reconnectDelayMs_ = std::min(reconnectDelayMs_ * 2, 60000); // plafonné à 60s
}

std::string BinanceConnector::sign(const std::string& queryString) const {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    HMAC(EVP_sha256(),
         apiSecret_.data(), static_cast<int>(apiSecret_.size()),
         reinterpret_cast<const unsigned char*>(queryString.data()), queryString.size(),
         digest, &digestLen);

    std::ostringstream oss;
    for (unsigned int i = 0; i < digestLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string BinanceConnector::placeOrder(const Order& order) {
    std::string baseUrl = testnet_
        ? "https://testnet.binance.vision"
        : "https://api.binance.com";

    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::ostringstream qs;
    qs << "symbol=" << order.symbol
       << "&side=" << (order.side == OrderSide::Buy ? "BUY" : "SELL")
       << "&type=" << (order.price > 0.0 ? "LIMIT" : "MARKET")
       << "&quantity=" << order.quantity;
    if (order.price > 0.0) {
        qs << "&price=" << order.price << "&timeInForce=GTC";
    }
    qs << "&timestamp=" << timestamp;

    std::string queryString = qs.str();
    std::string signature = sign(queryString);
    std::string fullUrl = baseUrl + "/api/v3/order?" + queryString + "&signature=" + signature;

    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("BinanceConnector: échec d'initialisation libcurl");

    std::string response;
    struct curl_slist* headers = nullptr;
    std::string apiKeyHeader = "X-MBX-APIKEY: " + apiKey_;
    headers = curl_slist_append(headers, apiKeyHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("BinanceConnector: requête échouée: ") + curl_easy_strerror(res));
    }

    json responseJson = json::parse(response, nullptr, false);
    if (httpCode != 200 || responseJson.is_discarded()) {
        throw std::runtime_error("BinanceConnector: ordre refusé (HTTP " + std::to_string(httpCode) + ") : " + response);
    }

    std::string orderId = std::to_string(responseJson.value("orderId", 0LL));
    std::cout << "[BinanceConnector] Ordre placé, id=" << orderId << "\n";
    return orderId;
}

void BinanceConnector::cancelOrder(const std::string& orderId) {
    // TODO: DELETE /api/v3/order?symbol=...&orderId=...&timestamp=...&signature=...
    // Nécessite de connaître le symbole associé à orderId — à stocker côté OrderExecutor
    // lors du placeOrder() pour pouvoir annuler proprement.
    std::cout << "[BinanceConnector] cancelOrder " << orderId << " (non implémenté — voir TODO)\n";
}

} // namespace tradingbot
