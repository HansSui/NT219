#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "hash_core.hpp"
#include "sha256_ext.hpp"

#include <string>

TEST_CASE("SHA-256 matches the standard vectors") {
    REQUIRE(hashcore::hex(sha256ext::sha256("")) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    REQUIRE(hashcore::hex(sha256ext::sha256("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(hashcore::hex(sha256ext::sha256(
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("Length extension reconstructs H(k || m || pad || m')") {
    const std::string key = "0123456789abcdef";
    const std::string message = "amount=100&to=alice";
    const std::string append = "&to=mallory";
    const std::string mac = sha256ext::sha256(key + message);

    std::string forged;
    const std::string forgedMac =
        sha256ext::extend(mac, key.size(), message, append, forged);
    REQUIRE(forgedMac == sha256ext::sha256(key + forged));
    REQUIRE(forged.compare(0, message.size(), message) == 0);
    REQUIRE(forged.size() > message.size());
    REQUIRE(forged.compare(forged.size() - append.size(), append.size(), append) == 0);
}

TEST_CASE("Padding boundaries are handled") {
    const std::string key = "key";
    for (std::size_t length : {0, 1, 55, 56, 63, 64, 120}) {
        const std::string message(length, 'm');
        const std::string append = "tail";
        const std::string mac = sha256ext::sha256(key + message);
        std::string forged;
        const std::string forgedMac =
            sha256ext::extend(mac, key.size(), message, append, forged);
        REQUIRE(forgedMac == sha256ext::sha256(key + forged));
    }
}

TEST_CASE("Bad MAC lengths are rejected") {
    std::string forged;
    REQUIRE_THROWS(sha256ext::extend("short", 16, "message", "append", forged));
    REQUIRE_THROWS(sha256ext::extend(std::string(33, '\0'), 16, "message", "append", forged));
}
