#include "modesA.hpp"
#include "AES.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
using Block = std::array<uint8_t, 16>;

namespace MODEa_NS {

Block xor_blocks(const Block& a, const Block& b) {
    Block out;
    for (int i = 0; i < 16; ++i) out[i] = a[i] ^ b[i];
    return out;
}

Block xor_blocks_partial(const Block& a, const uint8_t* b, size_t len) {
    Block out = a;
    for (size_t i = 0; i < len; ++i) out[i] ^= b[i];
    return out;
}

void xor_into(uint8_t* out, const uint8_t* a, const uint8_t* b, size_t len) {
    for (size_t i = 0; i < len; ++i) out[i] = a[i] ^ b[i];
}

// PKCS#7 padding (for ECB/CBC)
std::vector<uint8_t> pkcs7_pad(const std::vector<uint8_t>& data) {
    size_t pad = 16 - (data.size() % 16);
    std::vector<uint8_t> out = data;
    out.insert(out.end(), pad, static_cast<uint8_t>(pad));
    return out;
}

std::vector<uint8_t> pkcs7_unpad(const std::vector<uint8_t>& data) {
    if (data.empty()) throw std::runtime_error("Empty data");
    uint8_t pad = data.back();
    if (pad == 0 || pad > 16) throw std::runtime_error("Invalid padding");
    for (size_t i = 1; i <= pad; ++i) {
        if (data[data.size() - i] != pad) throw std::runtime_error("Invalid padding");
    }
    return std::vector<uint8_t>(data.begin(), data.end() - pad);
}

} // namespace MODEa_NS

// MODEa method implementations

std::vector<uint8_t> MODEa::ECB_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext){
    auto pad = MODEa_NS::pkcs7_pad(plaintext);
    std::vector<uint8_t> out(pad.size());
    for (size_t i = 0; i < pad.size(); i += 16){
        Block block;
        std::copy(pad.begin() + i, pad.begin() + i + 16, block.begin());
        Block enc = cipher.encrypt(block);
        std::copy(enc.begin(), enc.end(), out.begin() + i);
    }
    return out;
}

std::vector<uint8_t> MODEa::ECB_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext){
    if (ciphertext.size() % 16 != 0) throw std::runtime_error("invalid length");
    std::vector<uint8_t> out(ciphertext.size());
    for (size_t i = 0; i < ciphertext.size(); i += 16){
        Block block;
        std::copy(ciphertext.begin() + i, ciphertext.begin() + i + 16, block.begin());
        Block dec = cipher.decrypt(block);
        std::copy(dec.begin(), dec.end(), out.begin() + i);
    }
    return MODEa_NS::pkcs7_unpad(out);
}

std::vector<uint8_t> MODEa::CBC_encrypt(const AES& cipher, const std::vector<uint8_t> plaintext, const Block& iv){
    auto pad = MODEa_NS::pkcs7_pad(plaintext);
    Block prev = iv;
    std::vector<uint8_t> out(pad.size());
    for (size_t i = 0; i < pad.size(); i += 16){
        Block block;
        std::copy(pad.begin() + i, pad.begin() + i + 16, block.begin());
        Block xored = MODEa_NS::xor_blocks(prev, block);
        Block enc = cipher.encrypt(xored);
        prev = enc;
        std::copy(enc.begin(), enc.end(), out.begin() + i);
    }
    return out;
}

std::vector<uint8_t> MODEa::CBC_decrypt(const AES& cipher, const std::vector<uint8_t> ciphertext, const Block& iv){
    if (ciphertext.size() % 16 != 0) throw std::runtime_error("invalid length");
    std::vector<uint8_t> out(ciphertext.size());
    Block prev = iv;
    for (size_t i = 0; i < ciphertext.size(); i += 16){
        Block block;
        std::copy(ciphertext.begin() + i, ciphertext.begin() + i + 16, block.begin());
        Block dec = cipher.decrypt(block);
        Block plain = MODEa_NS::xor_blocks(prev, dec);
        prev = block;
        std::copy(plain.begin(), plain.end(), out.begin() + i);
    }
    return MODEa_NS::pkcs7_unpad(out);
}

void MODEa::incr32(Block& counter){
    for (int i = 15; i >= 12; --i){
        if (++counter[i] != 0) break;
    }
}

inline void MODEa::inc64(Block& ctr) {
    for (int i = 15; i >= 8; --i) if (++ctr[i] != 0) break;
}

inline void MODEa::inc128(Block& ctr) {
    for (int i = 15; i >= 0; --i) if (++ctr[i] != 0) break;
}

Block MODEa::msb(const Block& x, size_t bits) {
    Block out{}; out.fill(0);
    size_t bytes = bits / 8;
    size_t rem   = bits % 8;
    if (bytes > 0) std::copy(x.begin(), x.begin() + bytes, out.begin());
    if (rem > 0)   out[bytes] = x[bytes] & (0xFF << (8 - rem));
    return out;
}

Block MODEa::lsb(const Block& x, size_t bits) {
    Block out{}; out.fill(0);
    size_t bytes = bits / 8;
    size_t rem   = bits % 8;
    if (rem > 0) {
        out[15 - bytes] = x[15 - bytes] & (0xFF >> (8 - rem));
        if (bytes > 0) std::copy(x.end() - bytes, x.end(), out.end() - bytes);
    } else if (bytes > 0) {
        std::copy(x.end() - bytes, x.end(), out.end() - bytes);
    }
    return out;
}

