#include "catch.hpp"
#include "AES.hpp"
#include "modes.hpp"
#include <vector>
#include <array>
#include <string>
#include <stdexcept>

TEST_CASE("NIST SP 800-38A AES-128 CTR Known Answer Test", "[ctr][kat]") {
    // NIST SP 800-38A CTR Mode Key
    std::vector<uint8_t> key = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };

    // Starting Counter block (T1)
    Block iv = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };

    // 64-byte plaintext (4 full blocks)
    std::vector<uint8_t> pt = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
        0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
        0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
        0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
        0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
        0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
        0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
    };

    // Expected Ciphertext
    std::vector<uint8_t> expected_ct = {
        0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
        0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
        0x98, 0x06, 0xf6, 0x6b, 0x79, 0x70, 0xfd, 0xff,
        0x86, 0x17, 0x18, 0x7b, 0xb9, 0xff, 0xfd, 0xff,
        0x5a, 0xe4, 0xdf, 0x3e, 0xdb, 0xd5, 0xd3, 0x5e,
        0x5b, 0x4f, 0x09, 0x02, 0x0d, 0xb0, 0x3e, 0xab,
        0x1e, 0x03, 0x1d, 0xda, 0x2f, 0xbe, 0x03, 0xd1,
        0x79, 0x21, 0x70, 0xa0, 0xf3, 0x00, 0x9c, 0xee
    };

    AES cipher(key, AES::AES_128);
    Modes modeA;

    std::vector<uint8_t> ct = modeA.ctr_encrypt(cipher, pt, iv);
    REQUIRE(ct == expected_ct);

    std::vector<uint8_t> decrypted = modeA.ctr_decrypt(cipher, ct, iv);
    REQUIRE(decrypted == pt);
}

TEST_CASE("CTR Mode - Arbitrary Plaintext Length (No Padding)", "[ctr][partial]") {
    std::vector<uint8_t> key(16, 0x42);
    Block iv{};
    iv.fill(0x01);

    AES cipher(key, AES::AES_128);
    Modes modeA;

    // Test non-multiples of block size (1, 7, 15, 17, 33, 100 bytes)
    std::vector<size_t> test_sizes = {1, 7, 15, 17, 33, 100, 255};
    for (size_t size : test_sizes) {
        std::vector<uint8_t> pt(size);
        for (size_t i = 0; i < size; ++i) pt[i] = static_cast<uint8_t>(i & 0xFF);

        std::vector<uint8_t> ct = modeA.ctr_encrypt(cipher, pt, iv);
        REQUIRE(ct.size() == pt.size()); // Exactly same length!

        std::vector<uint8_t> dec = modeA.ctr_decrypt(cipher, ct, iv);
        REQUIRE(dec == pt);
    }
}

TEST_CASE("CTR Mode - Counter overflow fails closed", "[ctr][overflow]") {
    std::vector<uint8_t> key(16, 0x00);
    AES cipher(key, AES::AES_128);
    Modes modeA;

    Block counter;
    counter.fill(0xFF); // last valid counter block

    // One block is fine; a second block would wrap to 0 and reuse the keystream.
    std::vector<uint8_t> one_block(16, 0x00);
    REQUIRE_NOTHROW(modeA.ctr_encrypt(cipher, one_block, counter));

    std::vector<uint8_t> two_blocks(32, 0x00);
    REQUIRE_THROWS_AS(modeA.ctr_encrypt(cipher, two_blocks, counter), std::runtime_error);
}

TEST_CASE("XTS - full blocks and ciphertext stealing round trip", "[xts]") {
    std::vector<uint8_t> key(32);
    for (size_t i = 0; i < key.size(); ++i) key[i] = static_cast<uint8_t>(i);

    AES key1(key.data(), 16, AES::AES_128);
    AES key2(key.data() + 16, 16, AES::AES_128);
    Modes modes;

    Block dataUnit{};
    dataUnit[0] = 1; // sector 1, little-endian

    for (size_t size : std::vector<size_t>{16, 32, 33, 64, 65, 100}) {
        std::vector<uint8_t> pt(size);
        for (size_t i = 0; i < size; ++i) pt[i] = static_cast<uint8_t>(i * 7);

        std::vector<uint8_t> ct = modes.xts_encrypt(key1, key2, pt, dataUnit);
        REQUIRE(ct.size() == pt.size());
        REQUIRE(ct != pt);
        REQUIRE(modes.xts_decrypt(key1, key2, ct, dataUnit) == pt);
    }

    std::vector<uint8_t> too_short(15, 0);
    REQUIRE_THROWS_AS(modes.xts_encrypt(key1, key2, too_short, dataUnit), std::invalid_argument);
}

TEST_CASE("CTR Mode - Negative Tests & Malleability Demonstration", "[ctr][negative]") {
    std::vector<uint8_t> key_valid(16, 0x11);
    std::vector<uint8_t> key_wrong(16, 0x22);
    Block iv_valid{}; iv_valid.fill(0xAA);
    Block iv_wrong{}; iv_wrong.fill(0xBB);

    std::string msg = "Secret Payload Requiring Authentication!";
    std::vector<uint8_t> pt(msg.begin(), msg.end());

    AES cipher_valid(key_valid, AES::AES_128);
    AES cipher_wrong(key_wrong, AES::AES_128);
    Modes modeA;

    std::vector<uint8_t> ct = modeA.ctr_encrypt(cipher_valid, pt, iv_valid);

    SECTION("Wrong Key produces corrupted plaintext") {
        std::vector<uint8_t> dec = modeA.ctr_decrypt(cipher_wrong, ct, iv_valid);
        REQUIRE(dec != pt);
    }

    SECTION("Wrong IV produces corrupted plaintext") {
        std::vector<uint8_t> dec = modeA.ctr_decrypt(cipher_valid, ct, iv_wrong);
        REQUIRE(dec != pt);
    }

    SECTION("Tampered Ciphertext - Bit-flipping malleability") {
        // Flipping byte 0 in ciphertext flips byte 0 in decrypted plaintext by exact same mask!
        std::vector<uint8_t> tampered_ct = ct;
        tampered_ct[0] ^= 0x20; // Flip 1 bit

        std::vector<uint8_t> dec_tampered = modeA.ctr_decrypt(cipher_valid, tampered_ct, iv_valid);
        REQUIRE(dec_tampered != pt);
        REQUIRE(dec_tampered[0] == (pt[0] ^ 0x20)); // Exact bit flipped!
        // Remaining bytes remain intact (shows malleability and lack of integrity)
        for (size_t i = 1; i < pt.size(); ++i) {
            REQUIRE(dec_tampered[i] == pt[i]);
        }
    }
}
