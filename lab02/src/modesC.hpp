#ifndef MODES_C_HPP
#define MODES_C_HPP

#include "AES.hpp"
#include <vector>
#include <array>
#include <cstdint>

using Block = std::array<uint8_t, 16>;

class MODEc {
public:
    // CCM encryption
    // nonce: 7-13 bytes (15-L bytes where L=2..8)
    // aad: additional authenticated data (can be empty)
    // tag_len: 4, 6, 8, 10, 12, 14, or 16 bytes
    std::vector<uint8_t> CCM_encrypt(const AES& cipher,
                                      const std::vector<uint8_t>& plaintext,
                                      const std::vector<uint8_t>& nonce,
                                      const std::vector<uint8_t>& aad,
                                      size_t tag_len = 16);

    // CCM decryption
    // Throws std::runtime_error if authentication fails
    std::vector<uint8_t> CCM_decrypt(const AES& cipher,
                                      const std::vector<uint8_t>& ciphertext_with_tag,
                                      const std::vector<uint8_t>& nonce,
                                      const std::vector<uint8_t>& aad,
                                      size_t tag_len = 16);

private:
    void validate_params(size_t nonce_len, size_t aad_len, size_t pt_len, size_t tag_len);
    
    Block build_B0(const std::vector<uint8_t>& nonce, size_t msg_len, size_t L, size_t tag_len, size_t aad_len);
    
    Block build_ctr0(const std::vector<uint8_t>& nonce, size_t L);
    
    Block build_ctri(const std::vector<uint8_t>& nonce, size_t L, uint64_t counter);
    
    std::vector<Block> build_aad_blocks(const std::vector<uint8_t>& aad);
    
    std::vector<Block> build_pt_blocks(const std::vector<uint8_t>& plaintext);
    
    std::vector<uint8_t> ctr_crypt(const AES& cipher, const std::vector<uint8_t>& data,
                                    const std::vector<uint8_t>& nonce, size_t L);
};

#endif // MODES_C_HPP