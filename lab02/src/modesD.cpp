#include "modesD.hpp"
#include "AES.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
using Block = std::array<uint8_t, 16>;

namespace GCM {

Block xor_blocks(const Block& a, const Block& b) {
    Block out;
    for (int i = 0; i < 16; ++i) out[i] = a[i] ^ b[i];
    return out;
}

void inc_ctr32(Block& ctr) {
    for (int i = 15; i >= 12; --i) {
        if (++ctr[i] != 0) break;
    }
}

Block gf_mul(const Block& x, const Block& y) {
    Block z{}; z.fill(0);
    Block v = x;
    
    for (int byte_idx = 0; byte_idx < 16; ++byte_idx) {
        uint8_t byte = y[byte_idx];
        for (int bit = 7; bit >= 0; --bit) {
            if (byte & (1 << bit)) {
                for (int i = 0; i < 16; ++i) z[i] ^= v[i];
            }
            
            bool lsb = v[15] & 0x01;
            for (int i = 15; i > 0; --i) {
                v[i] = (v[i] >> 1) | ((v[i-1] & 0x01) << 7);
            }
            v[0] >>= 1;
            if (lsb) v[0] ^= 0xE1;
        }
    }
    return z;
}

Block ghash(const Block& H, const std::vector<uint8_t>& aad, const std::vector<uint8_t>& ciphertext) {
    Block Y{}; Y.fill(0);
    
    size_t aad_blocks = (aad.size() + 15) / 16;
    for (size_t i = 0; i < aad_blocks; ++i) {
        Block block{};
        size_t len = std::min<size_t>(16, aad.size() - i * 16);
        std::copy(aad.begin() + i * 16, aad.begin() + i * 16 + len, block.begin());
        Y = xor_blocks(Y, block);
        Y = gf_mul(Y, H);
    }
    
    size_t ct_blocks = (ciphertext.size() + 15) / 16;
    for (size_t i = 0; i < ct_blocks; ++i) {
        Block block{};
        size_t len = std::min<size_t>(16, ciphertext.size() - i * 16);
        std::copy(ciphertext.begin() + i * 16, ciphertext.begin() + i * 16 + len, block.begin());
        Y = xor_blocks(Y, block);
        Y = gf_mul(Y, H);
    }
    
    Block len_block{};
    uint64_t aad_bits = static_cast<uint64_t>(aad.size()) * 8;
    uint64_t ct_bits = static_cast<uint64_t>(ciphertext.size()) * 8;
    
    for (int i = 0; i < 8; ++i) {
        len_block[i] = static_cast<uint8_t>(aad_bits >> (56 - 8 * i));
        len_block[8 + i] = static_cast<uint8_t>(ct_bits >> (56 - 8 * i));
    }
    
    Y = xor_blocks(Y, len_block);
    Y = gf_mul(Y, H);
    
    return Y;
}

Block build_j0(const AES& /*cipher*/, const std::vector<uint8_t>& iv, const Block& H) {
    if (iv.size() == 12) {
        Block j0{};
        std::copy(iv.begin(), iv.end(), j0.begin());
        j0[15] = 1;
        return j0;
    } else {
        std::vector<uint8_t> padded_iv = iv;
        size_t pad = (16 - (padded_iv.size() % 16)) % 16;
        padded_iv.insert(padded_iv.end(), pad, 0);
        
        Block Y = ghash(H, std::vector<uint8_t>(), padded_iv);
        
        Block len_block{};
        uint64_t iv_bits = static_cast<uint64_t>(iv.size()) * 8;
        for (int i = 0; i < 8; ++i) {
            len_block[8 + i] = static_cast<uint8_t>(iv_bits >> (56 - 8 * i));
        }
        Y = xor_blocks(Y, len_block);
        Y = gf_mul(Y, H);
        
        return Y;
    }
}

std::vector<uint8_t> gcm_ctr_crypt(const AES& cipher, const std::vector<uint8_t>& data, Block counter) {
    std::vector<uint8_t> out(data.size());
    size_t pos = 0;
    
    while (pos < data.size()) {
        Block keystream = cipher.encrypt(counter);
        inc_ctr32(counter);
        
        size_t chunk = std::min<size_t>(16, data.size() - pos);
        for (size_t i = 0; i < chunk; ++i) {
            out[pos + i] = data[pos + i] ^ keystream[i];
        }
        pos += chunk;
    }
    return out;
}

} // namespace GCM

// MODEd method implementations

std::vector<uint8_t> MODEd::GCM_encrypt(const AES& cipher,
                                         const std::vector<uint8_t>& plaintext,
                                         const std::vector<uint8_t>& iv,
                                         const std::vector<uint8_t>& aad,
                                         size_t tag_len) {
    validate_params(iv.size(), tag_len);
    
    Block zero{}; zero.fill(0);
    Block H = cipher.encrypt(zero);
    
    Block j0 = GCM::build_j0(cipher, iv, H);
    Block ctr = j0;
    
    GCM::inc_ctr32(ctr);
    std::vector<uint8_t> ciphertext = GCM::gcm_ctr_crypt(cipher, plaintext, ctr);
    
    Block tag = compute_tag(cipher, H, aad, ciphertext, j0, tag_len);
    
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.begin() + tag_len);
    
    return ciphertext;
}

std::vector<uint8_t> MODEd::GCM_decrypt(const AES& cipher,
                                         const std::vector<uint8_t>& ciphertext_with_tag,
                                         const std::vector<uint8_t>& iv,
                                         const std::vector<uint8_t>& aad,
                                         size_t tag_len) {
    validate_params(iv.size(), tag_len);
    
    if (ciphertext_with_tag.size() < tag_len) {
        throw std::runtime_error("Ciphertext too short for tag");
    }
    
    Block zero{}; zero.fill(0);
    Block H = cipher.encrypt(zero);
    
    Block j0 = GCM::build_j0(cipher, iv, H);
    
    size_t ct_len = ciphertext_with_tag.size() - tag_len;
    std::vector<uint8_t> ciphertext(ciphertext_with_tag.begin(), ciphertext_with_tag.begin() + ct_len);
    std::vector<uint8_t> received_tag(ciphertext_with_tag.begin() + ct_len, ciphertext_with_tag.end());
    
    Block computed_tag = compute_tag(cipher, H, aad, ciphertext, j0, tag_len);
    
    bool valid = true;
    for (size_t i = 0; i < tag_len; ++i) {
        if (computed_tag[i] != received_tag[i]) valid = false;
    }
    
    if (!valid) {
        throw std::runtime_error("GCM authentication failed");
    }
    
    Block ctr = j0;
    GCM::inc_ctr32(ctr);
    std::vector<uint8_t> plaintext = GCM::gcm_ctr_crypt(cipher, ciphertext, ctr);
    
    return plaintext;
}

void MODEd::validate_params(size_t iv_len, size_t tag_len) {
    if (tag_len < 8 || tag_len > 16) {
        throw std::invalid_argument("Tag length must be 8-16 bytes (64-128 bits)");
    }
    if (iv_len == 0) {
        throw std::invalid_argument("IV must not be empty");
    }
}

Block MODEd::compute_tag(const AES& cipher, const Block& H,
                          const std::vector<uint8_t>& aad,
                          const std::vector<uint8_t>& ciphertext,
                          const Block& j0, size_t /*tag_len*/) {
    Block S = GCM::ghash(H, aad, ciphertext);
    
    Block e_j0 = cipher.encrypt(j0);
    Block tag = GCM::xor_blocks(S, e_j0);
    
    return tag;
}