#include "hash_core.hpp"
#include "sha256_ext.hpp"
#include "util.hpp"
#include "x509_inspect.hpp"

#include <openssl/opensslv.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string command;
    std::string inPath;
    std::string text;
    std::string outPath;
    std::string algo;
    std::string encode;
    std::string issuerPath;
    std::string mac;
    std::string appendPath;
    std::string appendText;
    std::string katPath;
    std::string workDir;
    std::vector<std::string> algos;
    std::vector<std::string> ios;
    std::vector<int> sizes;
    int outlen = 0;
    bool hasOutlen = false;
    bool hasIn = false;
    bool hasText = false;
    bool hasAppend = false;
    bool hasAppendText = false;
    bool requireVerified = false;
    bool verbose = false;
    bool stream = false;
    bool hasRuns = false;
    int runs = 0;
    double warmup = 1.0;
    std::size_t keyLen = 0;
    bool hasKeyLen = false;
};

const char* kUsage = R"(hashtool - Lab 4 hashing, X.509 and length-extension tool

Usage:
  hashtool --algo sha256 --in FILE [--out FILE] [--encode hex|base64|raw]
  hashtool --algo shake128 --outlen 128 --in FILE
  hashtool cert --in CERT [--issuer CERT_OR_PUBKEY] [--require-verified] [--out FILE]
  hashtool lenext --mac HEX --key-len N --in FILE [--append FILE | --append-text STR]
                  [--out FORGED_FILE] [--encode hex|base64|raw]
  hashtool bench [--algos LIST] [--sizes MIB[,MIB...]] [--io stream,mmap]
                 [--runs N] [--warmup SECONDS] [--out CSV] [--work-dir DIR]
  hashtool --kat kat/hash_vectors.json
  hashtool help

Hashes: sha224 sha256 sha384 sha512 sha3-224 sha3-256 sha3-384 sha3-512
        shake128 shake256 (SHAKE requires --outlen in bits, multiple of 8).
Input:  --in is binary-safe; --text is UTF-8. Exactly one is required for hash.
Output: stdout is hex; --out is raw unless --encode overrides.
)";

std::vector<std::string> splitList(const std::string& value) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream input(value);
    while (std::getline(input, item, ',')) {
        if (item.empty()) throw std::runtime_error("empty list item");
        out.push_back(item);
    }
    if (out.empty()) throw std::runtime_error("empty list");
    return out;
}

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

std::size_t parseSize(const std::string& value, const std::string& name) {
    std::size_t used = 0;
    unsigned long long result = 0;
    try {
        result = std::stoull(value, &used);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + name);
    }
    if (used != value.size()) throw std::runtime_error("invalid value for " + name);
    return static_cast<std::size_t>(result);
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
    if (argc < 2) throw std::runtime_error("missing command (try: hashtool help)");
    const std::string first = argv[1];
    int start = 1;
    if (first == "help" || first == "--help" || first == "-h") {
        options.command = "help";
        return options;
    }
    if (first == "--kat") {
        options.command = "kat";
        if (argc < 3) throw std::runtime_error("missing KAT file");
        options.katPath = argv[2];
        start = 3;
    } else if (first == "hash" || first == "cert" || first == "lenext" || first == "bench") {
        options.command = first;
        start = 2;
    } else if (first.rfind("--", 0) == 0) {
        options.command = "hash";
    } else {
        throw std::runtime_error("unknown command: " + first);
    }

    for (int i = start; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + flag);
            return argv[++i];
        };
        if (arg == "--in") { options.inPath = value("--in"); options.hasIn = true; }
        else if (arg == "--text") { options.text = value("--text"); options.hasText = true; }
        else if (arg == "--out") options.outPath = value("--out");
        else if (arg == "--algo") options.algo = value("--algo");
        else if (arg == "--outlen") { options.outlen = parseInt(value("--outlen"), "--outlen"); options.hasOutlen = true; }
        else if (arg == "--encode") options.encode = value("--encode");
        else if (arg == "--issuer") options.issuerPath = value("--issuer");
        else if (arg == "--require-verified") options.requireVerified = true;
        else if (arg == "--verbose") options.verbose = true;
        else if (arg == "--stream") options.stream = true;
        else if (arg == "--mac") options.mac = value("--mac");
        else if (arg == "--key-len") { options.keyLen = parseSize(value("--key-len"), "--key-len"); options.hasKeyLen = true; }
        else if (arg == "--append") { options.appendPath = value("--append"); options.hasAppend = true; }
        else if (arg == "--append-text") { options.appendText = value("--append-text"); options.hasAppendText = true; }
        else if (arg == "--algos") options.algos = splitList(value("--algos"));
        else if (arg == "--sizes") {
            for (const std::string& item : splitList(value("--sizes")))
                options.sizes.push_back(parseInt(item, "--sizes"));
        }
        else if (arg == "--io") options.ios = splitList(value("--io"));
        else if (arg == "--runs") { options.runs = parseInt(value("--runs"), "--runs"); options.hasRuns = true; }
        else if (arg == "--warmup") options.warmup = parseDouble(value("--warmup"), "--warmup");
        else if (arg == "--work-dir") options.workDir = value("--work-dir");
        else if (arg == "--kat") options.katPath = value("--kat");
        else if (arg == "--help" || arg == "-h") options.command = "help";
        else throw std::runtime_error("unknown option: " + arg);
    }
    if (!options.katPath.empty()) options.command = "kat";
    return options;
}

