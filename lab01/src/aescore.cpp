#include "aescore.hpp"

#include "cryptopp_all.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace aes {
namespace {

constexpr const char* kMagic = "AESL1";
constexpr unsigned char kVersion = 1;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void checkKey(Mode mode, const std::string& key) {
    const std::size_t n = key.size();
    const bool ok = (mode == Mode::XTS) ? (n == 32 || n == 64) : (n == 16 || n == 24 || n == 32);
    require(ok, "invalid AES key length (" + std::to_string(n) +
                    " bytes); expected 16/24/32, or 32/64 for XTS");
}

void checkIv(Mode mode, const std::string& iv) {
    const bool ok = (mode == Mode::CCM) ? (iv.size() >= 7 && iv.size() <= 13)
                                        : (iv.size() == ivSize(mode));
    require(ok, "invalid IV/nonce length for " + modeName(mode) + ": got " +
                    std::to_string(iv.size()) + " bytes");
}

bool validIvLength(Mode mode, std::size_t length) {
    return (mode == Mode::CCM) ? (length >= 7 && length <= 13) : (length == ivSize(mode));
}

void checkData(Mode mode, const std::string& data, bool blockAligned) {
    if (mode == Mode::XTS) {
        require(data.size() >= 16, "XTS requires at least 16 bytes");
    }
    if (blockAligned && (mode == Mode::ECB || mode == Mode::CBC)) {
        require(data.size() % 16 == 0, "data length is not a multiple of the AES block size");
    }
}

template <class Cipher>
std::string transform(Cipher& cipher, const std::string& input,
                      CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme padding) {
    std::string output;
    CryptoPP::StreamTransformationFilter filter(cipher, new CryptoPP::StringSink(output), padding);
    filter.Put(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
    filter.MessageEnd();
    return output;
}

template <class Cipher>
std::string aeadEncrypt(Cipher& cipher, const std::string& plaintext, const std::string& aad,
                        std::size_t tagLength) {
    std::string output;
    CryptoPP::AuthenticatedEncryptionFilter filter(
        cipher, new CryptoPP::StringSink(output), false, static_cast<int>(tagLength));
    if (!aad.empty()) {
        filter.ChannelPut(CryptoPP::AAD_CHANNEL,
                          reinterpret_cast<const CryptoPP::byte*>(aad.data()), aad.size());
        filter.ChannelMessageEnd(CryptoPP::AAD_CHANNEL);
    }
    filter.ChannelPut(CryptoPP::DEFAULT_CHANNEL,
                      reinterpret_cast<const CryptoPP::byte*>(plaintext.data()), plaintext.size());
    filter.ChannelMessageEnd(CryptoPP::DEFAULT_CHANNEL);
    return output;
}

template <class Cipher>
std::string aeadDecrypt(Cipher& cipher, const std::string& ciphertext, const std::string& aad,
                        const std::string& tag, std::size_t tagLength) {
    std::string output;
    std::string input = ciphertext + tag;
    try {
        CryptoPP::AuthenticatedDecryptionFilter filter(
            cipher, new CryptoPP::StringSink(output),
            CryptoPP::AuthenticatedDecryptionFilter::THROW_EXCEPTION,
            static_cast<int>(tagLength));
        if (!aad.empty()) {
            filter.ChannelPut(CryptoPP::AAD_CHANNEL,
                              reinterpret_cast<const CryptoPP::byte*>(aad.data()), aad.size());
            filter.ChannelMessageEnd(CryptoPP::AAD_CHANNEL);
        }
        filter.ChannelPut(CryptoPP::DEFAULT_CHANNEL,
                          reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
        filter.ChannelMessageEnd(CryptoPP::DEFAULT_CHANNEL);
    } catch (const CryptoPP::Exception& e) {
        throw std::runtime_error(std::string("authentication failed: ") + e.what());
    }
    return output;
}

void appendU32(std::string& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
}

std::uint32_t readU32(const std::string& in, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(in[offset + i])) << (8 * i);
    }
    return value;
}

std::string trim(const std::string& text) {
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

}  // namespace

Mode parseMode(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "ecb") return Mode::ECB;
    if (lower == "cbc") return Mode::CBC;
    if (lower == "ofb") return Mode::OFB;
    if (lower == "cfb") return Mode::CFB;
    if (lower == "ctr") return Mode::CTR;
    if (lower == "xts") return Mode::XTS;
    if (lower == "ccm") return Mode::CCM;
    if (lower == "gcm") return Mode::GCM;
    throw std::runtime_error("unknown mode '" + name + "'");
}

std::string modeName(Mode mode) {
    switch (mode) {
        case Mode::ECB: return "ECB";
        case Mode::CBC: return "CBC";
        case Mode::OFB: return "OFB";
        case Mode::CFB: return "CFB";
        case Mode::CTR: return "CTR";
        case Mode::XTS: return "XTS";
        case Mode::CCM: return "CCM";
        case Mode::GCM: return "GCM";
    }
    return "?";
}

