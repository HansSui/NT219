#ifndef MODES_HPP
#define MODES_HPP

#include "AES.hpp"
#include <array>
#include <cstdint>
#include <vector>

using Block = std::array<uint8_t, 16>;

// Lab 2 modes of operation: CTR (required) and XTS (bonus B).
class Modes {
public:
    // `allow_aesni` keeps a pure-software path available for benchmarking.
    std::vector<uint8_t> ctr_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, Block counter,
                                     bool allow_aesni = true);
    std::vector<uint8_t> ctr_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, Block counter,
                                     bool allow_aesni = true);

    // IEEE P1619 XTS: key1 protects data, key2 encrypts the 16-byte data-unit tweak.
    std::vector<uint8_t> xts_encrypt(const AES& key1, const AES& key2,
                                     const std::vector<uint8_t>& plaintext, const Block& data_unit);
    std::vector<uint8_t> xts_decrypt(const AES& key1, const AES& key2,
                                     const std::vector<uint8_t>& ciphertext, const Block& data_unit);
};

#endif // MODES_HPP
