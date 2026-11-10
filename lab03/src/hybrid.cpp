#include "hybrid.hpp"

#include "minijson.hpp"
#include "util.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <vector>

namespace hybrid {
namespace {

constexpr std::size_t kAesKeyBytes = 32;
constexpr std::size_t kIvBytes = 12;
constexpr std::size_t kTagBytes = 16;
constexpr std::size_t kCtrHmacKeyBytes = 64;
constexpr std::size_t kCtrIvBytes = 16;
constexpr std::size_t kHmacTagBytes = 32;

std::string headerJson(const Envelope& envelope, bool withTag) {
    std::ostringstream out;
    out << "{\"version\":" << envelope.version
        << ",\"mode\":\"" << util::jsonEscape(envelope.mode)
        << "\",\"rsa_modulus\":" << envelope.rsaModulus
        << ",\"hash\":\"" << util::jsonEscape(envelope.hash)
        << "\",\"label\":\"" << util::jsonEscape(envelope.label)
        << "\",\"wrapped_key\":\"" << util::toBase64(envelope.wrappedKey)
        << "\",\"iv\":\"" << util::toBase64(envelope.iv)
        << "\",\"payload_len\":" << envelope.payload.size();
    if (withTag) out << ",\"tag\":\"" << util::toBase64(envelope.tag) << "\"";
    out << '}';
    return out.str();
}

std::string randomBytes(std::size_t count) {
    std::string out(count, '\0');
    util::require(RAND_bytes(reinterpret_cast<unsigned char*>(&out[0]), static_cast<int>(count)) == 1,
                  "secure random generator failed");
    return out;
}

std::string errorMessage() {
    return "decryption failed";
}

struct GcmResult {
    std::string ciphertext;
    std::string tag;
};

GcmResult gcmEncrypt(const std::string& key, const std::string& iv, const std::string& aad,
                     const std::string& plaintext) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    util::require(ctx != nullptr, "cannot create AES-GCM context");
    GcmResult result;
    result.ciphertext.resize(plaintext.size() + kTagBytes);
    int length = 0;
    int total = 0;
    const bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) == 1 &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()),
                           reinterpret_cast<const unsigned char*>(iv.data())) == 1 &&
        (aad.empty() ||
         EVP_EncryptUpdate(ctx, nullptr, &length,
                           reinterpret_cast<const unsigned char*>(aad.data()),
                           static_cast<int>(aad.size())) == 1) &&
        EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(&result.ciphertext[0]), &length,
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          static_cast<int>(plaintext.size())) == 1;
    total = length;
    if (ok) {
        int finalLength = 0;
        if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&result.ciphertext[0]) + total,
                                &finalLength) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("AES-GCM encryption failed");
        }
        total += finalLength;
        result.ciphertext.resize(static_cast<std::size_t>(total));
        result.tag.resize(kTagBytes);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagBytes),
                                &result.tag[0]) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("AES-GCM encryption failed");
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    util::require(ok, "AES-GCM encryption failed");
    return result;
}

std::string gcmDecrypt(const std::string& key, const std::string& iv, const std::string& aad,
                       const std::string& ciphertext, const std::string& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    util::require(ctx != nullptr, errorMessage());
    std::string out(ciphertext.size() + kTagBytes, '\0');
    int length = 0;
    int total = 0;
    const bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) == 1 &&
        EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()),
                           reinterpret_cast<const unsigned char*>(iv.data())) == 1 &&
        (aad.empty() ||
         EVP_DecryptUpdate(ctx, nullptr, &length,
                           reinterpret_cast<const unsigned char*>(aad.data()),
                           static_cast<int>(aad.size())) == 1);
    int aadOk = ok ? 1 : 0;
    bool bodyOk = false;
    if (aadOk) {
        bodyOk = EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length,
                                   reinterpret_cast<const unsigned char*>(ciphertext.data()),
                                   static_cast<int>(ciphertext.size())) == 1;
        total = length;
    }
    bool tagOk = false;
    if (bodyOk) {
        std::string tagCopy = tag;
        tagOk = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tagCopy.size()),
                                    &tagCopy[0]) == 1;
    }
    if (tagOk) {
        int finalLength = 0;
        tagOk = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&out[0]) + total,
                                    &finalLength) == 1;
        total += finalLength;
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!tagOk) {
        std::fill(out.begin(), out.end(), '\0');
        throw std::runtime_error(errorMessage());
    }
    out.resize(static_cast<std::size_t>(total));
    return out;
}

