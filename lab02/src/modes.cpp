#include "modes.hpp"
#include <algorithm>
#include <stdexcept>

namespace {

// Big-endian 128-bit increment; returns true on 2^128 wrap-around.
bool inc128(Block& ctr) {
    for (int i = 15; i >= 0; --i) {
        if (++ctr[i] != 0) return false;
    }
    return true;
}

std::vector<uint8_t> ctr_crypt(const AES& cipher, const std::vector<uint8_t>& data, Block counter) {
    std::vector<uint8_t> out(data.size());

    for (size_t pos = 0; pos < data.size(); pos += 16) {
        Block keystream = cipher.encrypt(counter);
        bool wrapped = inc128(counter);

        size_t chunk = std::min<size_t>(16, data.size() - pos);
        for (size_t i = 0; i < chunk; ++i) out[pos + i] = data[pos + i] ^ keystream[i];

        // Wrapping reuses counter 0 under the same key: catastrophic keystream reuse.
        if (wrapped && pos + chunk < data.size()) {
            throw std::runtime_error("CTR counter overflow: refusing keystream reuse");
        }
    }
    return out;
}

Block load_block(const uint8_t* p) {
    Block block;
    std::copy(p, p + 16, block.begin());
    return block;
}

void store_block(uint8_t* p, const Block& block) {
    std::copy(block.begin(), block.end(), p);
}

Block xor_block(const Block& a, const Block& b) {
    Block out;
    for (size_t i = 0; i < 16; ++i) out[i] = static_cast<uint8_t>(a[i] ^ b[i]);
    return out;
}

// Multiply the little-endian 128-bit tweak by alpha in GF(2^128),
// reduction polynomial x^128 + x^7 + x^2 + x + 1.
void xts_mul_alpha(Block& tweak) {
    uint8_t carry = 0;
    for (size_t i = 0; i < 16; ++i) {
        uint8_t next = static_cast<uint8_t>(tweak[i] >> 7);
        tweak[i] = static_cast<uint8_t>((tweak[i] << 1) | carry);
        carry = next;
    }
    if (carry) tweak[0] ^= 0x87;
}

std::vector<uint8_t> xts_crypt(const AES& data_cipher, const AES& tweak_cipher,
                               const std::vector<uint8_t>& in, const Block& data_unit,
                               bool encrypt) {
    if (in.size() < 16) {
        throw std::invalid_argument("XTS requires at least one 16-byte block");
    }

    std::vector<uint8_t> out(in.size());
    Block tweak = tweak_cipher.encrypt(data_unit);

    size_t full_blocks = in.size() / 16;
    size_t remainder = in.size() % 16;
    size_t normal_blocks = (remainder == 0) ? full_blocks : full_blocks - 1;

    for (size_t i = 0; i < normal_blocks; ++i) {
        Block block = xor_block(load_block(in.data() + 16 * i), tweak);
        block = encrypt ? data_cipher.encrypt(block) : data_cipher.decrypt(block);
        store_block(out.data() + 16 * i, xor_block(block, tweak));
        xts_mul_alpha(tweak);
    }

    if (remainder == 0) return out;

    // Ciphertext stealing (IEEE P1619): tweak is T[n-2], next is T[n-1].
    Block next_tweak = tweak;
    xts_mul_alpha(next_tweak);
    size_t last_full = 16 * (full_blocks - 1);
    size_t partial = 16 * full_blocks;

    if (encrypt) {
        Block stolen = xor_block(data_cipher.encrypt(xor_block(load_block(in.data() + last_full), tweak)), tweak);
        for (size_t i = 0; i < remainder; ++i) out[partial + i] = stolen[i];

        Block merged{};
        for (size_t i = 0; i < remainder; ++i) merged[i] = in[partial + i];
        for (size_t i = remainder; i < 16; ++i) merged[i] = stolen[i];

        Block last = xor_block(data_cipher.encrypt(xor_block(merged, next_tweak)), next_tweak);
        store_block(out.data() + last_full, last);
    } else {
        Block merged = xor_block(data_cipher.decrypt(xor_block(load_block(in.data() + last_full), next_tweak)), next_tweak);
        for (size_t i = 0; i < remainder; ++i) out[partial + i] = merged[i];

        Block stolen{};
        for (size_t i = 0; i < remainder; ++i) stolen[i] = in[partial + i];
        for (size_t i = remainder; i < 16; ++i) stolen[i] = merged[i];

        Block last = xor_block(data_cipher.decrypt(xor_block(stolen, tweak)), tweak);
        store_block(out.data() + last_full, last);
    }
    return out;
}

} // namespace

std::vector<uint8_t> Modes::ctr_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, Block counter) {
    return ctr_crypt(cipher, plaintext, counter);
}

std::vector<uint8_t> Modes::ctr_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, Block counter) {
    return ctr_crypt(cipher, ciphertext, counter);
}

std::vector<uint8_t> Modes::xts_encrypt(const AES& key1, const AES& key2,
                                        const std::vector<uint8_t>& plaintext, const Block& data_unit) {
    return xts_crypt(key1, key2, plaintext, data_unit, true);
}

std::vector<uint8_t> Modes::xts_decrypt(const AES& key1, const AES& key2,
                                        const std::vector<uint8_t>& ciphertext, const Block& data_unit) {
    return xts_crypt(key1, key2, ciphertext, data_unit, false);
}