bool isAead(Mode mode) { return mode == Mode::CCM || mode == Mode::GCM; }

std::size_t ivSize(Mode mode) {
    switch (mode) {
        case Mode::ECB: return 0;
        case Mode::GCM: return 12;
        // shortcut: 12-byte CCM nonce means L=3 and a 16 MiB message cap; use 11 bytes (L=4) for larger payloads.
        case Mode::CCM: return 12;
        default: return 16;
    }
}

std::size_t tagSize(Mode mode) {
    if (mode == Mode::GCM) return 16;
    if (mode == Mode::CCM) return 8;
    return 0;
}

std::string fromHex(const std::string& hex) {
    std::string out;
    int high = -1;
    for (char c : hex) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ':') continue;
        int value;
        if (c >= '0' && c <= '9') value = c - '0';
        else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
        else throw std::runtime_error("invalid hex character in input");
        if (high < 0) high = value;
        else {
            out.push_back(static_cast<char>((high << 4) | value));
            high = -1;
        }
    }
    require(high < 0, "hex input has an odd number of digits");
    return out;
}

std::string toHex(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0x0f]);
    }
    return out;
}

std::string randomBytes(std::size_t count) {
    if (count == 0) return {};
    CryptoPP::AutoSeededRandomPool rng;
    std::string out(count, '\0');
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&out[0]), count);
    return out;
}

std::string toBase64(const std::string& bytes) {
    std::string out;
    CryptoPP::StringSource(
        bytes, true, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(out), false));
    return out;
}

std::string sha256Hex(const std::string& bytes) {
    std::string digest;
    CryptoPP::SHA256 hash;
    CryptoPP::StringSource(
        bytes, true, new CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new CryptoPP::StringSink(digest), false)));
    return digest;
}

std::string loadKeyFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "cannot open key file: " + path);
    std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string text = trim(raw);
    if (text.size() >= 4 && (text.compare(0, 4, "hex:") == 0 || text.compare(0, 4, "HEX:") == 0)) {
        return fromHex(text.substr(4));
    }
    return raw;
}

Encrypted encrypt(Mode mode, const std::string& key, const std::string& iv,
                  const std::string& plaintext, const std::string& aad, bool pkcs7) {
    checkKey(mode, key);
    checkIv(mode, iv);
    require(isAead(mode) || aad.empty(), "AAD is only accepted with AEAD modes");
    checkData(mode, plaintext, !pkcs7);

    const auto pad = pkcs7 ? CryptoPP::StreamTransformationFilter::PKCS_PADDING
                           : CryptoPP::StreamTransformationFilter::NO_PADDING;

    const auto* keyPtr = reinterpret_cast<const CryptoPP::byte*>(key.data());
    const auto* ivPtr = reinterpret_cast<const CryptoPP::byte*>(iv.data());
    const auto keyLen = key.size();
    const auto ivLen = iv.size();

    switch (mode) {
        case Mode::ECB: {
            CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption cipher;
            cipher.SetKey(keyPtr, keyLen);
            return {transform(cipher, plaintext, pad), {}};
        }
        case Mode::CBC: {
            CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return {transform(cipher, plaintext, pad), {}};
        }
        case Mode::OFB: {
            CryptoPP::OFB_Mode<CryptoPP::AES>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return {transform(cipher, plaintext, CryptoPP::StreamTransformationFilter::NO_PADDING), {}};
        }
        case Mode::CFB: {
            CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return {transform(cipher, plaintext, CryptoPP::StreamTransformationFilter::NO_PADDING), {}};
        }
        case Mode::CTR: {
            CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return {transform(cipher, plaintext, CryptoPP::StreamTransformationFilter::NO_PADDING), {}};
        }
        case Mode::XTS: {
            CryptoPP::XTS_Mode<CryptoPP::AES>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return {transform(cipher, plaintext, CryptoPP::StreamTransformationFilter::NO_PADDING), {}};
        }
        case Mode::GCM: {
            CryptoPP::GCM<CryptoPP::AES>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            std::string combined = aeadEncrypt(cipher, plaintext, aad, tagSize(mode));
            return {combined.substr(0, combined.size() - tagSize(mode)),
                    combined.substr(combined.size() - tagSize(mode))};
        }
        case Mode::CCM: {
            CryptoPP::CCM<CryptoPP::AES, 8>::Encryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            cipher.SpecifyDataLengths(aad.size(), plaintext.size(), 0);
            std::string combined = aeadEncrypt(cipher, plaintext, aad, tagSize(mode));
            return {combined.substr(0, combined.size() - tagSize(mode)),
                    combined.substr(combined.size() - tagSize(mode))};
        }
    }
    throw std::runtime_error("unsupported mode");
}

