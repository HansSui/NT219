#include "aesgui.h"

#include "aescore.hpp"

#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>

namespace {

thread_local std::string g_lastError;

void setError(const std::string& message) { g_lastError = message; }
void clearError() { g_lastError.clear(); }

template <class Function>
int guarded(Function&& function) {
    clearError();
    try {
        function();
        return 0;
    } catch (const std::bad_alloc&) {
        setError("out of memory");
        return -1;
    } catch (const std::exception& error) {
        setError(error.what());
        return -1;
    } catch (...) {
        setError("unknown error");
        return -1;
    }
}

std::string optionalHex(const char* hex) {
    return (hex != nullptr && *hex != '\0') ? aes::fromHex(hex) : std::string();
}

unsigned char* copyBytes(const std::string& bytes, size_t* length) {
    auto* buffer = new unsigned char[bytes.empty() ? 1 : bytes.size()];
    if (!bytes.empty()) std::memcpy(buffer, bytes.data(), bytes.size());
    *length = bytes.size();
    return buffer;
}

std::string inputBytes(const unsigned char* data, size_t length) {
    if (length == 0) return {};
    if (data == nullptr) throw std::runtime_error("input buffer is null");
    return std::string(reinterpret_cast<const char*>(data), length);
}

void writeText(const std::string& text, char* out, size_t capacity, const char* field) {
    if (out == nullptr) throw std::runtime_error(std::string("null ") + field + " buffer");
    if (capacity < text.size() + 1) {
        throw std::runtime_error(std::string(field) + " buffer is too small");
    }
    std::memcpy(out, text.c_str(), text.size() + 1);
}

}  // namespace

extern "C" {

int aesgui_encrypt(const char* mode, const char* key_hex, const char* iv_hex,
                   const char* aad_hex, const unsigned char* plaintext,
                   size_t plaintext_len, int pkcs7, unsigned char** record,
                   size_t* record_len) {
    return guarded([&] {
        if (mode == nullptr) throw std::runtime_error("mode is required");
        if (key_hex == nullptr) throw std::runtime_error("key is required");
        if (record == nullptr || record_len == nullptr) {
            throw std::runtime_error("output pointers are null");
        }
        *record = nullptr;
        *record_len = 0;

        const aes::Mode selected = aes::parseMode(mode);
        const std::string key = aes::fromHex(key_hex);
        std::string iv = optionalHex(iv_hex);
        if (iv.empty() && aes::ivSize(selected) != 0) iv = aes::randomBytes(aes::ivSize(selected));
        const std::string aad = optionalHex(aad_hex);
        const std::string data = inputBytes(plaintext, plaintext_len);

        const aes::Encrypted encrypted = aes::encrypt(selected, key, iv, data, aad, pkcs7 != 0);

        aes::Record container;
        container.mode = selected;
        container.iv = iv;
        container.aad = aad;
        container.tag = encrypted.tag;
        container.ciphertext = encrypted.ciphertext;

        const std::string bytes = aes::serialize(container);
        *record = copyBytes(bytes, record_len);
    });
}

int aesgui_decrypt(const char* mode, const char* key_hex, const char* iv_hex,
                   const char* aad_hex, const unsigned char* record, size_t record_len,
                   unsigned char** plaintext, size_t* plaintext_len) {
    return guarded([&] {
        if (mode == nullptr) throw std::runtime_error("mode is required");
        if (key_hex == nullptr) throw std::runtime_error("key is required");
        if (plaintext == nullptr || plaintext_len == nullptr) {
            throw std::runtime_error("output pointers are null");
        }
        *plaintext = nullptr;
        *plaintext_len = 0;

        const aes::Mode selected = aes::parseMode(mode);
        const std::string key = aes::fromHex(key_hex);
        const aes::Record container = aes::parseRecord(inputBytes(record, record_len));
        if (container.mode != selected) {
            throw std::runtime_error("--mode does not match the ciphertext header");
        }

        const std::string ivOverride = optionalHex(iv_hex);
        const std::string aadOverride = optionalHex(aad_hex);
        const std::string& iv = ivOverride.empty() ? container.iv : ivOverride;
        const std::string& aad = aadOverride.empty() ? container.aad : aadOverride;

        const std::string bytes =
            aes::decrypt(container.mode, key, iv, container.ciphertext, aad, container.tag, true);
        *plaintext = copyBytes(bytes, plaintext_len);
    });
}

void aesgui_free(void* buffer) { delete[] static_cast<unsigned char*>(buffer); }

const char* aesgui_last_error(void) { return g_lastError.c_str(); }

int aesgui_random_hex(int bytes, char* out, size_t out_size) {
    return guarded([&] {
        if (bytes < 0) throw std::runtime_error("byte count must be non-negative");
        if (out == nullptr) throw std::runtime_error("output buffer is null");
        const std::string hex = aes::toHex(aes::randomBytes(static_cast<std::size_t>(bytes)));
        writeText(hex, out, out_size, "random");
    });
}

int aesgui_record_info(const unsigned char* record, size_t record_len, char* mode,
                       size_t mode_size, char* iv_hex, size_t iv_hex_size, int* tag_len,
                       int* aad_len) {
    return guarded([&] {
        const aes::Record container = aes::parseRecord(inputBytes(record, record_len));
        writeText(aes::modeName(container.mode), mode, mode_size, "mode");
        writeText(aes::toHex(container.iv), iv_hex, iv_hex_size, "IV");
        if (tag_len != nullptr) *tag_len = static_cast<int>(container.tag.size());
        if (aad_len != nullptr) *aad_len = static_cast<int>(container.aad.size());
    });
}

int aesgui_mode_sizes(const char* mode, int* iv_len, int* tag_len, int* aead) {
    return guarded([&] {
        if (mode == nullptr) throw std::runtime_error("mode is required");
        const aes::Mode selected = aes::parseMode(mode);
        if (iv_len != nullptr) *iv_len = static_cast<int>(aes::ivSize(selected));
        if (tag_len != nullptr) *tag_len = static_cast<int>(aes::tagSize(selected));
        if (aead != nullptr) *aead = aes::isAead(selected) ? 1 : 0;
    });
}

}  // extern "C"
