#include "modesC.hpp"
#include "AES.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
using Block = std::array<uint8_t, 16>;

namespace CCM {

Block xor_blocks(const Block& a, const Block& b) {
    Block out;
    for (int i = 0; i < 16; ++i) out[i] = a[i] ^ b[i];
    return out;
}

void inc_counter(Block& ctr) {
    for (int i = 15; i >= 0; --i) {
        if (++ctr[i] != 0) break;
    }
}

Block aes_cmac(const AES& cipher, const std::vector<uint8_t>& data) {
    Block zero{}; zero.fill(0);
    Block L = cipher.encrypt(zero);
    
    Block K1 = L;
    bool msb = L[0] & 0x80;
    for (int i = 0; i < 15; ++i) {
        K1[i] = (L[i] << 1) | (L[i+1] >> 7);
    }
    K1[15] = (L[15] << 1) ^ (msb ? 0x87 : 0x00);
    
    Block K2 = K1;
    msb = K1[0] & 0x80;
    for (int i = 0; i < 15; ++i) {
        K2[i] = (K1[i] << 1) | (K1[i+1] >> 7);
    }
    K2[15] = (K1[15] << 1) ^ (msb ? 0x87 : 0x00);
    
    Block X{}; X.fill(0);
    size_t n_blocks = (data.size() + 15) / 16;
    bool last_full = false;
    
    for (size_t i = 0; i < n_blocks; ++i) {
        Block block{};
        size_t remaining = data.size() - i * 16;
        size_t len = std::min<size_t>(16, remaining);
        std::copy(data.begin() + i * 16, data.begin() + i * 16 + len, block.begin());
        
        bool last_block = (i == n_blocks - 1);
        last_full = (len == 16);
        
        if (last_block && last_full) {
            block = xor_blocks(block, K1);
        } else if (last_block && !last_full) {
            block[len] = 0x80;
            block = xor_blocks(block, K2);
        }
        
        X = xor_blocks(X, block);
        if (i != n_blocks - 1 || (last_block && last_full)) {
            X = cipher.encrypt(X);
        }
    }
    
    if (n_blocks > 0 && !last_full) {
        X = cipher.encrypt(X);
    }
    
    return X;
}

} // namespace CCM

// MODEc method implementations

std::vector<uint8_t> MODEc::CCM_encrypt(const AES& cipher,
                                         const std::vector<uint8_t>& plaintext,
                                         const std::vector<uint8_t>& nonce,
                                         const std::vector<uint8_t>& aad,
                                         size_t tag_len) {
    validate_params(nonce.size(), aad.size(), plaintext.size(), tag_len);
    
    size_t L = 15 - nonce.size();
    
    Block B0 = build_B0(nonce, plaintext.size(), L, tag_len, aad.size());
    
    std::vector<Block> auth_blocks;
    auth_blocks.push_back(B0);
    
    if (!aad.empty()) {
        auto aad_blocks = build_aad_blocks(aad);
        auth_blocks.insert(auth_blocks.end(), aad_blocks.begin(), aad_blocks.end());
    }
    
    auto pt_blocks = build_pt_blocks(plaintext);
    auth_blocks.insert(auth_blocks.end(), pt_blocks.begin(), pt_blocks.end());
    
    Block X{}; X.fill(0);
    for (const auto& blk : auth_blocks) {
        X = CCM::xor_blocks(X, blk);
        X = cipher.encrypt(X);
    }
    
    Block ctr0 = build_ctr0(nonce, L);
    Block tag_full = cipher.encrypt(ctr0);
    Block tag = CCM::xor_blocks(tag_full, X);
    
    std::vector<uint8_t> tag_truncated(tag.begin(), tag.begin() + tag_len);
    
    std::vector<uint8_t> ciphertext = ctr_crypt(cipher, plaintext, nonce, L);
    
    ciphertext.insert(ciphertext.end(), tag_truncated.begin(), tag_truncated.end());
    
    return ciphertext;
}

