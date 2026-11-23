#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "hash_core.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

const char* kEmptySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
const char* kAbcSha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

}  // namespace

TEST_CASE("All fixed hashes produce their required output size") {
    const std::pair<const char*, int> cases[] = {
        {"sha224", 28}, {"sha256", 32}, {"sha384", 48}, {"sha512", 64},
        {"sha3-224", 28}, {"sha3-256", 32}, {"sha3-384", 48}, {"sha3-512", 64},
    };
    for (const auto& item : cases) {
        const std::string digest = hashcore::hashBytes(item.first, "abc", item.second * 8);
        REQUIRE(digest.size() == static_cast<std::size_t>(item.second));
    }
}

TEST_CASE("NIST example digests") {
    REQUIRE(hashcore::hex(hashcore::hashBytes("sha256", "", 256)) == kEmptySha256);
    REQUIRE(hashcore::hex(hashcore::hashBytes("sha256", "abc", 256)) == kAbcSha256);
    REQUIRE(hashcore::hex(hashcore::hashBytes("sha3-256", "abc", 256)) ==
            "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
}

TEST_CASE("SHAKE output length is honoured") {
    REQUIRE(hashcore::hashBytes("shake128", "", 128).size() == 16);
    REQUIRE(hashcore::hashBytes("shake256", "", 512).size() == 64);
    REQUIRE(hashcore::hex(hashcore::hashBytes("shake128", "", 256)) ==
            "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef26");
}

TEST_CASE("Algorithm names are case-insensitive and accept underscores") {
    REQUIRE(hashcore::hex(hashcore::hashBytes("SHA3_256", "abc", 256)) ==
            "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
    REQUIRE_THROWS(hashcore::find("md5"));
    REQUIRE_THROWS(hashcore::find("sha1"));
}

TEST_CASE("Output-length validation fails closed") {
    REQUIRE(hashcore::outputBits("sha256", false, 0) == 256);
    REQUIRE_THROWS(hashcore::outputBits("sha256", true, 256));
    REQUIRE_THROWS(hashcore::outputBits("shake128", false, 0));
    REQUIRE_THROWS(hashcore::outputBits("shake128", true, 0));
    REQUIRE_THROWS(hashcore::outputBits("shake128", true, 7));
    REQUIRE_THROWS(hashcore::outputBits("shake128", true, 8 * 1024 * 1024 + 8));
    REQUIRE(hashcore::outputBits("shake128", true, 128) == 128);
}

TEST_CASE("Stream and mmap file paths agree") {
    const auto path = std::filesystem::temp_directory_path() / "hashtool_test_input.bin";
    {
        std::ofstream file(path, std::ios::binary);
        REQUIRE(file.good());
        file << "streamed hashing input";
    }
    const std::string streamed = hashcore::hashFile("sha256", path.string(), 256, false);
    const std::string mapped = hashcore::hashFile("sha256", path.string(), 256, true);
    REQUIRE(streamed == mapped);
    std::filesystem::remove(path);
}

TEST_CASE("Missing files and bad streams fail") {
    REQUIRE_THROWS(hashcore::hashFile("sha256", "/definitely/not/here", 256, false));
    std::istringstream input("abc");
    REQUIRE(hashcore::hex(hashcore::hashStream("sha256", input, 256)) == kAbcSha256);
}
