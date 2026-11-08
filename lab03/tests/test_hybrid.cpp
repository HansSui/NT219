#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "hybrid.hpp"
#include "rsa.hpp"
#include "util.hpp"

#include <string>

namespace {

const rsa::Key& testKey() {
    static const rsa::Key key = rsa::generateKey(3072);
    return key;
}

const rsa::Key& secondKey() {
    static const rsa::Key key = rsa::generateKey(3072);
    return key;
}

}  // namespace

TEST_CASE("Hybrid envelope round-trips 1 KiB and 1 MiB") {
    for (const std::size_t size : {static_cast<std::size_t>(1024),
                                   static_cast<std::size_t>(1048576)}) {
        const std::string plaintext(size, 'h');
        const std::string envelope = hybrid::encrypt(testKey(), plaintext, "ctx");
        REQUIRE(hybrid::isEnvelope(envelope));
        REQUIRE(hybrid::decrypt(testKey(), envelope, "ctx") == plaintext);
    }
}

TEST_CASE("Envelope is self-describing and carries the algorithm metadata") {
    const std::string envelope = hybrid::encrypt(testKey(), std::string(2048, 'x'), "");
    const hybrid::Envelope parsed = hybrid::parse(envelope);
    REQUIRE(parsed.version == 1);
    REQUIRE(parsed.mode == "RSA-OAEP-AES-GCM");
    REQUIRE(parsed.rsaModulus == 3072);
    REQUIRE(parsed.hash == "SHA-256");
    REQUIRE(parsed.iv.size() == 12);
    REQUIRE(parsed.tag.size() == 16);
    REQUIRE(parsed.wrappedKey.size() == 384);
    REQUIRE(parsed.payload.size() == 2048);
}

TEST_CASE("Tampered GCM ciphertext, tag, IV, wrapped key or header fails") {
    const std::string original = hybrid::encrypt(testKey(), std::string(512, 'x'), "label");
    const hybrid::Envelope good = hybrid::parse(original);

    const auto rejected = [](const hybrid::Envelope& envelope) {
        REQUIRE_THROWS(hybrid::decrypt(testKey(), hybrid::serialize(envelope), "label"));
    };

    hybrid::Envelope bad = good;
    bad.payload[0] ^= 0x01;
    rejected(bad);

    bad = good;
    bad.tag[0] ^= 0x01;
    rejected(bad);

    bad = good;
    bad.iv[0] ^= 0x01;
    rejected(bad);

    bad = good;
    bad.wrappedKey[0] ^= 0x01;
    rejected(bad);

    bad = good;
    bad.rsaModulus = 4096;
    rejected(bad);

    bad = good;
    bad.label = "tampered";
    rejected(bad);
}

TEST_CASE("Wrong label, wrong key and malformed envelopes fail closed") {
    const std::string envelope = hybrid::encrypt(testKey(), std::string(1024, 'x'), "right");
    REQUIRE_THROWS(hybrid::decrypt(testKey(), envelope, "wrong"));
    REQUIRE_THROWS(hybrid::decrypt(secondKey(), envelope, "right"));
    REQUIRE_THROWS(hybrid::parse("RSAENV1\n{}\n"));
    REQUIRE_THROWS(hybrid::parse("not an envelope"));
    REQUIRE_THROWS(hybrid::parse(std::string(hybrid::kMagic) + "{\"version\":1}\npayload"));
}

TEST_CASE("AES-256-GCM rejects tampered ciphertext, tag and AAD") {
    const std::string key(32, 'k');
    const std::string iv(12, 'i');
    std::string tag;
    const std::string ciphertext = hybrid::aesGcmEncrypt(key, iv, "aad", "payload", tag);
    REQUIRE(hybrid::aesGcmDecrypt(key, iv, "aad", ciphertext, tag) == "payload");
    REQUIRE_THROWS(hybrid::aesGcmDecrypt(key, iv, "other", ciphertext, tag));

    std::string badTag = tag;
    badTag[0] ^= 0x01;
    REQUIRE_THROWS(hybrid::aesGcmDecrypt(key, iv, "aad", ciphertext, badTag));

    std::string badCiphertext = ciphertext;
    badCiphertext[0] ^= 0x01;
    REQUIRE_THROWS(hybrid::aesGcmDecrypt(key, iv, "aad", badCiphertext, tag));
}
