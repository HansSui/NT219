#include "aescore.hpp"
#include "minijson.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string command;
    std::string mode;
    std::string inPath;
    std::string text;
    std::string outPath;
    std::string keyPath;
    std::string keyHex;
    std::string iv;
    std::string nonce;
    std::string aadPath;
    std::string aadText;
    std::string encode;
    std::string katPath;

    bool hasIn = false;
    bool hasText = false;
    bool hasOut = false;
    bool hasKey = false;
    bool hasKeyHex = false;
    bool hasIv = false;
    bool hasNonce = false;
    bool hasAad = false;
    bool hasAadText = false;
    bool hasEncode = false;
    bool hasMode = false;
    bool hasKat = false;

    bool aead = false;
    bool allowEcb = false;
    bool verbose = false;

    std::size_t size = 0;
    std::size_t runs = 0;
    double warmup = -1.0;
    int threads = 0;
};

const char* kUsage = R"(aestool - AES symmetric encryption lab tool

Usage:
  aestool encrypt --mode MODE [--in FILE | --text STRING] [--out FILE]
                  (--key FILE | --key-hex HEX) [--iv FILE|HEX] [--nonce FILE|HEX]
                  [--aead] [--aad FILE | --aad-text STRING]
                  [--encode hex|base64|raw] [--allow-ecb] [--verbose]
  aestool decrypt --mode MODE [--in FILE | --text HEX] [--out FILE]
                  (--key FILE | --key-hex HEX) [--iv FILE|HEX] [--nonce FILE|HEX]
                  [--aead] [--aad FILE | --aad-text STRING]
                  [--encode hex|base64|raw] [--allow-ecb] [--verbose]
  aestool bench --mode MODE (--key FILE | --key-hex HEX) [--size BYTES]
                [--runs N] [--warmup SECONDS] [--out CSV] [--allow-ecb]
  aestool --kat vectors.json
  aestool help

Modes: ecb cbc ofb cfb ctr xts ccm gcm
AEAD:  gcm and ccm require --aead. GCM nonce is 12 bytes; CCM accepts 7..13.
Keys:  16/24/32 bytes (AES-128/192/256); XTS uses 32 or 64 bytes.
)";

[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }

std::string readAll(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) fail("cannot open input file: " + path);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeAll(const std::string& path, const std::string& data) {
    std::filesystem::path target(path);
    if (target.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
    }
    std::ofstream file(path, std::ios::binary);
    if (!file.good()) fail("cannot open output file: " + path);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file.good()) fail("failed while writing output file: " + path);
}

bool looksLikeHex(const std::string& text) {
    std::size_t digits = 0;
    for (char c : text) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            ++digits;
        } else if (!std::isspace(static_cast<unsigned char>(c)) && c != ':') {
            return false;
        }
    }
    return digits > 0 && digits % 2 == 0;
}

std::string readValueOrFile(const std::string& value) {
    std::ifstream file(value, std::ios::binary);
    if (file.good()) {
        file.close();
        return readAll(value);
    }
    return aes::fromHex(value);
}

std::string readKey(const Options& options) {
    if (options.hasKey && options.hasKeyHex) fail("use either --key or --key-hex, not both");
    if (options.hasKeyHex) return aes::fromHex(options.keyHex);
    if (options.hasKey) return aes::loadKeyFile(options.keyPath);
    fail("a key is required: pass --key FILE or --key-hex HEX");
}

std::string readInput(const Options& options, bool decrypting) {
    if (options.hasIn && options.hasText) fail("use either --in or --text, not both");
    if (options.hasIn) return readAll(options.inPath);
    if (!options.hasText) fail("an input is required: pass --in FILE or --text STRING");
    if (!decrypting) return options.text;
    return looksLikeHex(options.text) ? aes::fromHex(options.text) : options.text;
}

std::string readAad(const Options& options) {
    if (options.hasAad && options.hasAadText) fail("use either --aad or --aad-text, not both");
    if (options.hasAad) return readAll(options.aadPath);
    if (options.hasAadText) return options.aadText;
    return {};
}

void checkEncode(const Options& options) {
    if (!options.hasEncode) return;
    const std::set<std::string> allowed = {"hex", "base64", "raw"};
    if (!allowed.count(options.encode)) fail("--encode must be hex, base64 or raw");
}