std::vector<uint8_t> MODEc::CCM_decrypt(const AES& cipher,
                                         const std::vector<uint8_t>& ciphertext_with_tag,
                                         const std::vector<uint8_t>& nonce,
                                         const std::vector<uint8_t>& aad,
                                         size_t tag_len) {
    validate_params(nonce.size(), aad.size(), 
                    (ciphertext_with_tag.size() >= tag_len ? ciphertext_with_tag.size() - tag_len : 0), 
                    tag_len);
    
    if (ciphertext_with_tag.size() < tag_len) {
        throw std::runtime_error("Ciphertext too short for tag");
    }
    
    size_t L = 15 - nonce.size();
    
    size_t ct_len = ciphertext_with_tag.size() - tag_len;
    std::vector<uint8_t> ciphertext(ciphertext_with_tag.begin(), ciphertext_with_tag.begin() + ct_len);
    std::vector<uint8_t> received_tag(ciphertext_with_tag.begin() + ct_len, ciphertext_with_tag.end());
    
    std::vector<uint8_t> plaintext = ctr_crypt(cipher, ciphertext, nonce, L);
    
    Block B0 = build_B0(nonce, plaintext.size(), L, tag_len, aad.size());
    
    std::vector<Block> auth_blocks;
    auth_blocks.push_back(B0);
    
    if (!aad.empty()) {
        auto aad_blocks = build_aad_blocks(aad);
        auth_blocks.insert(auth_blocks.end(), aad_blocks.begin(), aad_blocks.end());
    }
    
    auto pt_blocks = build_pt_blocks(plaintext);
    auth_blocks.insert(auth_blocks.end(), pt_blocks.begin(), pt_blocks.end());
    
    Block X{}; X.fill(0);
    for (const auto& blk : auth_blocks) {
        X = CCM::xor_blocks(X, blk);
        X = cipher.encrypt(X);
    }
    
    Block ctr0 = build_ctr0(nonce, L);
    Block tag_full = cipher.encrypt(ctr0);
    Block computed_tag = CCM::xor_blocks(tag_full, X);
    
    bool valid = true;
    for (size_t i = 0; i < tag_len; ++i) {
        if (computed_tag[i] != received_tag[i]) valid = false;
    }
    
    if (!valid) {
        throw std::runtime_error("CCM authentication failed");
    }
    
    return plaintext;
}

void MODEc::validate_params(size_t nonce_len, size_t /*aad_len*/, size_t pt_len, size_t tag_len) {
    if (nonce_len < 7 || nonce_len > 13) {
        throw std::invalid_argument("Nonce must be 7-13 bytes");
    }
    if (tag_len < 4 || tag_len > 16 || tag_len % 2 != 0) {
        throw std::invalid_argument("Tag length must be 4, 6, 8, 10, 12, 14, or 16");
    }
    size_t L = 15 - nonce_len;
    size_t max_msg_len = (L >= 8) ? SIZE_MAX : ((size_t)1 << (8 * L)) - 1;
    if (pt_len > max_msg_len) {
        throw std::invalid_argument("Plaintext too long for nonce size");
    }
}

Block MODEc::build_B0(const std::vector<uint8_t>& nonce, size_t msg_len, size_t L, size_t tag_len, size_t aad_len) {
    Block B0{};
    uint8_t flags = 0;
    if (aad_len > 0) flags |= 0x40;
    flags |= static_cast<uint8_t>(((tag_len - 2) / 2) << 3);
    flags |= static_cast<uint8_t>(L - 1);
    B0[0] = flags;
    
    std::copy(nonce.begin(), nonce.end(), B0.begin() + 1);
    
    for (size_t i = 0; i < L; ++i) {
        B0[16 - L + i] = static_cast<uint8_t>((msg_len >> (8 * (L - 1 - i))) & 0xFF);
    }
    
    return B0;
}

