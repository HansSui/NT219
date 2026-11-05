#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "AES.hpp"
#include "modes.hpp"

#include <openssl/err.h>
#include <openssl/evp.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string openssl_error() {
    char buffer[256];
    ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
    return buffer;
}

std::vector<uint8_t> evp_crypt(const EVP_CIPHER* cipher,
                               const std::vector<uint8_t>& key,
                               const Block& iv,
                               const std::vector<uint8_t>& input) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) throw std::runtime_error("EVP_CIPHER_CTX_new: " + openssl_error());

    std::vector<uint8_t> output(input.size() + EVP_MAX_BLOCK_LENGTH);
    int out_len = 0;
    int final_len = 0;

    const bool ok =
        EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data()) == 1 &&
        EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
        EVP_EncryptUpdate(ctx, output.data(), &out_len, input.data(),
                          static_cast<int>(input.size())) == 1 &&
        EVP_EncryptFinal_ex(ctx, output.data() + out_len, &final_len) == 1;

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) throw std::runtime_error("OpenSSL EVP: " + openssl_error());

    output.resize(static_cast<size_t>(out_len + final_len));
    return output;
}

}  // namespace

TEST_CASE("Cross-Validation: Pure C++ AES-128-CTR vs OpenSSL AES-128-CTR", "[crossval]") {
    std::vector<uint8_t> key = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };

    Block iv = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };

    std::string plaintext_str = "Cryptography cross-validation test: Pure C++ FIPS-197 AES vs OpenSSL reference oracle!";
    std::vector<uint8_t> pt(plaintext_str.begin(), plaintext_str.end());

    AES our_cipher(key, AES::AES_128);
    Modes our_modeA;
    std::vector<uint8_t> our_ct = our_modeA.ctr_encrypt(our_cipher, pt, iv);
    std::vector<uint8_t> oracle = evp_crypt(EVP_aes_128_ctr(), key, iv, pt);

    REQUIRE(our_ct.size() == oracle.size());
    REQUIRE(our_ct == oracle);
    REQUIRE(our_modeA.ctr_decrypt(our_cipher, oracle, iv) == pt);
}

TEST_CASE("Cross-Validation: AES-192/AES-256 CTR vs OpenSSL", "[crossval][bonus]") {
    Block iv = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };
    std::vector<uint8_t> pt(257);
    for (size_t i = 0; i < pt.size(); ++i) pt[i] = static_cast<uint8_t>(i * 11 + 7);

    struct Case {
        std::vector<uint8_t> key;
        AES::KeyLength length;
        const EVP_CIPHER* cipher;
    };
    const std::vector<Case> cases = {
        {{0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
          0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
          0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b}, AES::AES_192,
         EVP_aes_192_ctr()},
        {{0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
          0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
          0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
          0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4}, AES::AES_256,
         EVP_aes_256_ctr()},
    };

    for (const auto& item : cases) {
        AES ours(item.key, item.length);
        Modes modes;
        std::vector<uint8_t> our_ct = modes.ctr_encrypt(ours, pt, iv);
        std::vector<uint8_t> expected = evp_crypt(item.cipher, item.key, iv, pt);
        REQUIRE(our_ct == expected);
        REQUIRE(modes.ctr_decrypt(ours, expected, iv) == pt);
    }
}

TEST_CASE("Cross-Validation: Pure C++ XTS vs OpenSSL XTS", "[crossval][xts]") {
    std::vector<uint8_t> key(32);
    for (size_t i = 0; i < key.size(); ++i) key[i] = static_cast<uint8_t>(i);

    Block dataUnit{};
    dataUnit[0] = 1;  // sector 1, little-endian

    AES key1(key.data(), 16, AES::AES_128);
    AES key2(key.data() + 16, 16, AES::AES_128);
    Modes modes;

    // 32/64 are block aligned; the other sizes exercise ciphertext stealing.
    for (size_t size : std::vector<size_t>{32, 37, 64, 65, 509}) {
        std::vector<uint8_t> pt(size);
        for (size_t i = 0; i < size; ++i) pt[i] = static_cast<uint8_t>(i * 13 + 5);

        std::vector<uint8_t> ours = modes.xts_encrypt(key1, key2, pt, dataUnit);
        std::vector<uint8_t> expected = evp_crypt(EVP_aes_128_xts(), key, dataUnit, pt);

        REQUIRE(ours.size() == expected.size());
        REQUIRE(ours == expected);
        REQUIRE(modes.xts_decrypt(key1, key2, expected, dataUnit) == pt);
    }
}