std::string decrypt(Mode mode, const std::string& key, const std::string& iv,
                    const std::string& ciphertext, const std::string& aad,
                    const std::string& tag, bool pkcs7) {
    checkKey(mode, key);
    checkIv(mode, iv);
    require(isAead(mode) || (aad.empty() && tag.empty()),
            "AAD/tag are only accepted with AEAD modes");
    if (isAead(mode)) {
        require(tag.size() == tagSize(mode), "invalid authentication tag length for " + modeName(mode));
    }
    checkData(mode, ciphertext, true);

    const auto pad = pkcs7 ? CryptoPP::StreamTransformationFilter::PKCS_PADDING
                           : CryptoPP::StreamTransformationFilter::NO_PADDING;

    const auto* keyPtr = reinterpret_cast<const CryptoPP::byte*>(key.data());
    const auto* ivPtr = reinterpret_cast<const CryptoPP::byte*>(iv.data());
    const auto keyLen = key.size();
    const auto ivLen = iv.size();

    switch (mode) {
        case Mode::ECB: {
            CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption cipher;
            cipher.SetKey(keyPtr, keyLen);
            return transform(cipher, ciphertext, pad);
        }
        case Mode::CBC: {
            CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return transform(cipher, ciphertext, pad);
        }
        case Mode::OFB: {
            CryptoPP::OFB_Mode<CryptoPP::AES>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return transform(cipher, ciphertext, CryptoPP::StreamTransformationFilter::NO_PADDING);
        }
        case Mode::CFB: {
            CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return transform(cipher, ciphertext, CryptoPP::StreamTransformationFilter::NO_PADDING);
        }
        case Mode::CTR: {
            CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return transform(cipher, ciphertext, CryptoPP::StreamTransformationFilter::NO_PADDING);
        }
        case Mode::XTS: {
            CryptoPP::XTS_Mode<CryptoPP::AES>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return transform(cipher, ciphertext, CryptoPP::StreamTransformationFilter::NO_PADDING);
        }
        case Mode::GCM: {
            CryptoPP::GCM<CryptoPP::AES>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            return aeadDecrypt(cipher, ciphertext, aad, tag, tagSize(mode));
        }
        case Mode::CCM: {
            CryptoPP::CCM<CryptoPP::AES, 8>::Decryption cipher;
            cipher.SetKeyWithIV(keyPtr, keyLen, ivPtr, ivLen);
            cipher.SpecifyDataLengths(aad.size(), ciphertext.size(), 0);
            return aeadDecrypt(cipher, ciphertext, aad, tag, tagSize(mode));
        }
    }
    throw std::runtime_error("unsupported mode");
}

std::string serialize(const Record& record) {
    require(validIvLength(record.mode, record.iv.size()),
            "record IV length does not match mode");
    require(record.tag.size() == tagSize(record.mode), "record tag length does not match mode");
    require(isAead(record.mode) || record.aad.empty(), "record AAD requires an AEAD mode");

    std::string out(kMagic, 5);
    out.push_back(static_cast<char>(kVersion));
    out.push_back(static_cast<char>(static_cast<int>(record.mode)));
    out.push_back(static_cast<char>(record.iv.size()));
    out.push_back(static_cast<char>(record.tag.size()));
    appendU32(out, static_cast<std::uint32_t>(record.aad.size()));
    out += record.iv;
    out += record.tag;
    out += record.aad;
    out += record.ciphertext;
    return out;
}

Record parseRecord(const std::string& bytes) {
    require(bytes.size() >= 13, "input is too short to contain an aestool header");
    require(bytes.compare(0, 5, kMagic) == 0, "bad ciphertext magic (not an aestool file)");
    require(static_cast<unsigned char>(bytes[5]) == kVersion, "unsupported ciphertext version");

    const int modeId = static_cast<unsigned char>(bytes[6]);
    require(modeId >= 0 && modeId <= static_cast<int>(Mode::GCM), "invalid mode in ciphertext header");
    Record record;
    record.mode = static_cast<Mode>(modeId);

    const std::size_t ivLen = static_cast<unsigned char>(bytes[7]);
    const std::size_t tagLen = static_cast<unsigned char>(bytes[8]);
    const std::size_t aadLen = readU32(bytes, 9);
    require(validIvLength(record.mode, ivLen),
            "ciphertext header has an invalid IV length");
    require(tagLen == tagSize(record.mode), "ciphertext header has an invalid tag length");
    require(isAead(record.mode) || aadLen == 0, "ciphertext header has AAD for a non-AEAD mode");

    const std::size_t offset = 13;
    require(ivLen + tagLen + aadLen <= bytes.size() - offset,
            "ciphertext header lengths are out of range");

    record.iv = bytes.substr(offset, ivLen);
    record.tag = bytes.substr(offset + ivLen, tagLen);
    record.aad = bytes.substr(offset + ivLen + tagLen, aadLen);
    record.ciphertext = bytes.substr(offset + ivLen + tagLen + aadLen);
    return record;
}

}  // namespace aes