std::string encodeBytes(const std::string& data, const Options& options, bool screen, bool cipher) {
    if (options.hasEncode && options.encode == "hex") return aes::toHex(data);
    if (options.hasEncode && options.encode == "base64") return aes::toBase64(data);
    if (options.hasEncode && options.encode == "raw") return data;
    if (screen && cipher) return aes::toHex(data);
    return data;
}

void writeResult(const std::string& data, const Options& options, bool cipher) {
    const bool screen = !options.hasOut;
    const std::string encoded = encodeBytes(data, options, screen, cipher);
    if (screen) {
        std::cout.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        if (cipher || options.hasEncode) std::cout << '\n';
    } else {
        writeAll(options.outPath, encoded);
    }
}

void enforceEcbPolicy(aes::Mode mode, std::size_t payloadSize, bool allowed) {
    if (mode != aes::Mode::ECB) return;
    std::cerr << "WARNING: ECB mode has no semantic security and leaks plaintext structure.\n";
    if (payloadSize > 16384 && !allowed) {
        fail("ECB is blocked for payloads larger than 16 KiB; pass --allow-ecb to override");
    }
}

std::string keyFingerprint(const std::string& key) { return aes::sha256Hex(key); }

// shortcut: reuse registry is a per-working-directory file, not a locked database;
// it catches accidental reuse in normal CLI use, upgrade to a per-key store for multi-process use.
const char* kNonceRegistry = ".aestool_nonces";

void checkNonceReuse(aes::Mode mode, const std::string& key, const std::string& iv) {
    if (mode != aes::Mode::CTR && mode != aes::Mode::CCM && mode != aes::Mode::GCM) return;
    const std::string expected = keyFingerprint(key) + " " + aes::toHex(iv);
    std::ifstream registry(kNonceRegistry);
    std::string line;
    while (std::getline(registry, line)) {
        if (line == expected) {
            fail("nonce reuse rejected: this key and nonce were already used");
        }
    }
}

void recordNonce(aes::Mode mode, const std::string& key, const std::string& iv) {
    if (mode != aes::Mode::CTR && mode != aes::Mode::CCM && mode != aes::Mode::GCM) return;
    std::ofstream registry(kNonceRegistry, std::ios::app);
    if (!registry.good()) fail("cannot write nonce reuse registry: " + std::string(kNonceRegistry));
    registry << keyFingerprint(key) << ' ' << aes::toHex(iv) << '\n';
    if (!registry.good()) fail("failed while writing nonce reuse registry");
}

void printVerbose(const std::string& text, bool verbose) {
    if (verbose) std::cerr << text << '\n';
}

struct Stats {
    double mean = 0;
    double median = 0;
    double stddev = 0;
    double ci95 = 0;
};

Stats computeStats(const std::vector<double>& values) {
    Stats stats;
    if (values.empty()) return stats;
    stats.mean = 0;
    for (double value : values) stats.mean += value;
    stats.mean /= static_cast<double>(values.size());

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();
    stats.median = (n % 2 == 1) ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;

    if (n > 1) {
        double sum = 0;
        for (double value : values) sum += (value - stats.mean) * (value - stats.mean);
        stats.stddev = std::sqrt(sum / static_cast<double>(n - 1));
        stats.ci95 = 1.96 * stats.stddev / std::sqrt(static_cast<double>(n));
    }
    return stats;
}

double toMbps(std::size_t bytes, double microseconds) {
    if (microseconds <= 0) return 0;
    return static_cast<double>(bytes) / microseconds;
}

void printStats(const std::string& label, const Stats& stats, const std::string& unit) {
    std::cout << label << ": mean=" << std::fixed << std::setprecision(3) << stats.mean << ' '
              << unit << " median=" << stats.median << ' ' << unit
              << " stddev=" << stats.stddev << ' ' << unit
              << " ci95=" << stats.ci95 << ' ' << unit << '\n';
}

