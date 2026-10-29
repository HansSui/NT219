#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "AES.hpp"
#include "modesA.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>

#include <vector>
#include <string>

TEST_CASE("Cross-Validation: Pure C++ AES-128-CTR vs Crypto++ AES-128-CTR", "[crossval]") {
    std::vector<uint8_t> key = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };

    Block iv = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };

    std::string plaintext_str = "Cryptography cross-validation test: Pure C++ FIPS-197 AES vs Crypto++ reference oracle!";
    std::vector<uint8_t> pt(plaintext_str.begin(), plaintext_str.end());

    // 1. Encrypt with our Pure C++ implementation
    AES our_cipher(key, AES::AES_128);
    MODEa our_modeA;
    std::vector<uint8_t> our_ct = our_modeA.ctr_encrypt(our_cipher, pt, iv);

    // 2. Encrypt with Crypto++ reference oracle
    std::string cryptopp_ct_str;
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption cryptopp_enc;
    cryptopp_enc.SetKeyWithIV(key.data(), key.size(), iv.data(), 16);

    CryptoPP::StringSource ss(
        reinterpret_cast<const CryptoPP::byte*>(pt.data()), pt.size(), true,
        new CryptoPP::StreamTransformationFilter(
            cryptopp_enc,
            new CryptoPP::StringSink(cryptopp_ct_str),
            CryptoPP::StreamTransformationFilter::NO_PADDING
        )
    );

    std::vector<uint8_t> cryptopp_ct(cryptopp_ct_str.begin(), cryptopp_ct_str.end());

    // 3. Verify byte-for-byte identity
    REQUIRE(our_ct.size() == cryptopp_ct.size());
    REQUIRE(our_ct == cryptopp_ct);

    // 4. Cross-decryption: Decrypt Crypto++'s output using our Pure C++ engine
    std::vector<uint8_t> our_decrypted = our_modeA.ctr_decrypt(our_cipher, cryptopp_ct, iv);
    REQUIRE(our_decrypted == pt);
}
