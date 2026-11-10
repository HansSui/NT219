// RSA key handling and RSA-OAEP(SHA-256) encryption.
#pragma once

#include <cstddef>
#include <string>

#include <openssl/evp.h>

namespace rsa {

class Key {
public:
    Key() = default;
    explicit Key(EVP_PKEY* key) : key_(key) {}
    ~Key() { if (key_) EVP_PKEY_free(key_); }
    Key(Key&& other) noexcept : key_(other.key_) { other.key_ = nullptr; }
    Key& operator=(Key&& other) noexcept {
        if (this != &other) {
            if (key_) EVP_PKEY_free(key_);
            key_ = other.key_;
            other.key_ = nullptr;
        }
        return *this;
    }
    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;

    EVP_PKEY* get() const { return key_; }
    explicit operator bool() const { return key_ != nullptr; }
    int bits() const { return key_ ? EVP_PKEY_bits(key_) : 0; }

private:
    EVP_PKEY* key_ = nullptr;
};

// Both loaders reject keys below 3072 bits (course minimum) and bad encodings.
Key generateKey(int bits);
Key loadPublicKey(const std::string& data);
Key loadPrivateKey(const std::string& data);

std::string publicPem(const Key& key);
std::string privatePem(const Key& key);
std::string publicDer(const Key& key);
std::string privateDer(const Key& key);

std::size_t maxPlaintext(const Key& key);
std::string oaepEncrypt(const Key& publicKey, const std::string& plaintext,
                        const std::string& label);
std::string oaepDecrypt(const Key& privateKey, const std::string& ciphertext,
                        const std::string& label);

// Manual OAEP (MGF1-SHA256, XOR masking, constant-time-ish padding check)
// using OpenSSL only for raw RSA and SHA-256. Interoperates with `oaepEncrypt`
// and with standard RSA-OAEP-SHA256 ciphertexts.
std::string oaepEncryptManual(const Key& publicKey, const std::string& plaintext,
                              const std::string& label);
std::string oaepDecryptManual(const Key& privateKey, const std::string& ciphertext,
                              const std::string& label);

}  // namespace rsa
