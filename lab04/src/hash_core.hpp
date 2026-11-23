// SHA-2/SHA-3/SHAKE hashing through OpenSSL EVP, with streaming and mmap paths.
#pragma once

#include <istream>
#include <string>

namespace hashcore {

struct Algorithm {
    const char* name;
    const char* opensslName;
    int bits;
    bool xof;
};

// Case-insensitive; accepts sha3_256 as well as sha3-256.
const Algorithm& find(const std::string& name);
int outputBits(const std::string& name, bool hasOutlen, int outlen);

std::string hashBytes(const std::string& algorithm, const std::string& data, int outbits);
std::string hashStream(const std::string& algorithm, std::istream& input, int outbits);
std::string hashFile(const std::string& algorithm, const std::string& path, int outbits,
                     bool mmap);

// KAT file runner: prints PASS/FAIL per vector and returns the number of failures.
int runKat(const std::string& path);

std::string hex(const std::string& bytes);
std::string fromHex(const std::string& text);

}  // namespace hashcore
