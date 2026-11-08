// Small dependency-free helpers shared by the Lab 3 core and tests.
#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace util {

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

inline std::string readFile(const std::string& path) {
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file.good()) throw std::runtime_error("cannot open file: " + path);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

inline void writeFile(const std::string& path, const std::string& data) {
    const std::filesystem::path target = std::filesystem::u8path(path);
    if (target.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
    }
    std::ofstream file(target, std::ios::binary);
    if (!file.good()) throw std::runtime_error("cannot open output file: " + path);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file.good()) throw std::runtime_error("failed while writing output file: " + path);
}

inline std::string toHex(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0x0f]);
    }
    return out;
}

inline std::string fromHex(const std::string& hex) {
    std::string out;
    int high = -1;
    for (char c : hex) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ':') continue;
        int value;
        if (c >= '0' && c <= '9') value = c - '0';
        else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
        else throw std::runtime_error("invalid hex character");
        if (high < 0) high = value;
        else {
            out.push_back(static_cast<char>((high << 4) | value));
            high = -1;
        }
    }
    require(high < 0, "hex input has an odd number of digits");
    return out;
}

inline std::string toBase64(const std::string& bytes) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const unsigned a = static_cast<unsigned char>(bytes[i]);
        const unsigned b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
        const unsigned c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
        out.push_back(table[a >> 2]);
        out.push_back(table[((a & 3) << 4) | (b >> 4)]);
        out.push_back(i + 1 < bytes.size() ? table[((b & 15) << 2) | (c >> 6)] : '=');
        out.push_back(i + 2 < bytes.size() ? table[c & 63] : '=');
    }
    return out;
}

inline std::string fromBase64(const std::string& text) {
    std::string compact;
    for (unsigned char c : text) {
        if (!std::isspace(c)) compact.push_back(static_cast<char>(c));
    }
    require(compact.size() % 4 == 0, "invalid base64 length");
    int value = 0;
    int bits = -8;
    std::size_t padding = 0;
    std::string out;
    for (std::size_t i = 0; i < compact.size(); ++i) {
        const char c = compact[i];
        if (c == '=') {
            require(i + 2 >= compact.size(), "invalid base64 padding");
            ++padding;
            continue;
        }
        require(padding == 0, "invalid base64 padding");
        int digit;
        if (c >= 'A' && c <= 'Z') digit = c - 'A';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 26;
        else if (c >= '0' && c <= '9') digit = c - '0' + 52;
        else if (c == '+') digit = 62;
        else if (c == '/') digit = 63;
        else throw std::runtime_error("invalid base64 character");
        value = (value << 6) | digit;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((value >> bits) & 0xff));
            bits -= 8;
        }
    }
    require(padding <= 2, "invalid base64 padding");
    return out;
}

inline std::string jsonEscape(const std::string& text) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(digits[c >> 4]);
                    out.push_back(digits[c & 0x0f]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

}  // namespace util
