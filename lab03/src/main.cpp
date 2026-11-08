#include "hybrid.hpp"
#include "minijson.hpp"
#include "rsa.hpp"
#include "util.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <openssl/rand.h>

namespace {

struct Options {
    std::string command;
    std::string inPath;
    std::string text;
    std::string outPath;
    std::string pubPath;
    std::string privPath;
    std::string pubDerPath;
    std::string privDerPath;
    std::string metadataPath;
    std::string label;
    std::string encode;
    std::string decode;
    std::string katPath;
    int bits = 3072;
    int runsRsa = 1000;
    int runsSmall = 1000;
    int runsLarge = 100;
    int threads = 1;
    double warmup = 1.0;
    bool verbose = false;
    bool hasIn = false;
    bool hasText = false;
    bool hasOut = false;
    bool hasEncode = false;
    bool hasDecode = false;
};

const char* kUsage = R"(rsatool - RSA-OAEP and hybrid AES-GCM lab tool

Usage:
  rsatool keygen  --bits 3072 --pub pub.pem --priv priv.pem
                  [--pub-der FILE] [--priv-der FILE] [--metadata FILE]
  rsatool encrypt --in FILE | --text STRING --pub KEY --out FILE
                  [--label STRING] [--encode hex|base64|raw] [--verbose]
  rsatool decrypt --in FILE | --text STRING --priv KEY --out FILE
                  [--label STRING] [--decode hex|base64|raw] [--verbose]
  rsatool bench   [--runs-rsa N] [--runs-small N] [--runs-large N]
                  [--threads N] [--warmup SECONDS] [--out CSV] [--verbose]
                  [--runs N  (sets all three run counts)]
  rsatool --kat FILE
  rsatool help

Keys:       PEM or DER; RSA >= 3072 bits (4096 supported for comparison).
Direct:     RSA-OAEP with SHA-256 / MGF1-SHA-256 and an optional label.
Hybrid:     Automatic above k - 2*32 - 2 bytes. AES-256-GCM with a 96-bit IV;
            the AES key is wrapped with RSA-OAEP. Envelope = RSAENV1 + JSON
            header + raw ciphertext.
Encoding:   File output is raw by default; stdout is hex. --encode overrides.
            Decrypt encoded input with --decode.
)";

int parseInt(const std::string& value, const std::string& name) {
    std::size_t used = 0;
    int result = 0;
    try {
        result = std::stoi(value, &used);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + name);
    }
    if (used != value.size()) throw std::runtime_error("invalid value for " + name);
    return result;
}

double parseDouble(const std::string& value, const std::string& name) {
    std::size_t used = 0;
    double result = 0;
    try {
        result = std::stod(value, &used);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + name);
    }
    if (used != value.size() || result < 0) throw std::runtime_error("invalid value for " + name);
    return result;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    if (argc < 2) throw std::runtime_error("missing command (try: rsatool help)");
    options.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (options.command == "--kat" && options.katPath.empty() && arg.rfind("--", 0) != 0) {
            options.katPath = arg;
            continue;
        }
        auto value = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + flag);
            return argv[++i];
        };
        if (arg == "--in") { options.inPath = value("--in"); options.hasIn = true; }
        else if (arg == "--text") { options.text = value("--text"); options.hasText = true; }
        else if (arg == "--out") { options.outPath = value("--out"); options.hasOut = true; }
        else if (arg == "--pub") options.pubPath = value("--pub");
        else if (arg == "--priv") options.privPath = value("--priv");
        else if (arg == "--pub-der") options.pubDerPath = value("--pub-der");
        else if (arg == "--priv-der") options.privDerPath = value("--priv-der");
        else if (arg == "--metadata") options.metadataPath = value("--metadata");
        else if (arg == "--label") options.label = value("--label");
        else if (arg == "--encode") { options.encode = value("--encode"); options.hasEncode = true; }
        else if (arg == "--decode") { options.decode = value("--decode"); options.hasDecode = true; }
        else if (arg == "--bits") options.bits = parseInt(value("--bits"), "--bits");
        else if (arg == "--runs") {
            const int runs = parseInt(value("--runs"), "--runs");
            options.runsRsa = options.runsSmall = options.runsLarge = runs;
        }
        else if (arg == "--runs-rsa") options.runsRsa = parseInt(value("--runs-rsa"), "--runs-rsa");
        else if (arg == "--runs-small") options.runsSmall = parseInt(value("--runs-small"), "--runs-small");
        else if (arg == "--runs-large") options.runsLarge = parseInt(value("--runs-large"), "--runs-large");
        else if (arg == "--threads") options.threads = parseInt(value("--threads"), "--threads");
        else if (arg == "--warmup") options.warmup = parseDouble(value("--warmup"), "--warmup");
        else if (arg == "--kat") options.katPath = value("--kat");
        else if (arg == "--verbose") options.verbose = true;
        else if (arg == "--help" || arg == "-h") options.command = "help";
        else throw std::runtime_error("unknown option: " + arg);
    }
    return options;
}

