#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "hybrid.hpp"
#include "rsa.hpp"
#include "util.hpp"

#include <string>

TEST_CASE("Hex and base64 parsers fail closed") {
    REQUIRE(util::fromHex("00ff") == std::string("\x00\xff", 2));
    REQUIRE_THROWS(util::fromHex("abc"));
    REQUIRE_THROWS(util::fromHex("zz"));

    REQUIRE(util::fromBase64(util::toBase64("payload")) == "payload");
    REQUIRE_THROWS(util::fromBase64("AAA"));
    REQUIRE_THROWS(util::fromBase64("!!!!"));
    REQUIRE_THROWS(util::fromBase64("A==="));
}

TEST_CASE("Unsupported key sizes fail at the trust boundary") {
    REQUIRE_THROWS(rsa::generateKey(1024));
    REQUIRE_THROWS(rsa::generateKey(2048));
}

TEST_CASE("Envelope validation rejects bad versions, modes, hashes and lengths") {
    const rsa::Key key = rsa::generateKey(3072);
    const std::string envelope = hybrid::encrypt(key, std::string(1024, 'x'), "");
    const hybrid::Envelope good = hybrid::parse(envelope);

    auto badHeader = [](const hybrid::Envelope& source, const std::string& from,
                        const std::string& to) {
        std::string text = hybrid::serialize(source);
        const std::size_t position = text.find(from);
        REQUIRE(position != std::string::npos);
        text.replace(position, from.size(), to);
        return text;
    };

    REQUIRE_THROWS(hybrid::parse(badHeader(good, "\"version\":1", "\"version\":2")));
    REQUIRE_THROWS(hybrid::parse(badHeader(good, "RSA-OAEP-AES-GCM", "AES-CBC")));
    REQUIRE_THROWS(hybrid::parse(badHeader(good, "\"hash\":\"SHA-256\"", "\"hash\":\"SHA-1\"")));
    REQUIRE_THROWS(hybrid::parse(badHeader(good, "\"payload_len\":1024", "\"payload_len\":1025")));
}

TEST_CASE("Direct decipher rejects wrong-length and tampered ciphertext") {
    const rsa::Key key = rsa::generateKey(3072);
    const std::string ciphertext = rsa::oaepEncrypt(key, "data", "");
    REQUIRE_THROWS(rsa::oaepDecrypt(key, ciphertext.substr(0, ciphertext.size() - 1), ""));
    REQUIRE_THROWS(rsa::oaepDecrypt(key, std::string(ciphertext.size(), '\0'), ""));
}

TEST_CASE("Hybrid labels are authenticated") {
    const rsa::Key key = rsa::generateKey(3072);
    const std::string envelope = hybrid::encrypt(key, std::string(1024, 'x'), "right");
    REQUIRE_THROWS(hybrid::decrypt(key, envelope, "wrong"));
}