std::string encoded(const std::string& data, const std::string& encoding) {
    if (encoding == "hex") return util::toHex(data);
    if (encoding == "base64") return util::toBase64(data);
    if (encoding == "raw") return data;
    throw std::runtime_error("invalid encoding: " + encoding);
}

void writeResult(const std::string& data, const std::string& path, const std::string& encoding,
                 bool consoleHex) {
    const std::string selected = encoding.empty() ? (path.empty() ? (consoleHex ? "hex" : "raw") : "raw")
                                                  : encoding;
    const std::string bytes = encoded(data, selected);
    if (path.empty()) {
        std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        std::cout.flush();
    } else {
        util::writeFile(path, bytes);
    }
}

int runHash(const Options& options) {
    if (options.algo.empty()) throw std::runtime_error("--algo is required");
    if (options.hasIn == options.hasText) throw std::runtime_error("use exactly one of --in or --text");
    const int outbits = hashcore::outputBits(options.algo, options.hasOutlen, options.outlen);
    std::string digest;
    if (options.hasText) {
        digest = hashcore::hashBytes(options.algo, options.text, outbits);
    } else {
        digest = hashcore::hashFile(options.algo, options.inPath, outbits, false);
    }
    if (options.verbose)
        std::cerr << "algo=" << options.algo << " bits=" << outbits << " output_bits=" << digest.size() * 8 << '\n';
    writeResult(digest, options.outPath, options.encode, true);
    return 0;
}

int runCert(const Options& options) {
    if (!options.hasIn) throw std::runtime_error("cert requires --in");
    const std::string certificate = util::readFile(options.inPath);
    const std::string issuer = options.issuerPath.empty() ? std::string() : util::readFile(options.issuerPath);
    const x509::Info info = x509::inspect(certificate, issuer, options.requireVerified);
    const std::string report = x509::format(info);
    if (options.outPath.empty()) std::cout << report;
    else util::writeFile(options.outPath, report);
    return 0;
}

int runLengthExtension(const Options& options) {
    if (!options.hasKeyLen) throw std::runtime_error("lenext requires --key-len");
    if (options.mac.empty()) throw std::runtime_error("lenext requires --mac");
    if (options.hasIn == options.hasText) throw std::runtime_error("use exactly one of --in or --text");
    if (options.hasAppend == options.hasAppendText) throw std::runtime_error("use exactly one of --append or --append-text");
    const std::string message = options.hasText ? options.text : util::readFile(options.inPath);
    const std::string append = options.hasAppendText ? options.appendText : util::readFile(options.appendPath);
    const std::string mac = util::fromHex(options.mac);
    std::string forged;
    const std::string forgedMac = sha256ext::extend(mac, options.keyLen, message, append, forged);
    std::cout << "mac: " << util::toHex(mac) << '\n'
              << "forged_mac: " << util::toHex(forgedMac) << '\n'
              << "forged_message_hex: " << util::toHex(forged) << '\n'
              << "forged_message_len: " << forged.size() << '\n';
    if (!options.outPath.empty()) writeResult(forged, options.outPath, options.encode, false);
    return 0;
}

bool sameFileSize(const std::string& path, std::size_t expected) {
    std::error_code error;
    const auto size = std::filesystem::file_size(std::filesystem::u8path(path), error);
    return !error && size == expected;
}

