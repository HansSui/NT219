#include "AES.hpp"
#include <stdexcept>
#include <algorithm>

AES::AES(const std::vector<uint8_t>& key, KeyLength key_length)
    : key_(key), key_length_(key_length) {
    validate_key();
    round_keys_ = key_expansion(key_, key_length_);
}

AES::AES(const uint8_t* key, size_t key_size, KeyLength key_length)
    : key_(key, key + key_size), key_length_(key_length) {
    validate_key();
    round_keys_ = key_expansion(key_, key_length_);
}

void AES::validate_key() const {
    size_t expected = key_bytes(key_length_);
    if (key_.size() != expected) {
        throw std::invalid_argument("Invalid key size for selected AES variant");
    }
}

uint8_t AES::gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) p ^= a;
        bool hi_bit = a & 0x80;
        a <<= 1;
        if (hi_bit) a ^= 0x1b; // x^8 + x^4 + x^3 + x + 1
        b >>= 1;
    }
    return p;
}

AES::State AES::bytes_to_state(const std::array<uint8_t, BLOCK_SIZE>& block) {
    State state{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            state[c][r] = block[c * 4 + r];
        }
    }
    return state;
}

std::array<uint8_t, AES::BLOCK_SIZE> AES::state_to_bytes(const State& state) {
    std::array<uint8_t, BLOCK_SIZE> block{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            block[c * 4 + r] = state[c][r];
        }
    }
    return block;
}

void AES::sub_bytes(State& state) const {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            state[c][r] = S_BOX[state[c][r]];
        }
    }
}

void AES::shift_rows(State& state) const {
    // Row 1: shift 1
    std::swap(state[1][1], state[2][1]);
    std::swap(state[2][1], state[3][1]);
    std::swap(state[3][1], state[0][1]);
    
    // Row 2: shift 2
    std::swap(state[0][2], state[2][2]);
    std::swap(state[1][2], state[3][2]);
    
    // Row 3: shift 3 (equiv to shift 1 right)
    std::swap(state[0][3], state[3][3]);
    std::swap(state[3][3], state[2][3]);
    std::swap(state[2][3], state[1][3]);
}

void AES::mix_columns(State& state) const {
    for (int c = 0; c < 4; ++c) {
        uint8_t a0 = state[c][0];
        uint8_t a1 = state[c][1];
        uint8_t a2 = state[c][2];
        uint8_t a3 = state[c][3];
        
        state[c][0] = gmul(0x02, a0) ^ gmul(0x03, a1) ^ a2 ^ a3;
        state[c][1] = a0 ^ gmul(0x02, a1) ^ gmul(0x03, a2) ^ a3;
        state[c][2] = a0 ^ a1 ^ gmul(0x02, a2) ^ gmul(0x03, a3);
        state[c][3] = gmul(0x03, a0) ^ a1 ^ a2 ^ gmul(0x02, a3);
    }
}

void AES::add_round_key(State& state, size_t round) const {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            state[c][r] ^= round_keys_[round * 4 + c][r];
        }
    }
}

void AES::inv_sub_bytes(State& state) const {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            state[c][r] = INV_S_BOX[state[c][r]];
        }
    }
}

void AES::inv_shift_rows(State& state) const {
    // Row 1: shift 1 right (equiv to shift 3 left)
    std::swap(state[0][1], state[3][1]);
    std::swap(state[3][1], state[2][1]);
    std::swap(state[2][1], state[1][1]);
    
    // Row 2: shift 2
    std::swap(state[0][2], state[2][2]);
    std::swap(state[1][2], state[3][2]);
    
    // Row 3: shift 1 left
    std::swap(state[0][3], state[1][3]);
    std::swap(state[1][3], state[2][3]);
    std::swap(state[2][3], state[3][3]);
}

void AES::inv_mix_columns(State& state) const {
    for (int c = 0; c < 4; ++c) {
        uint8_t a0 = state[c][0];
        uint8_t a1 = state[c][1];
        uint8_t a2 = state[c][2];
        uint8_t a3 = state[c][3];
        
        state[c][0] = gmul(0x0e, a0) ^ gmul(0x0b, a1) ^ gmul(0x0d, a2) ^ gmul(0x09, a3);
        state[c][1] = gmul(0x09, a0) ^ gmul(0x0e, a1) ^ gmul(0x0b, a2) ^ gmul(0x0d, a3);
        state[c][2] = gmul(0x0d, a0) ^ gmul(0x09, a1) ^ gmul(0x0e, a2) ^ gmul(0x0b, a3);
        state[c][3] = gmul(0x0b, a0) ^ gmul(0x0d, a1) ^ gmul(0x09, a2) ^ gmul(0x0e, a3);
    }
}

std::array<uint8_t, 4> AES::sub_word(const std::array<uint8_t, 4>& word) {
    return { S_BOX[word[0]], S_BOX[word[1]], S_BOX[word[2]], S_BOX[word[3]] };
}

std::array<uint8_t, 4> AES::rot_word(const std::array<uint8_t, 4>& word) {
    return { word[1], word[2], word[3], word[0] };
}

std::array<uint8_t, 4> AES::xor_words(const std::array<uint8_t, 4>& a, const std::array<uint8_t, 4>& b) {
    return { 
        static_cast<uint8_t>(a[0] ^ b[0]),
        static_cast<uint8_t>(a[1] ^ b[1]),
        static_cast<uint8_t>(a[2] ^ b[2]),
        static_cast<uint8_t>(a[3] ^ b[3])
    };
}

std::vector<std::array<uint8_t, 4>> AES::key_expansion(const std::vector<uint8_t>& key, KeyLength length) const {
    size_t nk = key_bytes(length) / 4;  // 4, 6, or 8
    size_t nr = num_rounds_for_key_length(length); // 10, 12, or 14
    size_t nb = 4; // block size in words (always 4 for AES)
    size_t total_words = nb * (nr + 1);
    
    std::vector<std::array<uint8_t, 4>> w(total_words);
    
    // First nk words are the key
    for (size_t i = 0; i < nk; ++i) {
        w[i] = { key[4*i], key[4*i+1], key[4*i+2], key[4*i+3] };
    }
    
    // Generate remaining words
    for (size_t i = nk; i < total_words; ++i) {
        std::array<uint8_t, 4> temp = w[i - 1];
        
        if (i % nk == 0) {
            temp = rot_word(temp);
            temp = sub_word(temp);
            temp[0] ^= RCON[(i / nk) - 1][0];
        } else if (nk > 6 && i % nk == 4) { // AES-256 extra subword
            temp = sub_word(temp);
        }
        
        w[i] = xor_words(w[i - nk], temp);
    }
    
    return w;
}

std::array<uint8_t, AES::BLOCK_SIZE> AES::encrypt(const std::array<uint8_t, BLOCK_SIZE>& data) const {
    State state = bytes_to_state(data);
    size_t nr = num_rounds_for_key_length(key_length_);
    
    add_round_key(state, 0);
    
    for (size_t round = 1; round < nr; ++round) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, round);
    }
    
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, nr);
    
    return state_to_bytes(state);
}

std::array<uint8_t, AES::BLOCK_SIZE> AES::decrypt(const std::array<uint8_t, BLOCK_SIZE>& ciphertext) const {
    State state = bytes_to_state(ciphertext);
    size_t nr = num_rounds_for_key_length(key_length_);
    
    add_round_key(state, nr);
    
    for (size_t round = nr; round > 1; --round) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, round - 1);
        inv_mix_columns(state);
    }
    
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, 0);
    
    return state_to_bytes(state);
}