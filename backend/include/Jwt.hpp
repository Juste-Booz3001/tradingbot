#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace tradingbot {

// Implémentation JWT minimale (HS256 uniquement) — volontairement sans
// dépendance externe (pas de jwt-cpp) puisque le besoin ici se limite à
// authentifier le dashboard face au moteur, pas à interopérer avec des
// tiers. Ne pas réutiliser tel quel pour de l'auth inter-services.
class Jwt {
public:
    // Signe les claims fournis avec HMAC-SHA256, ajoute "exp" (epoch, secondes).
    static std::string sign(nlohmann::json claims, const std::string& secret, int expiresInSeconds);

    // Vérifie signature + expiration. Retourne les claims si le token est valide.
    static std::optional<nlohmann::json> verify(const std::string& token, const std::string& secret);

    // SHA-256 hex — utilisé pour stocker/comparer le mot de passe admin sans le garder en clair.
    static std::string sha256Hex(const std::string& input);

private:
    static std::string base64UrlEncode(const std::string& input);
    static std::string base64UrlDecode(const std::string& input);
    static std::string hmacSha256(const std::string& data, const std::string& secret);
};

} // namespace tradingbot
