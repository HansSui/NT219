#pragma once

#include <cstddef>
#include <string>

#if defined(_WIN32) && defined(AESCORE_SHARED)
#  ifdef AESCORE_EXPORTS
#    define AESCORE_API __declspec(dllexport)
#  else
#    define AESCORE_API __declspec(dllimport)
#  endif
#else
#  define AESCORE_API
#endif

namespace aes {

enum class Mode { ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM };

AESCORE_API Mode parseMode(const std::string& name);
AESCORE_API std::string modeName(Mode mode);
AESCORE_API bool isAead(Mode mode);
// Default IV/nonce length. ECB has none; CCM also accepts 7..13 bytes.
AESCORE_API std::size_t ivSize(Mode mode);
AESCORE_API std::size_t tagSize(Mode mode);

AESCORE_API std::string fromHex(const std::string& hex);
AESCORE_API std::string toHex(const std::string& bytes);
AESCORE_API std::string toBase64(const std::string& bytes);
AESCORE_API std::string sha256Hex(const std::string& bytes);
AESCORE_API std::string randomBytes(std::size_t count);
AESCORE_API std::string loadKeyFile(const std::string& path);

struct Encrypted {
    std::string ciphertext;
    std::string tag;  // empty for non-AEAD modes
};

// Throws std::exception on invalid key/IV/tag length or authentication failure.
// pkcs7 applies to ECB/CBC only; KAT vectors use raw block-aligned data.
AESCORE_API Encrypted encrypt(Mode mode, const std::string& key, const std::string& iv,
                              const std::string& plaintext, const std::string& aad = {},
                              bool pkcs7 = true);
AESCORE_API std::string decrypt(Mode mode, const std::string& key, const std::string& iv,
                                const std::string& ciphertext, const std::string& aad = {},
                                const std::string& tag = {}, bool pkcs7 = true);

// Self-describing ciphertext file: magic, mode, IV, tag, AAD and ciphertext.
struct Record {
    Mode mode = Mode::CBC;
    std::string iv;
    std::string aad;
    std::string tag;
    std::string ciphertext;
};

AESCORE_API std::string serialize(const Record& record);
AESCORE_API Record parseRecord(const std::string& bytes);

}  // namespace aes
