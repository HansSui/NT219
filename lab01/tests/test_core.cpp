#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "aescore.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

std::string pattern(std::size_t size, unsigned start = 0) {
    std::string out(size, '\0');
    for (std::size_t i = 0; i < size; ++i) out[i] = static_cast<char>((start + i) & 0xff);
    return out;
}

std::string key256() { return pattern(32, 0x10); }
std::string keyXts() { return pattern(64, 0x40); }

}  // namespace

TEST_CASE("hex round-trip") {
    const std::string data = pattern(33, 0xa0);
    REQUIRE(aes::fromHex(aes::toHex(data)) == data);
    REQUIRE(aes::fromHex("00:01 02\nff") == std::string("\x00\x01\x02\xff", 4));
    REQUIRE_THROWS_AS(aes::fromHex("abc"), std::runtime_error);
    REQUIRE_THROWS_AS(aes::fromHex("zz"), std::runtime_error);
}

TEST_CASE("base64 encoding") {
    REQUIRE(aes::toBase64("hello") == "aGVsbG8=");
    REQUIRE(aes::toBase64("") == "");
}

TEST_CASE("non-AEAD modes round-trip") {
    const std::string plaintext = pattern(37, 0x01);
    const std::string key = key256();
    const std::string iv = pattern(16, 0x80);

    for (aes::Mode mode : {aes::Mode::ECB, aes::Mode::CBC, aes::Mode::OFB, aes::Mode::CFB,
                           aes::Mode::CTR}) {
        INFO("mode " << aes::modeName(mode));
        const auto encrypted = aes::encrypt(mode, key, mode == aes::Mode::ECB ? "" : iv, plaintext);
        REQUIRE(encrypted.tag.empty());
        REQUIRE(encrypted.ciphertext != plaintext);
        REQUIRE(aes::decrypt(mode, key, mode == aes::Mode::ECB ? "" : iv,
                             encrypted.ciphertext) == plaintext);
    }
}

TEST_CASE("XTS handles a partial final block") {
    const std::string plaintext = pattern(37, 0x11);
    const std::string key = keyXts();
    const std::string iv = pattern(16, 0x22);
    const auto encrypted = aes::encrypt(aes::Mode::XTS, key, iv, plaintext);
    REQUIRE(encrypted.ciphertext.size() == plaintext.size());
    REQUIRE(aes::decrypt(aes::Mode::XTS, key, iv, encrypted.ciphertext) == plaintext);
}

TEST_CASE("AEAD round-trip and authentication") {
    const std::string plaintext = pattern(61, 0x31);
    const std::string aad = "header-authenticated";
    const std::string key = key256();

    SECTION("GCM") {
        const std::string iv = pattern(12, 0x40);
        const auto encrypted = aes::encrypt(aes::Mode::GCM, key, iv, plaintext, aad);
        REQUIRE(encrypted.tag.size() == 16);
        REQUIRE(aes::decrypt(aes::Mode::GCM, key, iv, encrypted.ciphertext, aad, encrypted.tag) ==
                plaintext);

        std::string badTag = encrypted.tag;
        badTag[0] ^= 0x01;
        REQUIRE_THROWS_AS(aes::decrypt(aes::Mode::GCM, key, iv, encrypted.ciphertext, aad, badTag),
                          std::runtime_error);

        std::string wrongIv = iv;
        wrongIv[0] ^= 0x01;
        REQUIRE_THROWS_AS(
            aes::decrypt(aes::Mode::GCM, key, wrongIv, encrypted.ciphertext, aad, encrypted.tag),
            std::runtime_error);

        std::string wrongKey = key;
        wrongKey[0] ^= 0x01;
        REQUIRE_THROWS_AS(
            aes::decrypt(aes::Mode::GCM, wrongKey, iv, encrypted.ciphertext, aad, encrypted.tag),
            std::runtime_error);

        std::string badAad = aad;
        badAad[0] ^= 0x01;
        REQUIRE_THROWS_AS(
            aes::decrypt(aes::Mode::GCM, key, iv, encrypted.ciphertext, badAad, encrypted.tag),
            std::runtime_error);
    }

    SECTION("CCM with a 13-byte nonce") {
        const std::string iv = pattern(13, 0x50);
        const auto encrypted = aes::encrypt(aes::Mode::CCM, key, iv, plaintext, aad);
        REQUIRE(encrypted.tag.size() == 8);
        REQUIRE(aes::decrypt(aes::Mode::CCM, key, iv, encrypted.ciphertext, aad, encrypted.tag) ==
                plaintext);

        std::string badTag = encrypted.tag;
        badTag[0] ^= 0x01;
        REQUIRE_THROWS_AS(aes::decrypt(aes::Mode::CCM, key, iv, encrypted.ciphertext, aad, badTag),
                          std::runtime_error);
    }
}

