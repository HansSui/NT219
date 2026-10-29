#ifndef MODES_D_HPP
#define MODES_D_HPP

#include "AES.hpp"
#include <vector>
#include <array>
#include <cstdint>

using Block = std::array<uint8_t, 16>;

class MODEd {
public:
    // GCM encryption
    // iv: 12 bytes recommended (96 bits), other sizes allowed
    // aad: additional authenticated data (can be empty)
    // tag_len: 8-16 bytes (64-128 bits), 12 bytes (96 bits) recommended
    std::vector<uint8_t> GCM_encrypt(const AES& cipher,
                                      const std::vector<uint8_t>& plaintext,
                                      const std::vector<uint8_t>& iv,
                                      const std::vector<uint8_t>& aad,
                                      size_t tag_len = 12);

    // GCM decryption
    // Throws std::runtime_error if authentication fails
    std::vector<uint8_t> GCM_decrypt(const AES& cipher,
                                      const std::vector<uint8_t>& ciphertext_with_tag,
                                      const std::vector<uint8_t>& iv,
                                      const std::vector<uint8_t>& aad,
                                      size_t tag_len = 12);

private:
    void validate_params(size_t iv_len, size_t tag_len);
    
    Block compute_tag(const AES& cipher, const Block& H,
                       const std::vector<uint8_t>& aad,
                       const std::vector<uint8_t>& ciphertext,
                       const Block& j0, size_t tag_len);
};

#endif // MODES_D_HPP