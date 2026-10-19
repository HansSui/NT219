// Minimal JSON reader for the KAT vector file. No external dependency.
#pragma once

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minijson {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    bool has(const std::string& key) const {
        if (type != Type::Object) return false;
        for (const auto& item : object)
            if (item.first == key) return true;
        return false;
    }

    const Value& at(const std::string& key) const {
        if (type != Type::Object) throw std::runtime_error("JSON value is not an object");
        for (const auto& item : object)
            if (item.first == key) return item.second;
        throw std::runtime_error("missing JSON key '" + key + "'");
    }

    std::string getString(const std::string& key) const {
        const Value& value = at(key);
        if (value.type != Type::String) throw std::runtime_error("JSON key '" + key + "' is not a string");
        return value.string;
    }

    bool getBool(const std::string& key, bool fallback) const {
        if (!has(key)) return fallback;
        const Value& value = at(key);
        if (value.type != Type::Bool) throw std::runtime_error("JSON key '" + key + "' is not a bool");
        return value.boolean;
    }
};

namespace detail {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Value parse() {
        Value value = parseValue();
        skipSpace();
        if (position_ != text_.size()) fail("trailing characters");
        return value;
    }

private:
    const std::string& text_;
    std::size_t position_ = 0;

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("JSON parse error at byte " + std::to_string(position_) + ": " + message);
    }

    void skipSpace() {
        while (position_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[position_])))
            ++position_;
    }

    char peek() {
        skipSpace();
        if (position_ >= text_.size()) fail("unexpected end of input");
        return text_[position_];
    }

    bool consume(char expected) {
        skipSpace();
        if (position_ < text_.size() && text_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (!consume(expected)) fail(std::string("expected '") + expected + "'");
    }

    Value parseValue() {
        const char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') {
            Value value;
            value.type = Value::Type::String;
            value.string = parseString();
            return value;
        }
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        return parseNumber();
    }

    Value parseObject() {
        expect('{');
        Value value;
        value.type = Value::Type::Object;
        if (consume('}')) return value;
        while (true) {
            if (peek() != '"') fail("expected object key");
            std::string key = parseString();
            expect(':');
            value.object.emplace_back(std::move(key), parseValue());
            if (consume('}')) break;
            expect(',');
        }
        return value;
    }

    Value parseArray() {
        expect('[');
        Value value;
        value.type = Value::Type::Array;
        if (consume(']')) return value;
        while (true) {
            value.array.push_back(parseValue());
            if (consume(']')) break;
            expect(',');
        }
        return value;
    }

    Value parseBool() {
        Value value;
        value.type = Value::Type::Bool;
        if (text_.compare(position_, 4, "true") == 0) {
            position_ += 4;
            value.boolean = true;
        } else if (text_.compare(position_, 5, "false") == 0) {
            position_ += 5;
            value.boolean = false;
        } else {
            fail("invalid literal");
        }
        return value;
    }

    Value parseNull() {
        if (text_.compare(position_, 4, "null") != 0) fail("invalid literal");
        position_ += 4;
        return {};
    }

    static int hexDigit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static void appendUtf8(std::string& out, unsigned codepoint) {
        if (codepoint <= 0x7f) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (position_ < text_.size()) {
            char c = text_[position_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (position_ >= text_.size()) fail("unfinished escape");
            char escape = text_[position_++];
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > text_.size()) fail("short unicode escape");
                    unsigned codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        int digit = hexDigit(text_[position_++]);
                        if (digit < 0) fail("invalid unicode escape");
                        codepoint = (codepoint << 4) | static_cast<unsigned>(digit);
                    }
                    appendUtf8(out, codepoint);
                    break;
                }
                default: fail("unknown escape");
            }
        }
        fail("unterminated string");
    }

    Value parseNumber() {
        std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') ++position_;
        while (position_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[position_])) || text_[position_] == '.' ||
                text_[position_] == 'e' || text_[position_] == 'E' || text_[position_] == '+' ||
                text_[position_] == '-'))
            ++position_;
        if (start == position_) fail("expected value");
        Value value;
        value.type = Value::Type::Number;
        try {
            value.number = std::stod(text_.substr(start, position_ - start));
        } catch (const std::exception&) {
            fail("invalid number");
        }
        return value;
    }
};

}  // namespace detail

inline Value parse(const std::string& text) { return detail::Parser(text).parse(); }

}  // namespace minijson