Block MODEc::build_ctr0(const std::vector<uint8_t>& nonce, size_t L) {
    Block ctr0{};
    ctr0[0] = static_cast<uint8_t>(L - 1);
    std::copy(nonce.begin(), nonce.end(), ctr0.begin() + 1);
    return ctr0;
}

Block MODEc::build_ctri(const std::vector<uint8_t>& nonce, size_t L, uint64_t counter) {
    Block ctr = build_ctr0(nonce, L);
    for (size_t i = 0; i < L; ++i) {
        ctr[16 - L + i] = static_cast<uint8_t>((counter >> (8 * (L - 1 - i))) & 0xFF);
    }
    return ctr;
}

std::vector<Block> MODEc::build_aad_blocks(const std::vector<uint8_t>& aad) {
    std::vector<Block> blocks;
    
    std::vector<uint8_t> aad_len_encoded;
    size_t aad_len = aad.size();
    if (aad_len < 0xFF00) {
        aad_len_encoded.push_back(static_cast<uint8_t>(aad_len >> 8));
        aad_len_encoded.push_back(static_cast<uint8_t>(aad_len & 0xFF));
    } else if (aad_len <= 0xFFFFFFFF) {
        aad_len_encoded.push_back(0xFF);
        aad_len_encoded.push_back(0xFE);
        aad_len_encoded.push_back(static_cast<uint8_t>(aad_len >> 24));
        aad_len_encoded.push_back(static_cast<uint8_t>((aad_len >> 16) & 0xFF));
        aad_len_encoded.push_back(static_cast<uint8_t>((aad_len >> 8) & 0xFF));
        aad_len_encoded.push_back(static_cast<uint8_t>(aad_len & 0xFF));
    } else {
        aad_len_encoded.push_back(0xFF);
        aad_len_encoded.push_back(0xFF);
        for (int i = 7; i >= 0; --i) {
            aad_len_encoded.push_back(static_cast<uint8_t>((aad_len >> (8 * i)) & 0xFF));
        }
    }
    
    std::vector<uint8_t> aad_data = aad_len_encoded;
    aad_data.insert(aad_data.end(), aad.begin(), aad.end());
    
    size_t padding = (16 - (aad_data.size() % 16)) % 16;
    aad_data.insert(aad_data.end(), padding, 0);
    
    for (size_t i = 0; i < aad_data.size(); i += 16) {
        Block blk;
        std::copy(aad_data.begin() + i, aad_data.begin() + i + 16, blk.begin());
        blocks.push_back(blk);
    }
    
    return blocks;
}

std::vector<Block> MODEc::build_pt_blocks(const std::vector<uint8_t>& plaintext) {
    std::vector<Block> blocks;
    std::vector<uint8_t> pt_padded = plaintext;
    size_t padding = (16 - (pt_padded.size() % 16)) % 16;
    pt_padded.insert(pt_padded.end(), padding, 0);
    
    for (size_t i = 0; i < pt_padded.size(); i += 16) {
        Block blk;
        std::copy(pt_padded.begin() + i, pt_padded.begin() + i + 16, blk.begin());
        blocks.push_back(blk);
    }
    
    return blocks;
}

std::vector<uint8_t> MODEc::ctr_crypt(const AES& cipher, const std::vector<uint8_t>& data,
                                       const std::vector<uint8_t>& nonce, size_t L) {
    std::vector<uint8_t> out(data.size());
    size_t pos = 0;
    uint64_t counter = 1;
    
    while (pos < data.size()) {
        Block ctr = build_ctri(nonce, L, counter);
        Block keystream = cipher.encrypt(ctr);
        
        size_t chunk = std::min<size_t>(16, data.size() - pos);
        for (size_t i = 0; i < chunk; ++i) {
            out[pos + i] = data[pos + i] ^ keystream[i];
        }
        
        pos += chunk;
        counter++;
    }
    
    return out;
}