std::string ctrXor(const std::string& key, const std::string& iv, const std::string& input) {
    if (input.empty()) return {};
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    util::require(ctx != nullptr, "cannot create AES-CTR context");
    std::string out(input.size(), '\0');
    int length = 0;
    int total = 0;
    const bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()),
                           reinterpret_cast<const unsigned char*>(iv.data())) == 1 &&
        EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length,
                          reinterpret_cast<const unsigned char*>(input.data()),
                          static_cast<int>(input.size())) == 1;
    total = length;
    if (ok) {
        int finalLength = 0;
        if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&out[0]) + total,
                                &finalLength) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("AES-CTR encryption failed");
        }
        total += finalLength;
    }
    EVP_CIPHER_CTX_free(ctx);
    util::require(ok, "AES-CTR encryption failed");
    out.resize(static_cast<std::size_t>(total));
    return out;
}

std::string hmacSha256(const std::string& key, const std::string& data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    util::require(HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
                       reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                       digest, &length) != nullptr,
                  "HMAC-SHA256 failed");
    return std::string(reinterpret_cast<char*>(digest), length);
}

std::string authData(const std::string& iv, const std::string& aad,
                     const std::string& ciphertext) {
    std::string out = iv;
    const std::uint64_t aadLength = aad.size();
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<char>((aadLength >> shift) & 0xff));
    out += aad;
    out += ciphertext;
    return out;
}

std::string getString(const minijson::Value& root, const char* key) {
    try {
        return root.getString(key);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid envelope");
    }
}

double getNumber(const minijson::Value& root, const char* key) {
    try {
        const minijson::Value& value = root.at(key);
        if (value.type != minijson::Value::Type::Number) throw std::runtime_error("not a number");
        return value.number;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid envelope");
    }
}

}  // namespace

bool isEnvelope(const std::string& data) {
    return data.compare(0, std::char_traits<char>::length(kMagic), kMagic) == 0;
}

std::string serialize(const Envelope& envelope) {
    return std::string(kMagic) + headerJson(envelope, true) + "\n" + envelope.payload;
}

Envelope parse(const std::string& data) {
    const std::size_t magicLength = std::char_traits<char>::length(kMagic);
    if (!isEnvelope(data)) throw std::runtime_error("invalid envelope");
    const std::size_t lineEnd = data.find('\n', magicLength);
    if (lineEnd == std::string::npos) throw std::runtime_error("invalid envelope");

    Envelope envelope;
    const minijson::Value root = minijson::parse(data.substr(magicLength, lineEnd - magicLength));
    envelope.version = static_cast<int>(getNumber(root, "version"));
    envelope.mode = getString(root, "mode");
    envelope.rsaModulus = static_cast<int>(getNumber(root, "rsa_modulus"));
    envelope.hash = getString(root, "hash");
    envelope.label = getString(root, "label");
    envelope.wrappedKey = util::fromBase64(getString(root, "wrapped_key"));
    envelope.iv = util::fromBase64(getString(root, "iv"));
    envelope.tag = util::fromBase64(getString(root, "tag"));
    envelope.payload = data.substr(lineEnd + 1);

    util::require(envelope.version == 1, "invalid envelope");
    util::require(envelope.mode == "RSA-OAEP-AES-GCM", "invalid envelope");
    util::require(envelope.hash == "SHA-256", "invalid envelope");
    util::require(envelope.rsaModulus >= 3072, "invalid envelope");
    util::require(envelope.iv.size() == kIvBytes, "invalid envelope");
    util::require(envelope.tag.size() == kTagBytes, "invalid envelope");
    util::require(envelope.wrappedKey.size() == static_cast<std::size_t>(envelope.rsaModulus / 8),
                  "invalid envelope");
    util::require(getNumber(root, "payload_len") == static_cast<double>(envelope.payload.size()),
                  "invalid envelope");
    return envelope;
}