int runBench(const Options& options) {
    if (!options.hasMode) fail("bench requires --mode");
    const aes::Mode mode = aes::parseMode(options.mode);
    const std::string key = readKey(options);
    const std::size_t size = options.size == 0 ? 1024 : options.size;
    const std::size_t runs = options.runs == 0 ? 30 : options.runs;
    const double warmup = options.warmup < 0 ? 1.0 : options.warmup;

    enforceEcbPolicy(mode, size, options.allowEcb);
    if (options.aead && !aes::isAead(mode)) fail("--aead is only valid with ccm or gcm");

    const std::string plaintext = aes::randomBytes(size);
    const std::string baseIv = mode == aes::Mode::ECB ? std::string()
                                                      : aes::randomBytes(aes::ivSize(mode));
    const std::string aad = aes::isAead(mode) ? "aestool-benchmark" : std::string();

    // A fresh counter-derived IV per operation keeps the benchmark free of nonce reuse.
    std::size_t nonceCounter = 0;
    auto nextIv = [&]() -> std::string {
        if (mode == aes::Mode::ECB) return {};
        std::string iv = baseIv;
        for (int i = 0; i < 4; ++i) {
            iv[iv.size() - 1 - static_cast<std::size_t>(i)] =
                static_cast<char>((nonceCounter >> (8 * i)) & 0xff);
        }
        ++nonceCounter;
        return iv;
    };

    auto encryptOnce = [&](const std::string& iv) {
        return aes::encrypt(mode, key, iv, plaintext, aad);
    };
    auto decryptOnce = [&](const std::string& iv, const aes::Encrypted& encrypted) {
        return aes::decrypt(mode, key, iv, encrypted.ciphertext, aad, encrypted.tag);
    };

    {
        const std::string iv = nextIv();
        if (decryptOnce(iv, encryptOnce(iv)) != plaintext) fail("benchmark self-check failed");
    }

    const auto warmupEnd = std::chrono::steady_clock::now() +
                           std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                               std::chrono::duration<double>(warmup));
    do {
        const std::string iv = nextIv();
        (void)decryptOnce(iv, encryptOnce(iv));
    } while (std::chrono::steady_clock::now() < warmupEnd);

    std::vector<double> encryptUs;
    std::vector<double> decryptUs;
    std::vector<double> encryptMbps;
    std::vector<double> decryptMbps;
    encryptUs.reserve(runs);
    decryptUs.reserve(runs);
    encryptMbps.reserve(runs);
    decryptMbps.reserve(runs);

    struct Row {
        std::size_t run;
        double encryptUs;
        double decryptUs;
    };
    std::vector<Row> rows;
    rows.reserve(runs);

    for (std::size_t run = 0; run < runs; ++run) {
        const std::string iv = nextIv();
        const auto encryptStart = std::chrono::steady_clock::now();
        const aes::Encrypted encrypted = encryptOnce(iv);
        const auto encryptEnd = std::chrono::steady_clock::now();
        const std::string recovered = decryptOnce(iv, encrypted);
        const auto decryptEnd = std::chrono::steady_clock::now();
        if (recovered != plaintext) fail("benchmark round-trip self-check failed");

        const double encUs =
            std::chrono::duration<double, std::micro>(encryptEnd - encryptStart).count();
        const double decUs =
            std::chrono::duration<double, std::micro>(decryptEnd - encryptEnd).count();
        encryptUs.push_back(encUs);
        decryptUs.push_back(decUs);
        encryptMbps.push_back(toMbps(size, encUs));
        decryptMbps.push_back(toMbps(size, decUs));
        rows.push_back({run + 1, encUs, decUs});
    }

    const Stats encStats = computeStats(encryptUs);
    const Stats decStats = computeStats(decryptUs);
    const Stats encThroughput = computeStats(encryptMbps);
    const Stats decThroughput = computeStats(decryptMbps);

    std::cout << "mode=" << aes::modeName(mode) << " size=" << size << " runs=" << runs
              << " warmup=" << warmup << "s\n";
    printStats("encrypt_us", encStats, "us");
    printStats("decrypt_us", decStats, "us");
    printStats("encrypt_MBps", encThroughput, "MB/s");
    printStats("decrypt_MBps", decThroughput, "MB/s");

    if (options.hasOut) {
        std::ostringstream csv;
        csv << "run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,"
               "throughput_decrypt_MBps\n";
        csv << std::fixed << std::setprecision(6);
        for (const Row& row : rows) {
            csv << row.run << ',' << aes::modeName(mode) << ',' << size << ',' << row.encryptUs
                << ',' << row.decryptUs << ',' << toMbps(size, row.encryptUs) << ','
                << toMbps(size, row.decryptUs) << '\n';
        }
        writeAll(options.outPath, csv.str());
    }
    return 0;
}

