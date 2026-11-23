#include "hash_core.hpp"

#include "io.hpp"
#include "minijson.hpp"
#include "util.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace hashcore {
namespace {

const Algorithm kAlgorithms[] = {
    {"sha224", "SHA2-224", 224, false},
    {"sha256", "SHA2-256", 256, false},
    {"sha384", "SHA2-384", 384, false},
    {"sha512", "SHA2-512", 512, false},
    {"sha3-224", "SHA3-224", 224, false},
    {"sha3-256", "SHA3-256", 256, false},
    {"sha3-384", "SHA3-384", 384, false},
    {"sha3-512", "SHA3-512", 512, false},
    {"shake128", "SHAKE128", 0, true},
    {"shake256", "SHAKE256", 0, true},
};

std::string normalize(const std::string& name) {
    std::string out;
    for (unsigned char c : name) {
        if (c == '_') out.push_back('-');
        else out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

struct MdDeleter {
    void operator()(EVP_MD* md) const { EVP_MD_free(md); }
};

std::unique_ptr<EVP_MD, MdDeleter> fetch(const Algorithm& algorithm) {
    EVP_MD* md = EVP_MD_fetch(nullptr, algorithm.opensslName, nullptr);
    if (!md) throw std::runtime_error(std::string("hash unavailable: ") + algorithm.name);
    return std::unique_ptr<EVP_MD, MdDeleter>(md);
}

std::string finish(EVP_MD_CTX* ctx, const Algorithm& algorithm, int outbits) {
    std::string out;
    if (algorithm.xof) {
        out.resize(static_cast<std::size_t>(outbits / 8));
        if (EVP_DigestFinalXOF(ctx, reinterpret_cast<unsigned char*>(&out[0]), out.size()) != 1)
            throw std::runtime_error("hash finalization failed");
    } else {
        out.resize(EVP_MD_get_size(EVP_MD_CTX_get0_md(ctx)));
        unsigned int length = 0;
        if (EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(&out[0]), &length) != 1)
            throw std::runtime_error("hash finalization failed");
        out.resize(length);
    }
    return out;
}

std::string digestBuffer(const Algorithm& algorithm, const EVP_MD* md, const char* data,
                         std::size_t size, int outbits) {
    EVP_MD_CTX* raw = EVP_MD_CTX_new();
    if (!raw) throw std::runtime_error("out of memory");
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(raw, EVP_MD_CTX_free);
    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), data, size) != 1)
        throw std::runtime_error("hash update failed");
    return finish(ctx.get(), algorithm, outbits);
}

}  // namespace

const Algorithm& find(const std::string& name) {
    const std::string key = normalize(name);
    for (const Algorithm& algorithm : kAlgorithms)
        if (key == algorithm.name) return algorithm;
    throw std::runtime_error("unsupported algorithm: " + name);
}

int outputBits(const std::string& name, bool hasOutlen, int outlen) {
    const Algorithm& algorithm = find(name);
    if (!algorithm.xof) {
        if (hasOutlen) throw std::runtime_error("--outlen is only valid for SHAKE128/SHAKE256");
        return algorithm.bits;
    }
    if (!hasOutlen) throw std::runtime_error("SHAKE requires --outlen in bits");
    // shortcut: XOF output capped at 8 MiB to bound allocation; raise the cap
    // if a legitimate use needs a longer stream.
    if (outlen <= 0 || outlen > 8 * 1024 * 1024 || outlen % 8 != 0)
        throw std::runtime_error("--outlen must be a positive multiple of 8, at most 8388608");
    return outlen;
}

std::string hashBytes(const std::string& algorithm, const std::string& data, int outbits) {
    const Algorithm& info = find(algorithm);
    auto md = fetch(info);
    return digestBuffer(info, md.get(), data.data(), data.size(), outbits);
}

std::string hashStream(const std::string& algorithm, std::istream& input, int outbits) {
    const Algorithm& info = find(algorithm);
    auto md = fetch(info);
    EVP_MD_CTX* raw = EVP_MD_CTX_new();
    if (!raw) throw std::runtime_error("out of memory");
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(raw, EVP_MD_CTX_free);
    if (EVP_DigestInit_ex(ctx.get(), md.get(), nullptr) != 1)
        throw std::runtime_error("hash initialization failed");

    char buffer[64 * 1024];
    while (input) {
        input.read(buffer, sizeof buffer);
        const std::streamsize got = input.gcount();
        if (got > 0 && EVP_DigestUpdate(ctx.get(), buffer, static_cast<std::size_t>(got)) != 1)
            throw std::runtime_error("hash update failed");
    }
    if (!input.eof()) throw std::runtime_error("failed while reading input");
    return finish(ctx.get(), info, outbits);
}

std::string hashFile(const std::string& algorithm, const std::string& path, int outbits,
                     bool mmap) {
    if (!mmap) {
        std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
        if (!file.good()) throw std::runtime_error("cannot open file: " + path);
        return hashStream(algorithm, file, outbits);
    }
    io::MappedFile mapped(path);
    const Algorithm& info = find(algorithm);
    auto md = fetch(info);
    EVP_MD_CTX* raw = EVP_MD_CTX_new();
    if (!raw) throw std::runtime_error("out of memory");
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(raw, EVP_MD_CTX_free);
    if (EVP_DigestInit_ex(ctx.get(), md.get(), nullptr) != 1)
        throw std::runtime_error("hash initialization failed");
    const char* data = mapped.data();
    for (std::size_t offset = 0; offset < mapped.size(); offset += 1024 * 1024) {
        const std::size_t chunk = std::min<std::size_t>(1024 * 1024, mapped.size() - offset);
        if (EVP_DigestUpdate(ctx.get(), data + offset, chunk) != 1)
            throw std::runtime_error("hash update failed");
    }
    return finish(ctx.get(), info, outbits);
}

std::string hex(const std::string& bytes) { return util::toHex(bytes); }
std::string fromHex(const std::string& text) { return util::fromHex(text); }

int runKat(const std::string& path) {
    const minijson::Value root = minijson::parse(util::readFile(path));
    const minijson::Value& vectors = root.at("vectors");
    if (vectors.type != minijson::Value::Type::Array)
        throw std::runtime_error("KAT file has no vectors array");

    int passed = 0;
    int failed = 0;
    for (std::size_t i = 0; i < vectors.array.size(); ++i) {
        const minijson::Value& vector = vectors.array[i];
        const std::string name = vector.getString("name");
        try {
            const std::string algorithm = vector.getString("algo");
            const std::string input = util::fromHex(vector.getString("input_hex"));
            bool hasOutlen = vector.has("outlen");
            int outlen = 0;
            if (hasOutlen) outlen = static_cast<int>(vector.at("outlen").number);
            const int outbits = outputBits(algorithm, hasOutlen, outlen);
            const std::string expected = util::fromHex(vector.getString("digest_hex"));
            const std::string actual = hashBytes(algorithm, input, outbits);
            if (actual == expected) {
                std::printf("PASS %s\n", name.c_str());
                ++passed;
            } else {
                std::printf("FAIL %s: digest mismatch\n", name.c_str());
                ++failed;
            }
        } catch (const std::exception& error) {
            std::printf("FAIL %s: %s\n", name.c_str(), error.what());
            ++failed;
        }
    }
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed;
}

}  // namespace hashcore