std::string encrypt(const rsa::Key& publicKey, const std::string& plaintext,
                    const std::string& label) {
    const std::string key = randomBytes(kAesKeyBytes);
    const std::string iv = randomBytes(kIvBytes);

    Envelope envelope;
    envelope.rsaModulus = publicKey.bits();
    envelope.label = label;
    envelope.wrappedKey = rsa::oaepEncrypt(publicKey, key, label);
    envelope.iv = iv;
    envelope.payload.resize(plaintext.size());

    const GcmResult gcm = gcmEncrypt(key, iv, headerJson(envelope, false), plaintext);
    envelope.payload = gcm.ciphertext;
    envelope.tag = gcm.tag;
    return serialize(envelope);
}

std::string decrypt(const rsa::Key& privateKey, const std::string& data,
                    const std::string& labelOverride) {
    std::string key;
    try {
        const Envelope envelope = parse(data);
        util::require(privateKey.bits() == envelope.rsaModulus, errorMessage());
        util::require(labelOverride.empty() || labelOverride == envelope.label, errorMessage());
        key = rsa::oaepDecrypt(privateKey, envelope.wrappedKey, envelope.label);
        util::require(key.size() == kAesKeyBytes, errorMessage());
        std::string plaintext =
            gcmDecrypt(key, envelope.iv, headerJson(envelope, false), envelope.payload, envelope.tag);
        OPENSSL_cleanse(&key[0], key.size());
        return plaintext;
    } catch (const std::exception&) {
        if (!key.empty()) OPENSSL_cleanse(&key[0], key.size());
        throw std::runtime_error(errorMessage());
    }
}

std::string aesGcmEncrypt(const std::string& key, const std::string& iv,
                          const std::string& aad, const std::string& plaintext,
                          std::string& tag) {
    util::require(key.size() == kAesKeyBytes && iv.size() == kIvBytes, "invalid AES-GCM key or IV");
    const GcmResult result = gcmEncrypt(key, iv, aad, plaintext);
    tag = result.tag;
    return result.ciphertext;
}

std::string aesGcmDecrypt(const std::string& key, const std::string& iv,
                          const std::string& aad, const std::string& ciphertext,
                          const std::string& tag) {
    util::require(key.size() == kAesKeyBytes && iv.size() == kIvBytes && tag.size() == kTagBytes,
                  "invalid AES-GCM key, IV or tag");
    return gcmDecrypt(key, iv, aad, ciphertext, tag);
}

std::string aesCtrHmacEncrypt(const std::string& key, const std::string& iv,
                              const std::string& aad, const std::string& plaintext,
                              std::string& tag) {
    util::require(key.size() == kCtrHmacKeyBytes && iv.size() == kCtrIvBytes,
                  "invalid AES-CTR+HMAC key or IV");
    const std::string aesKey = key.substr(0, kAesKeyBytes);
    const std::string macKey = key.substr(kAesKeyBytes);
    const std::string ciphertext = ctrXor(aesKey, iv, plaintext);
    tag = hmacSha256(macKey, authData(iv, aad, ciphertext));
    return ciphertext;
}

std::string aesCtrHmacDecrypt(const std::string& key, const std::string& iv,
                              const std::string& aad, const std::string& ciphertext,
                              const std::string& tag) {
    util::require(key.size() == kCtrHmacKeyBytes && iv.size() == kCtrIvBytes &&
                  tag.size() == kHmacTagBytes,
                  "invalid AES-CTR+HMAC key, IV or tag");
    const std::string aesKey = key.substr(0, kAesKeyBytes);
    const std::string macKey = key.substr(kAesKeyBytes);
    const std::string expected = hmacSha256(macKey, authData(iv, aad, ciphertext));
    util::require(CRYPTO_memcmp(expected.data(), tag.data(), expected.size()) == 0,
                  errorMessage());
    return ctrXor(aesKey, iv, ciphertext);
}

}  // namespace hybrid