int runKat(const std::string& path) {
    const minijson::Value root = minijson::parse(readAll(path));
    const minijson::Value& vectors = root.at("vectors");
    if (vectors.type != minijson::Value::Type::Array) fail("KAT file must contain a vectors array");

    int passed = 0;
    int failed = 0;
    for (std::size_t i = 0; i < vectors.array.size(); ++i) {
        const minijson::Value& vector = vectors.array[i];
        const std::string name =
            vector.has("name") ? vector.getString("name") : ("vector " + std::to_string(i + 1));
        try {
            const aes::Mode mode = aes::parseMode(vector.getString("mode"));
            const std::string key = aes::fromHex(vector.getString("key"));
            const std::string iv = aes::fromHex(vector.getString("iv"));
            const std::string plaintext = aes::fromHex(vector.getString("plaintext"));
            const std::string expectedCiphertext = aes::fromHex(vector.getString("ciphertext"));
            const std::string aad = vector.has("aad") ? aes::fromHex(vector.getString("aad")) : "";
            const std::string expectedTag = vector.has("tag") ? aes::fromHex(vector.getString("tag")) : "";

            const aes::Encrypted encrypted = aes::encrypt(mode, key, iv, plaintext, aad, false);
            if (encrypted.ciphertext != expectedCiphertext)
                fail("ciphertext mismatch");
            if (encrypted.tag != expectedTag) fail("tag mismatch");

            const std::string recovered =
                aes::decrypt(mode, key, iv, expectedCiphertext, aad, expectedTag, false);
            if (recovered != plaintext) fail("decryption mismatch");

            std::cout << "PASS " << name << '\n';
            ++passed;
        } catch (const std::exception& error) {
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
            ++failed;
        }
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

int runEncrypt(const Options& options) {
    if (!options.hasMode) fail("encrypt requires --mode");
    const aes::Mode mode = aes::parseMode(options.mode);
    const std::string key = readKey(options);
    const std::string plaintext = readInput(options, false);
    const std::string aad = readAad(options);

    if (aes::isAead(mode) && !options.aead)
        fail("mode " + aes::modeName(mode) + " is AEAD; pass --aead");
    if (!aes::isAead(mode) && options.aead) fail("--aead is only valid with ccm or gcm");
    if (!aes::isAead(mode) && !aad.empty()) fail("AAD is only valid with ccm or gcm");
    enforceEcbPolicy(mode, plaintext.size(), options.allowEcb);

    if (options.hasIv && options.hasNonce) fail("use either --iv or --nonce, not both");
    std::string iv;
    if (mode == aes::Mode::ECB) {
        iv.clear();
    } else if (options.hasIv) {
        iv = readValueOrFile(options.iv);
    } else if (options.hasNonce) {
        iv = readValueOrFile(options.nonce);
    } else {
        iv = aes::randomBytes(aes::ivSize(mode));
    }

    checkNonceReuse(mode, key, iv);
    const aes::Encrypted encrypted = aes::encrypt(mode, key, iv, plaintext, aad);
    const aes::Record record{mode, iv, aad, encrypted.tag, encrypted.ciphertext};
    const std::string container = aes::serialize(record);
    recordNonce(mode, key, iv);

    printVerbose("mode=" + aes::modeName(mode) + " key=" + std::to_string(key.size() * 8) +
                     " bits iv=" + aes::toHex(iv) + " payload=" +
                     std::to_string(plaintext.size()) + " bytes",
                 options.verbose);
    writeResult(container, options, true);
    return 0;
}

int runDecrypt(const Options& options) {
    if (!options.hasMode) fail("decrypt requires --mode (it must match the ciphertext header)");
    const std::string key = readKey(options);
    const std::string input = readInput(options, true);
    const aes::Record record = aes::parseRecord(input);

    const aes::Mode mode = aes::parseMode(options.mode);
    if (mode != record.mode) fail("--mode does not match the ciphertext header");
    if (aes::isAead(mode) && !options.aead)
        fail("mode " + aes::modeName(mode) + " is AEAD; pass --aead");
    if (!aes::isAead(mode) && options.aead) fail("--aead is only valid with ccm or gcm");

    const std::string aad = (options.hasAad || options.hasAadText) ? readAad(options) : record.aad;
    if (options.hasIv && options.hasNonce) fail("use either --iv or --nonce, not both");
    std::string iv = record.iv;
    if (options.hasIv) iv = readValueOrFile(options.iv);
    else if (options.hasNonce) iv = readValueOrFile(options.nonce);

    enforceEcbPolicy(mode, record.ciphertext.size(), options.allowEcb);
    const std::string plaintext =
        aes::decrypt(mode, key, iv, record.ciphertext, aad, record.tag);

    printVerbose("mode=" + aes::modeName(mode) + " key=" + std::to_string(key.size() * 8) +
                     " bits payload=" + std::to_string(plaintext.size()) + " bytes",
                 options.verbose);
    writeResult(plaintext, options, false);
    return 0;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string argument = argv[i];
        std::string value;
        bool inlineValue = false;
        if (argument.rfind("--", 0) == 0) {
            const auto equal = argument.find('=');
            if (equal != std::string::npos) {
                value = argument.substr(equal + 1);
                argument = argument.substr(0, equal);
                inlineValue = true;
            }
        }

        auto takeValue = [&]() -> std::string {
            if (inlineValue) return value;
            if (i + 1 >= argc) fail("missing value for " + argument);
            return argv[++i];
        };

        if (argument == "--help" || argument == "-h") {
            options.command = "help";
        } else if (argument == "--aead") {
            options.aead = true;
        } else if (argument == "--allow-ecb") {
            options.allowEcb = true;
        } else if (argument == "--verbose") {
            options.verbose = true;
        } else if (argument == "--mode") {
            options.mode = takeValue();
            options.hasMode = true;
        } else if (argument == "--in") {
            options.inPath = takeValue();
            options.hasIn = true;
        } else if (argument == "--text") {
            options.text = takeValue();
            options.hasText = true;
        } else if (argument == "--out") {
            options.outPath = takeValue();
            options.hasOut = true;
        } else if (argument == "--key") {
            options.keyPath = takeValue();
            options.hasKey = true;
        } else if (argument == "--key-hex") {
            options.keyHex = takeValue();
            options.hasKeyHex = true;
        } else if (argument == "--iv") {
            options.iv = takeValue();
            options.hasIv = true;
        } else if (argument == "--nonce") {
            options.nonce = takeValue();
            options.hasNonce = true;
        } else if (argument == "--aad") {
            options.aadPath = takeValue();
            options.hasAad = true;
        } else if (argument == "--aad-text") {
            options.aadText = takeValue();
            options.hasAadText = true;
        } else if (argument == "--encode") {
            options.encode = takeValue();
            options.hasEncode = true;
        } else if (argument == "--kat") {
            options.katPath = takeValue();
            options.hasKat = true;
        } else if (argument == "--size") {
            try {
                options.size = static_cast<std::size_t>(std::stoull(takeValue()));
            } catch (const std::exception&) {
                fail("--size must be a positive integer");
            }
        } else if (argument == "--runs") {
            try {
                options.runs = static_cast<std::size_t>(std::stoull(takeValue()));
            } catch (const std::exception&) {
                fail("--runs must be a positive integer");
            }
        } else if (argument == "--warmup") {
            try {
                options.warmup = std::stod(takeValue());
            } catch (const std::exception&) {
                fail("--warmup must be a number of seconds");
            }
        } else if (argument == "--threads") {
            // shortcut: accepted for CLI compatibility; crypto operations remain single-threaded.
            try {
                options.threads = std::stoi(takeValue());
            } catch (const std::exception&) {
                fail("--threads must be an integer");
            }
        } else if (argument.rfind("--", 0) == 0) {
            fail("unknown option: " + argument);
        } else if (options.command.empty()) {
            options.command = argument;
        } else if (options.command == "kat" && !options.hasKat && options.katPath.empty()) {
            options.katPath = argument;
        } else {
            fail("unexpected argument: " + argument);
        }
    }
    if (options.hasKat && options.command.empty()) options.command = "kat";
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.command.empty() || options.command == "help") {
            std::cout << kUsage;
            return 0;
        }
        checkEncode(options);
        if (options.command == "encrypt") return runEncrypt(options);
        if (options.command == "decrypt") return runDecrypt(options);
        if (options.command == "bench") return runBench(options);
        if (options.command == "kat") {
            if (options.katPath.empty()) fail("kat requires a JSON vector file path");
            return runKat(options.katPath);
        }
        fail("unknown command: " + options.command);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
