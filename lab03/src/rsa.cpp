#include "rsa.hpp"

#include "util.hpp"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace rsa {
namespace {

constexpr int kMinimumBits = 3072;

void checkBits(int bits) {
    util::require(bits >= kMinimumBits,
                  "RSA key size " + std::to_string(bits) + " bits is below the 3072-bit minimum");
}

std::string bioToString(BIO* bio) {
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio, &data);
    return std::string(data, static_cast<std::size_t>(length));
}

std::string writePem(const Key& key, bool isPrivate) {
    BIO* bio = BIO_new(BIO_s_mem());
    util::require(bio != nullptr, "cannot allocate PEM buffer");
    const int ok = isPrivate
        ? PEM_write_bio_PrivateKey(bio, key.get(), nullptr, nullptr, 0, nullptr, nullptr)
        : PEM_write_bio_PUBKEY(bio, key.get());
    std::string out = ok == 1 ? bioToString(bio) : std::string();
    BIO_free(bio);
    util::require(!out.empty(), "failed to encode PEM key");
    return out;
}

std::string writeDer(const Key& key, bool isPrivate) {
    unsigned char* data = nullptr;
    const int length = isPrivate ? i2d_PrivateKey(key.get(), &data) : i2d_PUBKEY(key.get(), &data);
    util::require(length > 0 && data != nullptr, "failed to encode DER key");
    std::string out(reinterpret_cast<char*>(data), static_cast<std::size_t>(length));
    OPENSSL_free(data);
    return out;
}

Key loadPemOrDer(const std::string& data, bool isPrivate) {
    EVP_PKEY* key = nullptr;
    BIO* bio = BIO_new_mem_buf(data.data(), static_cast<int>(data.size()));
    if (bio) {
        key = isPrivate ? PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr)
                        : PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
    }
    if (!key) {
        const unsigned char* cursor = reinterpret_cast<const unsigned char*>(data.data());
        key = isPrivate ? d2i_AutoPrivateKey(nullptr, &cursor, static_cast<long>(data.size()))
                        : d2i_PUBKEY(nullptr, &cursor, static_cast<long>(data.size()));
    }
    util::require(key != nullptr, isPrivate ? "invalid private key encoding"
                                            : "invalid public key encoding");
    Key result(key);
    checkBits(result.bits());
    return result;
}

void setLabel(EVP_PKEY_CTX* ctx, const std::string& label) {
    util::require(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) > 0,
                  "cannot set OAEP padding");
    util::require(EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0,
                  "cannot set OAEP hash");
    util::require(EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) > 0,
                  "cannot set OAEP MGF1 hash");
    if (!label.empty()) {
        void* copy = OPENSSL_malloc(label.size());
        util::require(copy != nullptr, "cannot allocate OAEP label");
        std::memcpy(copy, label.data(), label.size());
        if (EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, copy, static_cast<int>(label.size())) <= 0) {
            OPENSSL_free(copy);
            throw std::runtime_error("cannot set OAEP label");
        }
    }
}

EVP_PKEY_CTX* newContext(const Key& key) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.get(), nullptr);
    util::require(ctx != nullptr, "cannot create RSA context");
    return ctx;
}

}  // namespace

Key generateKey(int bits) {
    checkBits(bits);
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    util::require(ctx != nullptr, "cannot create RSA keygen context");
    EVP_PKEY* key = nullptr;
    const bool ok = EVP_PKEY_keygen_init(ctx) > 0 &&
                    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) > 0 &&
                    EVP_PKEY_keygen(ctx, &key) > 0;
    EVP_PKEY_CTX_free(ctx);
    util::require(ok && key != nullptr, "RSA key generation failed");
    return Key(key);
}

Key loadPublicKey(const std::string& data) { return loadPemOrDer(data, false); }
Key loadPrivateKey(const std::string& data) { return loadPemOrDer(data, true); }

std::string publicPem(const Key& key) { return writePem(key, false); }
std::string privatePem(const Key& key) { return writePem(key, true); }
std::string publicDer(const Key& key) { return writeDer(key, false); }
std::string privateDer(const Key& key) { return writeDer(key, true); }

std::size_t maxPlaintext(const Key& key) {
    const std::size_t k = static_cast<std::size_t>(key.bits() + 7) / 8;
    return k - 2 * 32 - 2;
}

std::string oaepEncrypt(const Key& publicKey, const std::string& plaintext,
                        const std::string& label) {
    util::require(plaintext.size() <= maxPlaintext(publicKey),
                  "plaintext exceeds the RSA-OAEP limit");
    EVP_PKEY_CTX* ctx = newContext(publicKey);
    std::string out;
    try {
        util::require(EVP_PKEY_encrypt_init(ctx) > 0, "cannot start RSA encryption");
        setLabel(ctx, label);
        std::size_t length = 0;
        util::require(EVP_PKEY_encrypt(ctx, nullptr, &length,
                                       reinterpret_cast<const unsigned char*>(plaintext.data()),
                                       plaintext.size()) > 0,
                      "RSA encryption failed");
        out.resize(length);
        util::require(EVP_PKEY_encrypt(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length,
                                       reinterpret_cast<const unsigned char*>(plaintext.data()),
                                       plaintext.size()) > 0,
                      "RSA encryption failed");
        out.resize(length);
    } catch (...) {
        EVP_PKEY_CTX_free(ctx);
        throw;
    }
    EVP_PKEY_CTX_free(ctx);
    return out;
}

std::string oaepDecrypt(const Key& privateKey, const std::string& ciphertext,
                        const std::string& label) {
    EVP_PKEY_CTX* ctx = newContext(privateKey);
    std::string out;
    try {
        util::require(EVP_PKEY_decrypt_init(ctx) > 0, "decryption failed");
        setLabel(ctx, label);
        std::size_t length = 0;
        util::require(EVP_PKEY_decrypt(ctx, nullptr, &length,
                                       reinterpret_cast<const unsigned char*>(ciphertext.data()),
                                       ciphertext.size()) > 0,
                      "decryption failed");
        out.resize(length);
        util::require(EVP_PKEY_decrypt(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length,
                                       reinterpret_cast<const unsigned char*>(ciphertext.data()),
                                       ciphertext.size()) > 0,
                      "decryption failed");
        out.resize(length);
    } catch (...) {
        EVP_PKEY_CTX_free(ctx);
        std::fill(out.begin(), out.end(), '\0');
        throw std::runtime_error("decryption failed");
    }
    EVP_PKEY_CTX_free(ctx);
    return out;
}

}  // namespace rsa
