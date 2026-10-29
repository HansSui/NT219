#ifndef MODES_A_HPP
#define MODES_A_HPP

#include "AES.hpp"
#include <vector>
#include <array>
#include <cstdint>

using Block = std::array<uint8_t, 16>;

class MODEa {
public:
    // AES-ECB
    std::vector<uint8_t> ECB_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext);
    std::vector<uint8_t> ECB_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext);

    // AES-CBC
    std::vector<uint8_t> CBC_encrypt(const AES& cipher, const std::vector<uint8_t> plaintext, const Block& iv);
    std::vector<uint8_t> CBC_decrypt(const AES& cipher, const std::vector<uint8_t> ciphertext, const Block& iv);

    // Counter increment functions
    void incr32(Block& counter);
    inline void inc64(Block& ctr);
    inline void inc128(Block& ctr);

    // CFB mode
    std::vector<uint8_t> cfb_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, const Block& iv, int segment_bits = 8);
    std::vector<uint8_t> cfb_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, const Block& iv, int segment_bits);

    // OFB mode
    std::vector<uint8_t> ofb_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, const Block& iv);
    std::vector<uint8_t> ofb_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, const Block& iv);

    // CTR mode
    std::vector<uint8_t> ctr_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, Block counter);
    std::vector<uint8_t> ctr_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, Block counter);

private:
    // Internal helpers
    static Block xor_blocks(const Block& a, const Block& b);
    static Block xor_blocks_partial(const Block& a, const uint8_t* b, size_t len);
    static void xor_into(uint8_t* out, const uint8_t* a, const uint8_t* b, size_t len);

    static std::vector<uint8_t> pkcs7_pad(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> pkcs7_unpad(const std::vector<uint8_t>& data);

    Block msb(const Block& x, size_t bits);
    Block lsb(const Block& x, size_t bits);
    void shift_left_append(Block& reg, const Block& segment, size_t bits);

    std::vector<uint8_t> ofb_crypt(const AES& cipher, const std::vector<uint8_t>& data, const Block& iv);
    std::vector<uint8_t> ctr_crypt(const AES& cipher, const std::vector<uint8_t>& data, Block counter);
};

#endif // MODES_A_HPP