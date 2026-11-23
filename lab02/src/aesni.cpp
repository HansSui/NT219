#include "aesni.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

// AES-NI is x86-only; everything else compiles to a stub so the core library
// stays portable and the caller can fall back to table-based AES.
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define AESNI_X86 1
#include <wmmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#if defined(__GNUC__) || defined(__clang__)
#define AESNI_FN __attribute__((target("aes,sse2")))
#else
#define AESNI_FN
#endif
#endif

namespace aesni {

bool available() {
#if defined(AESNI_X86)
    static const bool ok = [] {
#if defined(_MSC_VER)
        int regs[4];
        __cpuid(regs, 1);
        return (regs[2] & (1 << 25)) != 0;
#else
        __builtin_cpu_init();
        return __builtin_cpu_supports("aes") != 0;
#endif
    }();
    return ok;
#else
    return false;
#endif
}

#if defined(AESNI_X86)

namespace {

void inc128(uint8_t* counter) {
    for (int i = 15; i >= 0; --i) {
        if (++counter[i] != 0) return;
    }
}

AESNI_FN __m128i encrypt_block(__m128i x, const __m128i* round_keys) {
    x = _mm_xor_si128(x, round_keys[0]);
    for (int round = 1; round < 10; ++round) {
        x = _mm_aesenc_si128(x, round_keys[round]);
    }
    return _mm_aesenclast_si128(x, round_keys[10]);
}

} // namespace

void AESNI_FN ctr_crypt_128(const uint8_t* round_keys, uint8_t counter[16],
                            const uint8_t* in, uint8_t* out, size_t len) {
    __m128i rk[11];
    for (int round = 0; round <= 10; ++round) {
        rk[round] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(round_keys + 16 * round));
    }

    size_t pos = 0;
    // Four independent counter blocks per iteration keep the AES pipeline busy.
    while (pos + 64 <= len) {
        alignas(16) uint8_t keystream[64];
        alignas(16) uint8_t blocks[4][16];
        for (int i = 0; i < 4; ++i) {
            std::memcpy(blocks[i], counter, 16);
            inc128(counter);
            __m128i x = _mm_load_si128(reinterpret_cast<const __m128i*>(blocks[i]));
            _mm_store_si128(reinterpret_cast<__m128i*>(keystream + 16 * i),
                            encrypt_block(x, rk));
        }
        for (size_t i = 0; i < 64; ++i) out[pos + i] = in[pos + i] ^ keystream[i];
        pos += 64;
    }

    while (pos < len) {
        alignas(16) uint8_t keystream[16];
        alignas(16) uint8_t block[16];
        std::memcpy(block, counter, 16);
        inc128(counter);
        _mm_store_si128(reinterpret_cast<__m128i*>(keystream),
                        encrypt_block(_mm_load_si128(reinterpret_cast<const __m128i*>(block)), rk));
        const size_t chunk = std::min<size_t>(16, len - pos);
        for (size_t i = 0; i < chunk; ++i) out[pos + i] = in[pos + i] ^ keystream[i];
        pos += chunk;
    }
}

#else

void ctr_crypt_128(const uint8_t*, uint8_t[16], const uint8_t*, uint8_t*, size_t) {
    throw std::runtime_error("AES-NI is not compiled for this architecture");
}

#endif

} // namespace aesni