std::string encodeBytes(const std::string& data, const Options& options, bool screen) {
    if (options.hasEncode) {
        if (options.encode == "hex") return util::toHex(data);
        if (options.encode == "base64") return util::toBase64(data);
        if (options.encode == "raw") return data;
        throw std::runtime_error("--encode must be hex, base64 or raw");
    }
    return screen ? util::toHex(data) : data;
}

std::string readInput(const Options& options) {
    if (options.hasIn && options.hasText) throw std::runtime_error("use either --in or --text");
    if (options.hasIn) return util::readFile(options.inPath);
    if (options.hasText) return options.text;
    throw std::runtime_error("an input is required: pass --in FILE or --text STRING");
}

std::string decodeInput(const std::string& input, const Options& options) {
    if (!options.hasDecode) return input;
    if (options.decode == "hex") return util::fromHex(input);
    if (options.decode == "base64") return util::fromBase64(input);
    if (options.decode == "raw") return input;
    throw std::runtime_error("--decode must be hex, base64 or raw");
}

void writeResult(const std::string& data, const Options& options) {
    const bool screen = !options.hasOut;
    const std::string encoded = encodeBytes(data, options, screen);
    if (screen) {
        std::cout.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        if (!options.hasEncode || options.encode != "raw") std::cout << '\n';
    } else {
        util::writeFile(options.outPath, encoded);
    }
}

std::string replaceExtension(const std::string& path, const std::string& extension) {
    std::filesystem::path p = std::filesystem::u8path(path);
    p.replace_extension(extension);
    return p.u8string();
}

std::string utcNow() {
    const std::time_t now = std::time(nullptr);
    const std::tm* tm = std::gmtime(&now);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", tm);
    return buffer;
}

void commandKeygen(const Options& options) {
    if (options.pubPath.empty() || options.privPath.empty())
        throw std::runtime_error("keygen requires --pub FILE and --priv FILE");
    if (options.bits < 3072) throw std::runtime_error("--bits must be at least 3072");

    const rsa::Key key = rsa::generateKey(options.bits);
    const std::string pubDer = options.pubDerPath.empty()
        ? replaceExtension(options.pubPath, ".der") : options.pubDerPath;
    const std::string privDer = options.privDerPath.empty()
        ? replaceExtension(options.privPath, ".der") : options.privDerPath;
    const std::string metadata = options.metadataPath.empty()
        ? replaceExtension(options.privPath, ".json") : options.metadataPath;

    util::writeFile(options.pubPath, rsa::publicPem(key));
    util::writeFile(options.privPath, rsa::privatePem(key));
    util::writeFile(pubDer, rsa::publicDer(key));
    util::writeFile(privDer, rsa::privateDer(key));

    std::ostringstream json;
    json << "{\n"
         << "  \"creation_time\": \"" << utcNow() << "\",\n"
         << "  \"modulus_bits\": " << key.bits() << ",\n"
         << "  \"hash\": \"SHA-256\",\n"
         << "  \"public_key_pem\": \"" << util::jsonEscape(options.pubPath) << "\",\n"
         << "  \"private_key_pem\": \"" << util::jsonEscape(options.privPath) << "\",\n"
         << "  \"public_key_der\": \"" << util::jsonEscape(pubDer) << "\",\n"
         << "  \"private_key_der\": \"" << util::jsonEscape(privDer) << "\"\n"
         << "}\n";
    util::writeFile(metadata, json.str());
    if (options.verbose) {
        std::cerr << "wrote " << options.pubPath << ", " << options.privPath << ", "
                  << pubDer << ", " << privDer << ", " << metadata << "\n";
    }
}