TEST_CASE("empty plaintext for stream and padded modes") {
    const std::string key = key256();
    const std::string iv = pattern(16, 0x60);
    for (aes::Mode mode : {aes::Mode::CBC, aes::Mode::OFB, aes::Mode::CFB, aes::Mode::CTR}) {
        const auto encrypted = aes::encrypt(mode, key, iv, "");
        REQUIRE(aes::decrypt(mode, key, iv, encrypted.ciphertext).empty());
    }
}

TEST_CASE("invalid key and IV lengths are rejected") {
    const std::string key = key256();
    const std::string iv = pattern(16, 0x70);
    REQUIRE_THROWS_AS(aes::encrypt(aes::Mode::CBC, "short", iv, "data"), std::runtime_error);
    REQUIRE_THROWS_AS(aes::encrypt(aes::Mode::CBC, key, "short", "data"), std::runtime_error);
    REQUIRE_THROWS_AS(aes::encrypt(aes::Mode::GCM, key, pattern(16, 0x70), "data"),
                      std::runtime_error);
    REQUIRE_THROWS_AS(aes::encrypt(aes::Mode::XTS, std::string(16, 'x'), iv, "data"),
                      std::runtime_error);
    REQUIRE_THROWS_AS(aes::encrypt(aes::Mode::CBC, key, iv, "data", "aad-without-aead"),
                      std::runtime_error);
}

TEST_CASE("container round-trip and malformed headers") {
    const std::string key = key256();
    const std::string iv = pattern(12, 0x90);
    const std::string aad = "aad";
    const auto encrypted = aes::encrypt(aes::Mode::GCM, key, iv, "container payload", aad);
    const aes::Record original{aes::Mode::GCM, iv, aad, encrypted.tag, encrypted.ciphertext};
    const std::string bytes = aes::serialize(original);
    const aes::Record parsed = aes::parseRecord(bytes);

    REQUIRE(parsed.mode == aes::Mode::GCM);
    REQUIRE(parsed.iv == iv);
    REQUIRE(parsed.aad == aad);
    REQUIRE(parsed.tag == encrypted.tag);
    REQUIRE(parsed.ciphertext == encrypted.ciphertext);
    REQUIRE(aes::decrypt(parsed.mode, key, parsed.iv, parsed.ciphertext, parsed.aad, parsed.tag) ==
            "container payload");

    REQUIRE_THROWS_AS(aes::parseRecord("short"), std::runtime_error);
    REQUIRE_THROWS_AS(aes::parseRecord("XXXXX" + bytes.substr(5)), std::runtime_error);
}

TEST_CASE("container parser handles mutated input without crashing") {
    const std::string key = key256();
    const std::string iv = pattern(12, 0x91);
    const auto encrypted = aes::encrypt(aes::Mode::GCM, key, iv, "mutation payload");
    const std::string bytes =
        aes::serialize({aes::Mode::GCM, iv, "", encrypted.tag, encrypted.ciphertext});

    std::mt19937 rng(0xA35EED);
    int rejected = 0;
    for (int i = 0; i < 300; ++i) {
        std::string mutated = bytes;
        if (i % 3 == 0) {
            mutated.resize(rng() % (mutated.size() + 1));
        } else {
            mutated[rng() % mutated.size()] ^= static_cast<char>(1U << (rng() % 8));
        }
        try {
            const aes::Record record = aes::parseRecord(mutated);
            (void)aes::decrypt(record.mode, key, record.iv, record.ciphertext, record.aad,
                               record.tag);
        } catch (const std::exception&) {
            ++rejected;
        }
    }
    REQUIRE(rejected > 0);
}

TEST_CASE("key file supports raw bytes and hex-prefixed text") {
    const std::string key = key256();
    const auto rawPath = std::filesystem::temp_directory_path() / "aestool_test_key_raw.bin";
    const auto hexPath = std::filesystem::temp_directory_path() / "aestool_test_key_hex.txt";

    {
        std::ofstream raw(rawPath, std::ios::binary);
        raw.write(key.data(), static_cast<std::streamsize>(key.size()));
    }
    {
        std::ofstream hex(hexPath);
        hex << "hex:" << aes::toHex(key) << '\n';
    }

    REQUIRE(aes::loadKeyFile(rawPath.string()) == key);
    REQUIRE(aes::loadKeyFile(hexPath.string()) == key);

    std::filesystem::remove(rawPath);
    std::filesystem::remove(hexPath);
}
