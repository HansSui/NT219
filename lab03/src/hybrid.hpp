// AES-256-GCM hybrid envelope: RSA-OAEP wrapped key + authenticated payload.
#pragma once

#include "rsa.hpp"

#include <cstddef>
#include <string>

namespace hybrid {

constexpr const char* kMagic = "RSAENV1\n";

struct Envelope {
    int version = 1;
    std::string mode = "RSA-OAEP-AES-GCM";
    int rsaModulus = 0;
    std::string hash = "SHA-256";
    std::string label;
    std::string wrappedKey;
    std::string iv;
    std::string tag;
    std::string payload;
};

bool isEnvelope(const std::string& data);

// The canonical header (without tag) is the GCM AAD, so every header field is
// authenticated. Serialization is deterministic.
std::string serialize(const Envelope& envelope);
Envelope parse(const std::string& data);

// AES-256-GCM primitives, exposed for benchmarks and tests. The IV must be 12
// bytes and the key 32 bytes. Encryption returns the ciphertext and writes the
// 16-byte tag to `tag`.
std::string aesGcmEncrypt(const std::string& key, const std::string& iv,
                          const std::string& aad, const std::string& plaintext,
                          std::string& tag);
std::string aesGcmDecrypt(const std::string& key, const std::string& iv,
                          const std::string& aad, const std::string& ciphertext,
                          const std::string& tag);

// AES-256-CTR + HMAC-SHA256 (encrypt-then-MAC) comparison primitive. The key
// is 64 bytes: the first 32 bytes encrypt, the last 32 bytes authenticate. The
// IV is 16 bytes and the tag is 32 bytes. The tag covers the IV, AAD length,
// AAD and ciphertext.
std::string aesCtrHmacEncrypt(const std::string& key, const std::string& iv,
                              const std::string& aad, const std::string& plaintext,
                              std::string& tag);
std::string aesCtrHmacDecrypt(const std::string& key, const std::string& iv,
                              const std::string& aad, const std::string& ciphertext,
                              const std::string& tag);

std::string encrypt(const rsa::Key& publicKey, const std::string& plaintext,
                    const std::string& label);
std::string decrypt(const rsa::Key& privateKey, const std::string& data,
                    const std::string& labelOverride);

}  // namespace hybrid
