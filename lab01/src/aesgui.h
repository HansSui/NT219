#pragma once

// Stable C ABI used by the Python/Tkinter GUI (gui/aestool_gui.py) through
// ctypes. Build the project with -DBUILD_SHARED_LIBS=ON so this interface is
// exported from aescore.dll (Windows) or libaescore.so (Linux).
//
// All strings are UTF-8/ASCII. Byte buffers are length-counted and may contain
// NUL bytes. Every function returns 0 on success and -1 on failure; on failure
// call aesgui_last_error() for a human-readable message.

#include <stddef.h>

#if defined(_WIN32) && defined(AESCORE_SHARED)
#  ifdef AESCORE_EXPORTS
#    define AESGUI_API __declspec(dllexport)
#  else
#    define AESGUI_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define AESGUI_API __attribute__((visibility("default")))
#else
#  define AESGUI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Encrypts plaintext and returns the self-describing AESL1 container.
// iv_hex may be NULL or empty to generate a random IV/nonce; the IV is stored
// in the returned record. aad_hex may be NULL or empty. pkcs7 is 1 for the CLI
// behaviour (ECB/CBC padding) and 0 for raw block-aligned data.
// The caller must release *record with aesgui_free().
AESGUI_API int aesgui_encrypt(const char* mode, const char* key_hex, const char* iv_hex,
                              const char* aad_hex, const unsigned char* plaintext,
                              size_t plaintext_len, int pkcs7, unsigned char** record,
                              size_t* record_len);

// Decrypts an AESL1 container. mode must match the mode in the record header.
// iv_hex/aad_hex may be NULL or empty to use the values stored in the record;
// when non-empty they override the header (useful for negative tests).
// The caller must release *plaintext with aesgui_free().
AESGUI_API int aesgui_decrypt(const char* mode, const char* key_hex, const char* iv_hex,
                              const char* aad_hex, const unsigned char* record,
                              size_t record_len, unsigned char** plaintext,
                              size_t* plaintext_len);

// Frees a buffer returned by aesgui_encrypt() or aesgui_decrypt().
AESGUI_API void aesgui_free(void* buffer);

// Error message for the most recent failed call on this thread. The returned
// pointer is valid until the next AESGUI call on the same thread.
AESGUI_API const char* aesgui_last_error(void);

// Fills out with 2*bytes hex characters plus a terminator. out_size must be at
// least 2*bytes + 1.
AESGUI_API int aesgui_random_hex(int bytes, char* out, size_t out_size);

// Reports the header of an AESL1 record: mode name, IV hex, tag length and AAD
// length. mode_size/iv_hex_size include the terminating NUL.
AESGUI_API int aesgui_record_info(const unsigned char* record, size_t record_len,
                                  char* mode, size_t mode_size, char* iv_hex,
                                  size_t iv_hex_size, int* tag_len, int* aad_len);

// Returns the default IV length, tag length and AEAD flag for a mode.
AESGUI_API int aesgui_mode_sizes(const char* mode, int* iv_len, int* tag_len, int* aead);

#ifdef __cplusplus
}
#endif