void commandEncrypt(const Options& options) {
    if (options.pubPath.empty()) throw std::runtime_error("encrypt requires --pub KEY");
    const rsa::Key key = rsa::loadPublicKey(util::readFile(options.pubPath));
    const std::string input = readInput(options);
    const bool direct = input.size() <= rsa::maxPlaintext(key);
    const std::string output = direct
        ? rsa::oaepEncrypt(key, input, options.label)
        : hybrid::encrypt(key, input, options.label);
    writeResult(output, options);
    if (options.verbose) {
        std::cerr << "mode=" << (direct ? "RSA-OAEP" : "RSA-OAEP-AES-GCM")
                  << " plaintext=" << input.size() << " bytes\n";
    }
}

void commandDecrypt(const Options& options) {
    if (options.privPath.empty()) throw std::runtime_error("decrypt requires --priv KEY");
    const rsa::Key key = rsa::loadPrivateKey(util::readFile(options.privPath));
    const std::string input = decodeInput(readInput(options), options);
    const std::string output = hybrid::isEnvelope(input)
        ? hybrid::decrypt(key, input, options.label)
        : rsa::oaepDecrypt(key, input, options.label);
    writeResult(output, options);
}

int runKat(const std::string& path) {
    const minijson::Value root = minijson::parse(util::readFile(path));
    const minijson::Value& vectors = root.at("vectors");
    if (vectors.type != minijson::Value::Type::Array)
        throw std::runtime_error("KAT file has no vectors array");
    int passed = 0;
    int failed = 0;
    for (const minijson::Value& vector : vectors.array) {
        const std::string id = vector.getString("id");
        try {
            const rsa::Key key = rsa::loadPrivateKey(vector.getString("private_key_pem"));
            const std::string label = vector.has("label") ? vector.getString("label") : "";
            const std::string ciphertext = util::fromHex(vector.getString("ciphertext_hex"));
            const std::string expected = util::fromHex(vector.getString("plaintext_hex"));
            const std::string actual = rsa::oaepDecrypt(key, ciphertext, label);
            if (actual == expected) {
                std::cout << "PASS " << id << "\n";
                ++passed;
            } else {
                std::cout << "FAIL " << id << "\n";
                ++failed;
            }
        } catch (const std::exception&) {
            std::cout << "FAIL " << id << "\n";
            ++failed;
        }
    }
    std::cout << "KAT summary: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

std::string randomBytes(std::size_t count) {
    std::string out(count, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&out[0]), static_cast<int>(count)) != 1)
        throw std::runtime_error("secure random generator failed");
    return out;
}

void writeCsv(const Options& options, const std::vector<std::string>& rows) {
    std::ostringstream csv;
    csv << "case,bits,size_bytes,threads,run,elapsed_ms,mbps\n";
    for (const std::string& row : rows) csv << row << '\n';
    if (options.hasOut) util::writeFile(options.outPath, csv.str());
    else std::cout << csv.str();
}

