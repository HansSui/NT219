// Minimal SHA-256 plus the state reconstruction needed for a length-extension demo.
#pragma once

#include <cstddef>
#include <string>

namespace sha256ext {

std::string sha256(const std::string& data);

// Given H(k || m) (32 raw bytes), the length of k, m and an appended suffix,
// returns H(k || m || pad || append) and the bytes m || pad || append.
std::string extend(const std::string& mac, std::size_t keyLength, const std::string& message,
                   const std::string& append, std::string& forgedMessage);

}  // namespace sha256ext
