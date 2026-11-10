#include "rsa.hpp"

#include "util.hpp"

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
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

constexpr std::size_t kOaepHashBytes = 32;

std::string sha256(const std::string& data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    util::require(EVP_Digest(data.data(), data.size(), digest, &length, EVP_sha256(), nullptr) == 1,
                  "SHA-256 failed");
    return std::string(reinterpret_cast<char*>(digest), length);
}

std::string mgf1(const std::string& seed, std::size_t length) {
    std::string out;
    out.reserve(length);
    for (std::uint32_t counter = 0; out.size() < length; ++counter) {
        std::string input = seed;
        input.push_back(static_cast<char>((counter >> 24) & 0xff));
        input.push_back(static_cast<char>((counter >> 16) & 0xff));
        input.push_back(static_cast<char>((counter >> 8) & 0xff));
        input.push_back(static_cast<char>(counter & 0xff));
        out += sha256(input);
    }
    out.resize(length);
    return out;
}

std::string xorStrings(const std::string& left, const std::string& right) {
    util::require(left.size() == right.size(), "internal OAEP length mismatch");
    std::string out(left.size(), '\0');
    for (std::size_t i = 0; i < left.size(); ++i)
        out[i] = static_cast<char>(static_cast<unsigned char>(left[i]) ^
                                   static_cast<unsigned char>(right[i]));
    return out;
}

// Best-effort constant-time helpers for the OAEP padding check.
unsigned char ctIsZero(unsigned char value) {
    return static_cast<unsigned char>(((static_cast<unsigned int>(value) - 1) >> 8) & 1);
}

unsigned char ctLess(std::size_t left, std::size_t right) {
    return static_cast<unsigned char>((left - right) >>
        (std::numeric_limits<std::size_t>::digits - 1));
}

std::size_t ctSelect(unsigned char bit, std::size_t yes, std::size_t no) {
    const std::size_t mask = static_cast<std::size_t>(0) - bit;
    return (yes & mask) | (no & ~mask);
}

std::string rawRsa(const Key& key, const std::string& input, bool encrypt) {
    EVP_PKEY_CTX* ctx = newContext(key);
    std::string out;
    try {
        if (encrypt) {
            util::require(EVP_PKEY_encrypt_init(ctx) > 0, "RSA encryption failed");
            util::require(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING) > 0,
                          "RSA encryption failed");
            std::size_t length = 0;
            util::require(EVP_PKEY_encrypt(ctx, nullptr, &length,
                                           reinterpret_cast<const unsigned char*>(input.data()),
                                           input.size()) > 0,
                          "RSA encryption failed");
            out.resize(length);
            util::require(EVP_PKEY_encrypt(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length,
                                           reinterpret_cast<const unsigned char*>(input.data()),
                                           input.size()) > 0,
                          "RSA encryption failed");
            out.resize(length);
        } else {
            util::require(EVP_PKEY_decrypt_init(ctx) > 0, "decryption failed");
            util::require(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING) > 0,
                          "decryption failed");
            std::size_t length = 0;
            util::require(EVP_PKEY_decrypt(ctx, nullptr, &length,
                                           reinterpret_cast<const unsigned char*>(input.data()),
                                           input.size()) > 0,
                          "decryption failed");
            out.resize(length);
            util::require(EVP_PKEY_decrypt(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length,
                                           reinterpret_cast<const unsigned char*>(input.data()),
                                           input.size()) > 0,
                          "decryption failed");
            out.resize(length);
        }
    } catch (...) {
        EVP_PKEY_CTX_free(ctx);
        throw;
    }
    EVP_PKEY_CTX_free(ctx);
    return out;
}

std::string oaepEncode(const std::string& label, const std::string& message, std::size_t k) {
    util::require(k >= 2 * kOaepHashBytes + 2, "RSA key is too small for OAEP");
    util::require(message.size() <= k - 2 * kOaepHashBytes - 2,
                  "plaintext exceeds the RSA-OAEP limit");
    const std::string lHash = sha256(label);
    const std::size_t dbLength = k - kOaepHashBytes - 1;
    std::string db(dbLength, '\0');
    std::copy(lHash.begin(), lHash.end(), db.begin());
    const std::size_t psLength = dbLength - lHash.size() - 1 - message.size();
    db[lHash.size() + psLength] = '\x01';
    std::copy(message.begin(), message.end(), db.begin() + lHash.size() + psLength + 1);

    std::string seed(kOaepHashBytes, '\0');
    util::require(RAND_bytes(reinterpret_cast<unsigned char*>(&seed[0]),
                             static_cast<int>(seed.size())) == 1,
                  "secure random generator failed");
    const std::string maskedDb = xorStrings(db, mgf1(seed, dbLength));
    const std::string maskedSeed = xorStrings(seed, mgf1(maskedDb, kOaepHashBytes));

    std::string encoded(k, '\0');
    std::copy(maskedSeed.begin(), maskedSeed.end(), encoded.begin() + 1);
    std::copy(maskedDb.begin(), maskedDb.end(), encoded.begin() + 1 + kOaepHashBytes);
    return encoded;
}

std::string oaepDecode(const std::string& label, const std::string& encoded, std::size_t k) {
    util::require(encoded.size() == k, "decryption failed");
    unsigned char bad = ctIsZero(static_cast<unsigned char>(encoded[0])) ^ 1;

    const std::string maskedSeed = encoded.substr(1, kOaepHashBytes);
    const std::string maskedDb = encoded.substr(1 + kOaepHashBytes);
    const std::string seed = xorStrings(maskedSeed, mgf1(maskedDb, kOaepHashBytes));
    const std::string db = xorStrings(maskedDb, mgf1(seed, maskedDb.size()));
    const std::string lHash = sha256(label);
    bad |= static_cast<unsigned char>(CRYPTO_memcmp(db.data(), lHash.data(), lHash.size()) != 0);

    std::size_t separator = db.size();
    unsigned char found = 0;
    for (std::size_t i = kOaepHashBytes; i < db.size(); ++i) {
        const unsigned char isOne = ctIsZero(static_cast<unsigned char>(db[i]) ^ 1);
        separator = ctSelect(isOne & (found ^ 1), i, separator);
        found |= isOne;
    }
    bad |= static_cast<unsigned char>(found ^ 1);
    for (std::size_t i = kOaepHashBytes; i < db.size(); ++i) {
        bad |= ctLess(i, separator) & (ctIsZero(static_cast<unsigned char>(db[i])) ^ 1);
    }
    // shortcut: best-effort constant-time padding check; OpenSSL raw RSA and
    // the final plaintext copy still vary. Use EVP OAEP for production.
    if (bad) throw std::runtime_error("decryption failed");
    return db.substr(separator + 1);
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

std::string oaepEncryptManual(const Key& publicKey, const std::string& plaintext,
                              const std::string& label) {
    const std::size_t k = static_cast<std::size_t>(publicKey.bits() + 7) / 8;
    return rawRsa(publicKey, oaepEncode(label, plaintext, k), true);
}

std::string oaepDecryptManual(const Key& privateKey, const std::string& ciphertext,
                              const std::string& label) {
    const std::size_t k = static_cast<std::size_t>(privateKey.bits() + 7) / 8;
    std::string encoded;
    try {
        encoded = rawRsa(privateKey, ciphertext, false);
        return oaepDecode(label, encoded, k);
    } catch (...) {
        if (!encoded.empty()) OPENSSL_cleanse(&encoded[0], encoded.size());
        throw std::runtime_error("decryption failed");
    }
}

}  // namespace rsa