void commandBench(const Options& options) {
    if (options.runsRsa < 1 || options.runsSmall < 1 || options.runsLarge < 1)
        throw std::runtime_error("run counts must be positive");
    if (options.threads < 1 || options.threads > 64)
        throw std::runtime_error("--threads must be between 1 and 64");
    std::vector<std::string> rows;

    auto measure = [&](const std::string& name, int bits, std::size_t size, int runs,
                       auto operation) {
        const int threads = std::max(1, std::min(options.threads, runs));
        std::vector<std::vector<std::string>> threadRows(static_cast<std::size_t>(threads));
        const int base = runs / threads;
        const int extra = runs % threads;

        auto worker = [&](int index, int count) {
            const auto warmupEnd = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(static_cast<long long>(options.warmup * 1000));
            while (std::chrono::steady_clock::now() < warmupEnd) operation(index);
            auto& output = threadRows[static_cast<std::size_t>(index)];
            for (int i = 0; i < count; ++i) {
                const auto start = std::chrono::steady_clock::now();
                operation(index);
                const auto end = std::chrono::steady_clock::now();
                const double ms = std::chrono::duration<double, std::milli>(end - start).count();
                const double mbps = size == 0 ? 0.0
                    : (static_cast<double>(size) / (ms / 1000.0)) / 1000000.0;
                std::ostringstream row;
                row << name << ',' << bits << ',' << size << ',' << threads << ','
                    << (index * base + std::min(index, extra) + i) << ','
                    << std::fixed << std::setprecision(3) << ms << ','
                    << std::setprecision(3) << mbps;
                output.push_back(row.str());
            }
        };

        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back(worker, t, base + (t < extra ? 1 : 0));
        for (std::thread& thread : pool) thread.join();
        for (const std::vector<std::string>& output : threadRows)
            rows.insert(rows.end(), output.begin(), output.end());
        if (options.verbose) std::cerr << "bench " << name << " bits=" << bits
                                      << " size=" << size << " threads=" << threads
                                      << " runs=" << runs << " done\n";
    };

    for (int bits : {3072, 4096}) {
        measure("rsa_keygen", bits, 0, options.runsRsa, [&](int) { (void)rsa::generateKey(bits); });
        const rsa::Key key = rsa::generateKey(bits);
        const std::string message(32, 'x');
        measure("rsa_encrypt", bits, message.size(), options.runsSmall,
                [&](int) { (void)rsa::oaepEncrypt(key, message, ""); });
        const std::string ciphertext = rsa::oaepEncrypt(key, message, "");
        measure("rsa_decrypt", bits, message.size(), options.runsSmall,
                [&](int) { (void)rsa::oaepDecrypt(key, ciphertext, ""); });
    }

    const std::string aesKey = randomBytes(32);
    const std::string iv = randomBytes(12);
    std::vector<std::string> tags(static_cast<std::size_t>(options.threads));
    for (std::size_t size : {static_cast<std::size_t>(1024), static_cast<std::size_t>(1048576),
                             static_cast<std::size_t>(104857600)}) {
        const std::string data(size, 'x');
        const int runs = size >= static_cast<std::size_t>(104857600)
            ? options.runsLarge : options.runsSmall;
        measure("aes_gcm", 256, size, runs, [&](int worker) {
            tags[static_cast<std::size_t>(worker)].clear();
            (void)hybrid::aesGcmEncrypt(aesKey, iv, "", data, tags[static_cast<std::size_t>(worker)]);
        });
        const std::string ciphertext = hybrid::aesGcmEncrypt(aesKey, iv, "", data,
                                                             tags[0]);
        measure("aes_gcm_decrypt", 256, size, runs,
                [&](int) { (void)hybrid::aesGcmDecrypt(aesKey, iv, "", ciphertext, tags[0]); });
    }

    const rsa::Key key = rsa::generateKey(3072);
    for (std::size_t size : {static_cast<std::size_t>(1024), static_cast<std::size_t>(1048576),
                             static_cast<std::size_t>(104857600)}) {
        const std::string data(size, 'x');
        const int runs = size >= static_cast<std::size_t>(104857600)
            ? options.runsLarge : options.runsSmall;
        measure("hybrid_encrypt", 3072, size, runs,
                [&](int) { (void)hybrid::encrypt(key, data, ""); });
        const std::string envelope = hybrid::encrypt(key, data, "");
        measure("hybrid_decrypt", 3072, size, runs,
                [&](int) { (void)hybrid::decrypt(key, envelope, ""); });
    }
    writeCsv(options, rows);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.command == "help" || options.command == "--help" || options.command == "-h") {
            std::cout << kUsage;
            return 0;
        }
        if (options.command == "--kat" || options.command == "kat") {
            if (options.katPath.empty()) throw std::runtime_error("--kat requires a JSON file");
            return runKat(options.katPath);
        }
        if (!options.katPath.empty()) return runKat(options.katPath);
        if (options.command == "keygen") commandKeygen(options);
        else if (options.command == "encrypt") commandEncrypt(options);
        else if (options.command == "decrypt") commandDecrypt(options);
        else if (options.command == "bench") commandBench(options);
        else throw std::runtime_error("unknown command '" + options.command + "' (try: rsatool help)");
        return 0;
    } catch (const std::exception& error) {
        // Decryption failures always look the same: do not leak which check failed.
        if (argc >= 2 && std::string(argv[1]) == "decrypt") {
            std::cerr << "error: decryption failed\n";
        } else {
            std::cerr << "error: " << error.what() << "\n";
        }
        return 1;
    }
}