void MODEa::shift_left_append(Block& reg, const Block& segment, size_t bits) {
    size_t bytes = bits / 8;
    size_t rem   = bits % 8;

    if (bits == 128) {
        reg = segment;
        return;
    }

    if (bits == 8) {
        for (int i = 0; i < 15; ++i) reg[i] = reg[i + 1];
        reg[15] = segment[15];
        return;
    }

    if (bits == 1) {
        uint8_t carry = 0;
        for (int i = 0; i < 16; ++i) {
            uint8_t new_carry = reg[i] >> 7;
            reg[i] = (reg[i] << 1) | carry;
            carry = new_carry;
        }
        reg[15] |= static_cast<uint8_t>(segment[15] & 0x01);
        return;
    }

    Block shifted{};
    for (size_t i = 0; i < 16; ++i) {
        size_t src = i + bytes;
        if (src < 16) shifted[i] = reg[src] << rem;
        if (src + 1 < 16 && rem > 0) shifted[i] |= static_cast<uint8_t>(reg[src + 1] >> (8 - rem));
    }
    for (size_t i = 0; i < bytes; ++i) {
        shifted[16 - bytes + i] = segment[16 - bytes + i];
    }
    if (rem > 0) {
        shifted[16 - bytes - 1] |= static_cast<uint8_t>(segment[16 - bytes - 1] & (0xFF >> (8 - rem)));
    }
    reg = shifted;
}

std::vector<uint8_t> MODEa::cfb_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, const Block& iv, int segment_bits){
    if (segment_bits != 1 && segment_bits != 8 && segment_bits != 128){
        throw std::runtime_error("CFB segment_bits should be 1, 8, or 128");
    }
    std::vector<uint8_t> ciphertext(plaintext.size());
    Block prev = iv;
    size_t pos = 0;
    while (pos < plaintext.size()){
        Block keystream = cipher.encrypt(prev);
        Block s_msb = msb(keystream, segment_bits);
        size_t seg_bytes = (segment_bits + 7) / 8;
        size_t chunk = std::min(seg_bytes, plaintext.size() - pos);
        MODEa_NS::xor_into(ciphertext.data() + pos, plaintext.data() + pos, s_msb.data(), chunk);
        Block c_segment{};
        std::copy(ciphertext.begin() + pos, ciphertext.begin() + pos + chunk, c_segment.end() - chunk);
        shift_left_append(prev, c_segment, segment_bits);
        pos += chunk;
    }
    return ciphertext;
}

std::vector<uint8_t> MODEa::cfb_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, const Block& iv, int segment_bits){
    if (segment_bits != 1 && segment_bits != 8 && segment_bits != 128){
        throw std::runtime_error("CFB segment_bits should be 1, 8, or 128");
    }
    std::vector<uint8_t> plaintext(ciphertext.size());
    Block prev = iv;
    size_t pos = 0;
    while (pos < ciphertext.size()){
        Block keystream = cipher.encrypt(prev);
        Block s_msb = msb(keystream, segment_bits);
        size_t seg_bytes = (segment_bits + 7) / 8;
        size_t chunk = std::min(seg_bytes, ciphertext.size() - pos);
        MODEa_NS::xor_into(plaintext.data() + pos, ciphertext.data() + pos, s_msb.data(), chunk);
        Block c_segment{};
        std::copy(ciphertext.begin() + pos, ciphertext.begin() + pos + chunk, c_segment.end() - chunk);
        shift_left_append(prev, c_segment, segment_bits);
        pos += chunk;
    }
    return plaintext;
}

std::vector<uint8_t> MODEa::ofb_crypt(const AES& cipher, const std::vector<uint8_t>& data, const Block& iv) {
    std::vector<uint8_t> out(data.size());
    Block state = iv;
    size_t pos = 0;

    while (pos < data.size()) {
        state = cipher.encrypt(state);
        size_t chunk = std::min<size_t>(16, data.size() - pos);
        MODEa_NS::xor_into(out.data() + pos, data.data() + pos, state.data(), chunk);
        pos += chunk;
    }
    return out;
}

std::vector<uint8_t> MODEa::ofb_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, const Block& iv) {
    return ofb_crypt(cipher, plaintext, iv);
}

std::vector<uint8_t> MODEa::ofb_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, const Block& iv) {
    return ofb_crypt(cipher, ciphertext, iv);
}

std::vector<uint8_t> MODEa::ctr_crypt(const AES& cipher, const std::vector<uint8_t>& data, Block counter) {
    std::vector<uint8_t> out(data.size());
    size_t pos = 0;

    while (pos < data.size()) {
        Block keystream = cipher.encrypt(counter);
        inc128(counter);

        size_t chunk = std::min<size_t>(16, data.size() - pos);
        MODEa_NS::xor_into(out.data() + pos, data.data() + pos, keystream.data(), chunk);

        pos += chunk;
    }
    return out;
}

std::vector<uint8_t> MODEa::ctr_encrypt(const AES& cipher, const std::vector<uint8_t>& plaintext, Block counter) {
    return ctr_crypt(cipher, plaintext, counter);
}

std::vector<uint8_t> MODEa::ctr_decrypt(const AES& cipher, const std::vector<uint8_t>& ciphertext, Block counter) {
    return ctr_crypt(cipher, ciphertext, counter);
}