void makeFile(const std::string& path, std::size_t size) {
    if (sameFileSize(path, size)) return;
    std::ofstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    if (!file.good()) throw std::runtime_error("cannot create benchmark file: " + path);
    const std::string chunk(1024 * 1024, '\0');
    for (std::size_t written = 0; written < size; written += chunk.size()) {
        const std::size_t take = std::min(chunk.size(), size - written);
        file.write(chunk.data(), static_cast<std::streamsize>(take));
    }
    if (!file.good()) throw std::runtime_error("failed while writing benchmark file: " + path);
}

int runBench(const Options& options) {
    std::vector<std::string> algos = options.algos;
    if (algos.empty()) algos = {"sha256", "sha512", "sha3-256", "sha3-512"};
    std::vector<int> sizes = options.sizes;
    if (sizes.empty()) sizes = {1, 100, 1024};
    std::vector<std::string> ios = options.ios;
    if (ios.empty()) ios = {"stream", "mmap"};

    for (const std::string& algo : algos) hashcore::find(algo);
    for (int size : sizes)
        if (size <= 0 || size > 4096) throw std::runtime_error("benchmark size must be 1..4096 MiB");
    for (const std::string& io : ios)
        if (io != "stream" && io != "mmap") throw std::runtime_error("invalid --io: " + io);
    if (options.hasRuns && options.runs <= 0) throw std::runtime_error("--runs must be positive");

    const std::filesystem::path work = options.workDir.empty()
                                           ? std::filesystem::temp_directory_path() / "hashtool_bench"
                                           : std::filesystem::u8path(options.workDir);
    std::error_code error;
    std::filesystem::create_directories(work, error);
    if (error) throw std::runtime_error("cannot create benchmark directory: " + work.string());

    std::ostringstream csv;
    csv << "algo,size_bytes,size_mib,io,run,elapsed_ms,mbps,cpu_ms,cpu_pct\n";
    for (int size : sizes) {
        const std::size_t bytes = static_cast<std::size_t>(size) * 1024 * 1024;
        const std::string path = (work / ("bench_" + std::to_string(size) + "MiB.bin")).string();
        if (!sameFileSize(path, bytes)) {
            std::cerr << "creating " << size << " MiB benchmark file\n";
            makeFile(path, bytes);
        }
        for (const std::string& algo : algos) {
            const int outbits = hashcore::outputBits(algo, false, 0);
            for (const std::string& io : ios) {
                const bool mmap = io == "mmap";
                const int runs = options.hasRuns ? options.runs : (size <= 1 ? 1000 : (size <= 100 ? 100 : 30));
                if (options.warmup > 0) {
                    double warmed = 0;
                    while (warmed < options.warmup) {
                        const auto start = std::chrono::steady_clock::now();
                        hashcore::hashFile(algo, path, outbits, mmap);
                        warmed += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                    }
                }
                for (int run = 1; run <= runs; ++run) {
                    const std::clock_t cpuStart = std::clock();
                    const auto start = std::chrono::steady_clock::now();
                    hashcore::hashFile(algo, path, outbits, mmap);
                    const auto stop = std::chrono::steady_clock::now();
                    const double seconds = std::chrono::duration<double>(stop - start).count();
                    const double cpuSeconds = static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
                    const double mbps = seconds > 0 ? (static_cast<double>(bytes) / 1e6) / seconds : 0;
                    const double cpuPct = seconds > 0 ? 100.0 * cpuSeconds / seconds : 0;
                    csv << algo << ',' << bytes << ',' << size << ',' << io << ',' << run << ','
                        << std::fixed << std::setprecision(3) << seconds * 1000 << ',' << mbps << ','
                        << cpuSeconds * 1000 << ',' << cpuPct << '\n';
                }
                std::cerr << algo << ' ' << size << " MiB " << io << ": " << runs << " runs\n";
            }
        }
    }
    if (options.outPath.empty()) std::cout << csv.str();
    else util::writeFile(options.outPath, csv.str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.command == "help") {
            std::cout << kUsage;
            return 0;
        }
        if (options.command == "hash") return runHash(options);
        if (options.command == "cert") return runCert(options);
        if (options.command == "lenext") return runLengthExtension(options);
        if (options.command == "bench") return runBench(options);
        if (options.command == "kat") {
            const int failures = hashcore::runKat(options.katPath);
            return failures == 0 ? 0 : 1;
        }
        throw std::runtime_error("unknown command: " + options.command);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
