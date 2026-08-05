#include "Jwt.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace tradingbot {

using json = nlohmann::json;

namespace {
constexpr char kBase64UrlChars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
}

std::string Jwt::base64UrlEncode(const std::string& input) {
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < input.size()) {
        unsigned int n = (static_cast<unsigned char>(input[i]) << 16) |
                         (static_cast<unsigned char>(input[i + 1]) << 8) |
                         static_cast<unsigned char>(input[i + 2]);
        out += kBase64UrlChars[(n >> 18) & 0x3F];
        out += kBase64UrlChars[(n >> 12) & 0x3F];
        out += kBase64UrlChars[(n >> 6) & 0x3F];
        out += kBase64UrlChars[n & 0x3F];
        i += 3;
    }
    if (i < input.size()) {
        unsigned int n = static_cast<unsigned char>(input[i]) << 16;
        if (i + 1 < input.size()) n |= static_cast<unsigned char>(input[i + 1]) << 8;
        out += kBase64UrlChars[(n >> 18) & 0x3F];
        out += kBase64UrlChars[(n >> 12) & 0x3F];
        if (i + 1 < input.size()) out += kBase64UrlChars[(n >> 6) & 0x3F];
    }
    // Base64url "sans padding" (norme JWT / RFC 7515) : pas de '='.
    return out;
}

std::string Jwt::base64UrlDecode(const std::string& input) {
    static int table[256];
    static bool initialized = false;
    if (!initialized) {
        std::fill(std::begin(table), std::end(table), -1);
        for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(kBase64UrlChars[i])] = i;
        initialized = true;
    }

    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : input) {
        if (table[c] == -1) continue; // ignore tout caractère hors alphabet (ex. \n)
        val = (val << 6) + table[c];
        bits += 6;
        if (bits >= 0) {
            out += static_cast<char>((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

std::string Jwt::hmacSha256(const std::string& data, const std::string& secret) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    HMAC(EVP_sha256(),
         secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &digestLen);
    return std::string(reinterpret_cast<char*>(digest), digestLen);
}

std::string Jwt::sha256Hex(const std::string& input) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
    std::ostringstream oss;
    for (unsigned char b : digest) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

std::string Jwt::sign(nlohmann::json claims, const std::string& secret, int expiresInSeconds) {
    long long now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    claims["iat"] = now;
    claims["exp"] = now + expiresInSeconds;

    json header = {{"alg", "HS256"}, {"typ", "JWT"}};

    std::string headerPart = base64UrlEncode(header.dump());
    std::string payloadPart = base64UrlEncode(claims.dump());
    std::string signingInput = headerPart + "." + payloadPart;
    std::string signature = base64UrlEncode(hmacSha256(signingInput, secret));

    return signingInput + "." + signature;
}

std::optional<nlohmann::json> Jwt::verify(const std::string& token, const std::string& secret) {
    size_t firstDot = token.find('.');
    size_t secondDot = token.find('.', firstDot == std::string::npos ? 0 : firstDot + 1);
    if (firstDot == std::string::npos || secondDot == std::string::npos) return std::nullopt;

    std::string headerPart = token.substr(0, firstDot);
    std::string payloadPart = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string signaturePart = token.substr(secondDot + 1);

    std::string signingInput = headerPart + "." + payloadPart;
    std::string expectedSignature = base64UrlEncode(hmacSha256(signingInput, secret));

    // Comparaison à temps constant pour éviter une attaque par mesure de timing.
    if (expectedSignature.size() != signaturePart.size()) return std::nullopt;
    unsigned char diff = 0;
    for (size_t i = 0; i < expectedSignature.size(); ++i) {
        diff |= static_cast<unsigned char>(expectedSignature[i]) ^ static_cast<unsigned char>(signaturePart[i]);
    }
    if (diff != 0) return std::nullopt;

    json claims = json::parse(base64UrlDecode(payloadPart), nullptr, false);
    if (claims.is_discarded()) return std::nullopt;

    long long now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (claims.value("exp", 0LL) < now) return std::nullopt; // token expiré

    return claims;
}

} // namespace tradingbot
