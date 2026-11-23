#ifndef AESNI_HPP
#define AESNI_HPP

#include <cstddef>
#include <cstdint>

namespace aesni {

// Runtime CPU check (x86 AES-NI); always false on other architectures.
bool available();

// AES-128 CTR over a 128-bit big-endian counter. The caller guarantees the
// counter does not wrap before the end of the payload. `round_keys` holds the
// 176-byte AES-128 schedule (11 round keys).
void ctr_crypt_128(const uint8_t* round_keys, uint8_t counter[16],
                   const uint8_t* in, uint8_t* out, size_t len);

} // namespace aesni

#endif // AESNI_HPP
