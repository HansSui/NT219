#include "AES.hpp"
#include "modes.hpp"
#include "minijson.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

std::filesystem::path nativePath(const std::string& path) {
    // u8path converts a UTF-8 argument to the native wide path on Windows.
    return std::filesystem::u8path(path);
}

void printHelp(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [options]\n\n"
              << "Commands:\n"
              << "  encrypt    Encrypt data using AES-CTR or AES-XTS\n"
              << "  decrypt    Decrypt data using AES-CTR or AES-XTS\n"
              << "  bench      Benchmark encryption/decryption throughput & latency\n"
              << "  --kat FILE Run Known Answer Tests from JSON vector file\n"
              << "  help       Display this help message\n\n"
              << "Encrypt/Decrypt Options:\n"
              << "  --mode <ctr|xts>                      Cipher mode (default: ctr)\n"
              << "  --key <file>                          Binary key file (CTR: 16/24/32 B; XTS: 32/48/64 B)\n"
              << "  --key-hex <hex>                       Hexadecimal key string\n"
              << "  --iv <file>                           Binary counter / XTS data unit (16 bytes)\n"
              << "  --iv-hex <hex>                        Hexadecimal IV string (16 bytes = 32 hex chars)\n"
              << "  --nonce <file|hex>                    Alias for the CTR counter / XTS data unit\n"
              << "  --nonce-hex <hex>                     Hexadecimal --nonce (alias of --iv-hex)\n"
              << "  --in <file>                           Input data file\n"
              << "  --text <string>                       Plaintext input string directly\n"
              << "  --out <file>                          Output data file\n"
              << "  --out-hex                             Display output as hex to stdout\n"
              << "  --encode hex|base64|raw               Output encoding (default: raw for --out, hex for stdout)\n"
              << "  --decode hex|base64|raw               Decode decrypt input before use\n"
              << "  --verbose                             Print details and progress/ETA to stderr\n\n"
              << "Benchmark Options:\n"
              << "  --size <bytes>                        Payload size in bytes (e.g. 1048576 = 1MB)\n"
              << "  --runs <n>                            Number of iterations (default: 1000)\n"
              << "  --warmup <seconds>                    Time-based warm-up duration (default: 1.0)\n"
              << "  --threads <n>                         Worker threads for the benchmark (default: 1)\n"
              << "  --out <csv_file>                      Output CSV filename\n";
}

std::vector<uint8_t> hexToBytes(const std::string& rawHex) {
    std::string hex;
    for (unsigned char c : rawHex) {
        if (!std::isspace(c) && c != ':') hex.push_back(static_cast<char>(c));
    }
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Hex string must have an even length");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char* end = nullptr;
        unsigned long val = std::strtoul(byteString.c_str(), &end, 16);
        if (end != byteString.c_str() + 2) {
            throw std::runtime_error("Invalid hex byte: " + byteString);
        }
        bytes.push_back(static_cast<uint8_t>(val));
    }
    return bytes;
}

std::string bytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string formatDuration(double seconds) {
    std::ostringstream oss;
    if (seconds < 60.0) {
        oss << std::fixed << std::setprecision(1) << seconds << "s";
    } else {
        const long long total = static_cast<long long>(seconds + 0.5);
        oss << (total / 60) << "m" << std::setw(2) << std::setfill('0')
            << (total % 60) << "s";
    }
    return oss.str();
}

bool looksLikeHex(const std::string& text) {
    size_t digits = 0;
    for (unsigned char c : text) {
        if (std::isxdigit(c)) {
            ++digits;
        } else if (!std::isspace(c) && c != ':') {
            return false;
        }
    }
    return digits > 0 && digits % 2 == 0;
}

std::string base64Encode(const std::vector<uint8_t>& data) {
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= data.size()) {
        const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                           (static_cast<uint32_t>(data[i + 1]) << 8) |
                           static_cast<uint32_t>(data[i + 2]);
        out.push_back(alphabet[(n >> 18) & 0x3F]);
        out.push_back(alphabet[(n >> 12) & 0x3F]);
        out.push_back(alphabet[(n >> 6) & 0x3F]);
        out.push_back(alphabet[n & 0x3F]);
        i += 3;
    }
    if (i < data.size()) {
        const uint32_t b1 = data[i];
        const uint32_t b2 = (i + 1 < data.size()) ? data[i + 1] : 0;
        const uint32_t n = (b1 << 16) | (b2 << 8);
        out.push_back(alphabet[(n >> 18) & 0x3F]);
        out.push_back(alphabet[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < data.size() ? alphabet[(n >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::vector<uint8_t> base64Decode(const std::string& text) {
    std::string compact;
    for (unsigned char c : text) {
        if (!std::isspace(c)) compact.push_back(static_cast<char>(c));
    }
    if (compact.size() % 4 != 0) {
        throw std::runtime_error("Invalid base64 length");
    }
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    std::vector<uint8_t> out;
    out.reserve((compact.size() / 4) * 3);
    for (size_t i = 0; i < compact.size(); i += 4) {
        const char c0 = compact[i];
        const char c1 = compact[i + 1];
        const char c2 = compact[i + 2];
        const char c3 = compact[i + 3];
        const int v0 = value(c0);
        const int v1 = value(c1);
        if (v0 < 0 || v1 < 0) throw std::runtime_error("Invalid base64 character");
        const bool pad2 = c2 == '=';
        const bool pad3 = c3 == '=';
        if (pad2 && !pad3) throw std::runtime_error("Invalid base64 padding");
        if ((pad2 || pad3) && i + 4 != compact.size()) {
            throw std::runtime_error("Base64 padding is only allowed at the end");
        }
        const int v2 = pad2 ? 0 : value(c2);
        const int v3 = pad3 ? 0 : value(c3);
        if (v2 < 0 || v3 < 0) throw std::runtime_error("Invalid base64 character");
        out.push_back(static_cast<uint8_t>((v0 << 2) | (v1 >> 4)));
        if (!pad2) out.push_back(static_cast<uint8_t>(((v1 & 0x0F) << 4) | (v2 >> 2)));
        if (!pad3) out.push_back(static_cast<uint8_t>(((v2 & 0x03) << 6) | v3));
    }
    return out;
}

std::vector<uint8_t> encodeBytes(const std::vector<uint8_t>& data, const std::string& encoding) {
    if (encoding == "raw") return data;
    if (encoding == "hex") {
        const std::string text = bytesToHex(data.data(), data.size());
        return std::vector<uint8_t>(text.begin(), text.end());
    }
    if (encoding == "base64") {
        const std::string text = base64Encode(data);
        return std::vector<uint8_t>(text.begin(), text.end());
    }
    throw std::runtime_error("Unsupported --encode value: " + encoding + " (use hex, base64 or raw)");
}

std::vector<uint8_t> decodeBytes(const std::vector<uint8_t>& data, const std::string& encoding) {
    if (encoding == "raw") return data;
    const std::string text(data.begin(), data.end());
    if (encoding == "hex") return hexToBytes(text);
    if (encoding == "base64") return base64Decode(text);
    throw std::runtime_error("Unsupported --decode value: " + encoding + " (use hex, base64 or raw)");
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream ifs(nativePath(path), std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Cannot open input file: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream ofs(nativePath(path), std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Cannot open output file: " + path);
    }
    ofs.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    if (!ofs) {
        throw std::runtime_error("Failed writing output file: " + path);
    }
}

AES::KeyLength getKeyLength(size_t len) {
    if (len == 16) return AES::AES_128;
    if (len == 24) return AES::AES_192;
    if (len == 32) return AES::AES_256;
    throw std::runtime_error("Invalid key length: " + std::to_string(len) +
                             " bytes (must be 16 for AES-128, 24 for AES-192, 32 for AES-256)");
}

Block makeBlock(const uint8_t* data) {
    Block b;
    std::memcpy(b.data(), data, 16);
    return b;
}

Block addBlocks(Block counter, size_t blocks) {
    for (size_t n = 0; n < blocks; ++n) {
        for (int i = 15; i >= 0; --i) {
            if (++counter[i] != 0) break;
        }
    }
    return counter;
}

std::vector<uint8_t> ctrCryptThreaded(const AES& cipher,
                                      const std::vector<uint8_t>& data,
                                      Block counter,
                                      int threads) {
    if (threads <= 1 || data.size() < 65536) {
        Modes modes;
        return modes.ctr_encrypt(cipher, data, counter);
    }

    const size_t blocks = (data.size() + 15) / 16;
    const size_t per_thread = (blocks + static_cast<size_t>(threads) - 1) /
                              static_cast<size_t>(threads);
    std::vector<std::vector<uint8_t>> parts(static_cast<size_t>(threads));
    std::vector<std::thread> workers;

    for (int t = 0; t < threads; ++t) {
        const size_t start_block = static_cast<size_t>(t) * per_thread;
        if (start_block >= blocks) break;
        const size_t end_block = std::min(blocks, start_block + per_thread);
        const size_t begin = start_block * 16;
        const size_t finish = std::min(data.size(), end_block * 16);
        std::vector<uint8_t> chunk(data.begin() + static_cast<std::ptrdiff_t>(begin),
                                   data.begin() + static_cast<std::ptrdiff_t>(finish));
        const Block chunk_counter = addBlocks(counter, start_block);
        workers.emplace_back([&cipher, &parts, t, chunk = std::move(chunk), chunk_counter]() mutable {
            Modes modes;
            parts[static_cast<size_t>(t)] = modes.ctr_encrypt(cipher, chunk, chunk_counter);
        });
    }
    for (auto& worker : workers) worker.join();

    std::vector<uint8_t> out;
    out.reserve(data.size());
    for (const auto& part : parts) out.insert(out.end(), part.begin(), part.end());
    return out;
}

int runKat(const std::string& katFile) {
    std::string content;
    {
        std::ifstream ifs(nativePath(katFile));
        if (!ifs) {
            std::cerr << "FAIL: Cannot open KAT file: " << katFile << "\n";
            return 1;
        }
        content.assign((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    }

    minijson::Value root;
    try {
        minijson::detail::Parser parser(content);
        root = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: JSON parse error: " << e.what() << "\n";
        return 1;
    }

    if (!root.has("vectors")) {
        std::cerr << "FAIL: Missing 'vectors' array in KAT JSON\n";
        return 1;
    }

    const auto& vectors = root.at("vectors").array;
    size_t passed = 0;
    size_t failed = 0;
    size_t skipped = 0;

    Modes modes;

    for (const auto& item : vectors) {
        std::string name = item.getString("name");
        std::string mode = item.getString("mode");
        std::vector<uint8_t> key = hexToBytes(item.getString("key"));
        std::string ivStr = item.getString("iv");
        std::vector<uint8_t> pt = hexToBytes(item.getString("plaintext"));
        std::vector<uint8_t> expectedCt = hexToBytes(item.getString("ciphertext"));

        bool ok = false;
        try {
            if (mode == "ctr") {
                AES cipher(key, getKeyLength(key.size()));
                if (ivStr.empty()) throw std::runtime_error("CTR requires IV");
                std::vector<uint8_t> iv = hexToBytes(ivStr);
                if (iv.size() != 16) throw std::runtime_error("CTR IV must be 16 bytes");
                Block counter = makeBlock(iv.data());
                std::vector<uint8_t> ct = modes.ctr_encrypt(cipher, pt, counter);
                std::vector<uint8_t> roundtrip = modes.ctr_decrypt(cipher, ct, counter);
                ok = (ct == expectedCt && roundtrip == pt);
            } else if (mode == "ecb") {
                // FIPS-197 Appendix C single-block vector
                if (pt.size() != 16 || expectedCt.size() != 16) {
                    throw std::runtime_error("ECB KAT vector must be exactly one block");
                }
                AES cipher(key, getKeyLength(key.size()));
                Block ctBlock = cipher.encrypt(makeBlock(pt.data()));
                Block dtBlock = cipher.decrypt(ctBlock);
                ok = std::memcmp(ctBlock.data(), expectedCt.data(), 16) == 0 &&
                     std::memcmp(dtBlock.data(), pt.data(), 16) == 0;
            } else if (mode == "xts") {
                if (key.size() % 2 != 0) {
                    throw std::runtime_error("XTS key must be two equal halves");
                }
                const size_t half = key.size() / 2;
                const AES::KeyLength halfLength = getKeyLength(half);
                if (ivStr.empty()) throw std::runtime_error("XTS requires a data-unit IV");
                std::vector<uint8_t> iv = hexToBytes(ivStr);
                if (iv.size() != 16) throw std::runtime_error("XTS data unit must be 16 bytes");
                AES key1(key.data(), half, halfLength);
                AES key2(key.data() + half, half, halfLength);
                Block dataUnit = makeBlock(iv.data());
                std::vector<uint8_t> ct = modes.xts_encrypt(key1, key2, pt, dataUnit);
                std::vector<uint8_t> roundtrip = modes.xts_decrypt(key1, key2, ct, dataUnit);
                ok = (ct == expectedCt && roundtrip == pt);
            } else {
                std::cerr << "SKIP unsupported KAT mode: " << mode << " (" << name << ")\n";
                ++skipped;
                continue;
            }
        } catch (const std::exception& e) {
            std::cerr << "  reason: " << e.what() << "\n";
            ok = false;
        }

        if (ok) {
            std::cout << "PASS " << name << "\n";
            ++passed;
        } else {
            std::cout << "FAIL " << name << "\n";
            ++failed;
        }
    }

    std::cout << passed << " passed, " << failed << " failed";
    if (skipped > 0) std::cout << ", " << skipped << " skipped";
    std::cout << "\n";
    return (failed == 0 && skipped == 0) ? 0 : 1;
}

int runBench(int argc, char* argv[]) {
    size_t sizeBytes = 1048576; // Default: 1 MiB
    size_t runs = 1000;
    std::string outCsv;
    std::string keyHex = "000102030405060708090a0b0c0d0e0f";
    double warmupSeconds = 1.0;
    int threads = 1;
    bool verbose = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--size" && i + 1 < argc) sizeBytes = std::stoull(argv[++i]);
        else if (arg == "--runs" && i + 1 < argc) runs = std::stoull(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) outCsv = argv[++i];
        else if (arg == "--key-hex" && i + 1 < argc) keyHex = argv[++i];
        else if (arg == "--warmup" && i + 1 < argc) warmupSeconds = std::stod(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) threads = std::stoi(argv[++i]);
        else if (arg == "--mode" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode != "ctr") {
                throw std::runtime_error("bench only supports --mode ctr");
            }
        } else if (arg == "--verbose") {
            verbose = true;
        } else {
            throw std::runtime_error("Unknown bench option '" + arg + "'");
        }
    }

    if (sizeBytes == 0 || runs == 0) {
        throw std::runtime_error("--size and --runs must be greater than zero");
    }
    if (warmupSeconds < 0.0) {
        throw std::runtime_error("--warmup must be non-negative");
    }
    if (threads < 1 || threads > 256) {
        throw std::runtime_error("--threads must be between 1 and 256");
    }

    std::vector<uint8_t> key = hexToBytes(keyHex);
    AES cipher(key, getKeyLength(key.size()));

    std::vector<uint8_t> payload(sizeBytes, 0x5A);
    Block counter;
    counter.fill(0);

    // Time-based warm-up on the cache-resident prefix. Running a whole 1 GiB
    // payload would turn a 1-2 s warm-up into a minute of extra work.
    const std::vector<uint8_t> warmPayload(
        payload.begin(),
        payload.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(sizeBytes, 1048576)));
    const auto warmStart = std::chrono::steady_clock::now();
    size_t warmIterations = 0;
    do {
        counter.fill(0);
        (void)ctrCryptThreaded(cipher, warmPayload, counter, threads);
        ++warmIterations;
    } while (warmupSeconds > 0.0 &&
             std::chrono::duration<double>(std::chrono::steady_clock::now() - warmStart).count() <
                 warmupSeconds);

    std::ostream* outStream = &std::cout;
    std::ofstream ofs;
    if (!outCsv.empty()) {
        ofs.open(nativePath(outCsv));
        if (!ofs) {
            std::cerr << "FAIL: Cannot open output CSV: " << outCsv << "\n";
            return 1;
        }
        outStream = &ofs;
    }

    if (verbose) {
        std::cerr << "bench: size=" << sizeBytes << " runs=" << runs
                  << " warmup=" << warmupSeconds << "s (" << warmIterations
                  << " iterations) threads=" << threads
                  << " key_bits=" << (key.size() * 8) << "\n";
    }

    *outStream << "run,mode,size_bytes,encrypt_us,decrypt_us,"
                  "throughput_encrypt_MBps,throughput_decrypt_MBps\n";

    const auto benchStart = std::chrono::steady_clock::now();
    auto lastProgress = benchStart;

    for (size_t r = 0; r < runs; ++r) {
        counter.fill(0);
        counter[15] = static_cast<uint8_t>(r & 0xFF);
        counter[14] = static_cast<uint8_t>((r >> 8) & 0xFF);

        auto t0 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> ct = ctrCryptThreaded(cipher, payload, counter, threads);
        auto t1 = std::chrono::high_resolution_clock::now();

        auto t2 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> pt = ctrCryptThreaded(cipher, ct, counter, threads);
        auto t3 = std::chrono::high_resolution_clock::now();

        double encUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        double decUs = std::chrono::duration<double, std::micro>(t3 - t2).count();

        double sizeMB = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);
        double encMBps = (encUs > 0) ? (sizeMB / (encUs / 1e6)) : 0.0;
        double decMBps = (decUs > 0) ? (sizeMB / (decUs / 1e6)) : 0.0;

        *outStream << r << ",ctr," << sizeBytes << ","
                   << std::fixed << std::setprecision(2) << encUs << ","
                   << decUs << "," << encMBps << "," << decMBps << "\n";

        if (verbose) {
            const auto now = std::chrono::steady_clock::now();
            const double sinceLast =
                std::chrono::duration<double>(now - lastProgress).count();
            // Fast runs would otherwise print thousands of lines; slow runs
            // (especially 1 GiB) still get a line roughly every iteration.
            if (r + 1 == runs || runs <= 20 || sinceLast >= 1.0) {
                const double elapsed =
                    std::chrono::duration<double>(now - benchStart).count();
                const double done = static_cast<double>(r + 1) / static_cast<double>(runs);
                const double eta = (done > 0.0) ? elapsed * (1.0 - done) / done : 0.0;
                std::cerr << "progress: " << (r + 1) << "/" << runs
                          << " (" << std::fixed << std::setprecision(1)
                          << (done * 100.0) << "%)"
                          << " elapsed=" << formatDuration(elapsed)
                          << " eta=" << formatDuration(eta)
                          << " enc=" << std::setprecision(2) << encUs
                          << "us dec=" << decUs << "us\n";
                lastProgress = now;
            }
        }
    }

    if (verbose) {
        const double total = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - benchStart).count();
        std::cerr << "bench: completed " << runs << " runs in "
                  << formatDuration(total) << "\n";
    }

    return 0;
}

int realMain(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printHelp(argv[0]);
            return 1;
        }

        std::string cmd = argv[1];
        if (cmd == "help" || cmd == "--help" || cmd == "-h") {
            printHelp(argv[0]);
            return 0;
        }

        if (cmd == "--kat") {
            if (argc < 3) {
                std::cerr << "FAIL: Missing vector file argument for --kat\n";
                return 1;
            }
            return runKat(argv[2]);
        }

        if (cmd == "bench") {
            return runBench(argc, argv);
        }

        if (cmd != "encrypt" && cmd != "decrypt") {
            std::cerr << "FAIL: Unknown command '" << cmd << "'\n";
            printHelp(argv[0]);
            return 1;
        }

        const bool isEncrypt = (cmd == "encrypt");
        std::string mode = "ctr";
        std::string keyFile, keyHex;
        std::string ivFile, ivHex;
        std::string nonce, nonceHex;
        std::string inFile, textIn;
        std::string outFile;
        std::string encode;
        std::string decode;
        bool outHex = false;
        bool verbose = false;
        bool hasThreads = false;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
            else if (arg == "--key" && i + 1 < argc) keyFile = argv[++i];
            else if (arg == "--key-hex" && i + 1 < argc) keyHex = argv[++i];
            else if (arg == "--iv" && i + 1 < argc) ivFile = argv[++i];
            else if (arg == "--iv-hex" && i + 1 < argc) ivHex = argv[++i];
            else if (arg == "--nonce" && i + 1 < argc) nonce = argv[++i];
            else if (arg == "--nonce-hex" && i + 1 < argc) nonceHex = argv[++i];
            else if (arg == "--in" && i + 1 < argc) inFile = argv[++i];
            else if (arg == "--text" && i + 1 < argc) textIn = argv[++i];
            else if (arg == "--out" && i + 1 < argc) outFile = argv[++i];
            else if (arg == "--out-hex") outHex = true;
            else if (arg == "--encode" && i + 1 < argc) encode = argv[++i];
            else if (arg == "--decode" && i + 1 < argc) decode = argv[++i];
            else if (arg == "--verbose") verbose = true;
            else if (arg == "--threads") {
                hasThreads = true;
                if (i + 1 < argc) ++i;
            } else {
                std::cerr << "FAIL: Unknown option '" << arg << "'\n";
                return 1;
            }
        }

        if (hasThreads) {
            throw std::runtime_error("--threads is only valid with bench");
        }
        if (!encode.empty() && encode != "hex" && encode != "base64" && encode != "raw") {
            throw std::runtime_error("Unsupported --encode value: " + encode + " (use hex, base64 or raw)");
        }
        if (!decode.empty() && decode != "hex" && decode != "base64" && decode != "raw") {
            throw std::runtime_error("Unsupported --decode value: " + decode + " (use hex, base64 or raw)");
        }
        if (isEncrypt && !decode.empty()) {
            throw std::runtime_error("--decode is only valid with decrypt");
        }

        const int keySources = (!keyHex.empty() ? 1 : 0) + (!keyFile.empty() ? 1 : 0);
        if (keySources > 1) {
            throw std::runtime_error("Use either --key or --key-hex, not both");
        }
        const int ivSources = (!ivHex.empty() ? 1 : 0) + (!ivFile.empty() ? 1 : 0) +
                              (!nonceHex.empty() ? 1 : 0) + (!nonce.empty() ? 1 : 0);
        if (ivSources > 1) {
            throw std::runtime_error("Use only one of --iv, --iv-hex, --nonce or --nonce-hex");
        }

        // Key acquisition
        std::vector<uint8_t> key;
        if (!keyHex.empty()) {
            key = hexToBytes(keyHex);
        } else if (!keyFile.empty()) {
            key = readFile(keyFile);
        } else {
            throw std::runtime_error("Key must be specified using --key or --key-hex");
        }

        // IV acquisition. --nonce is an alias: a file path, or a 32-digit hex
        // string when it cannot be opened as a file.
        std::vector<uint8_t> iv;
        if (!ivHex.empty()) {
            iv = hexToBytes(ivHex);
        } else if (!nonceHex.empty()) {
            iv = hexToBytes(nonceHex);
        } else if (!ivFile.empty()) {
            iv = readFile(ivFile);
        } else if (!nonce.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(nativePath(nonce), ec)) {
                iv = readFile(nonce);
            } else if (looksLikeHex(nonce)) {
                iv = hexToBytes(nonce);
            } else {
                iv = readFile(nonce);
            }
        } else {
            throw std::runtime_error("IV must be specified using --iv, --iv-hex or --nonce");
        }

        if (iv.size() != 16) {
            throw std::runtime_error("Invalid IV length: " + std::to_string(iv.size()) +
                                     " bytes (must be exactly 16 bytes)");
        }

        // Data acquisition
        std::vector<uint8_t> input;
        if (!textIn.empty()) {
            input.assign(textIn.begin(), textIn.end());
        } else if (!inFile.empty()) {
            input = readFile(inFile);
        } else {
            throw std::runtime_error("Input must be specified using --in or --text");
        }

        if (!isEncrypt) {
            if (!decode.empty()) {
                input = decodeBytes(input, decode);
            } else if (!textIn.empty() && looksLikeHex(textIn)) {
                input = hexToBytes(textIn);
            }
        }

        Modes modes;
        Block dataUnit = makeBlock(iv.data());
        std::vector<uint8_t> output;

        if (mode == "ctr") {
            AES cipher(key, getKeyLength(key.size()));
            output = modes.ctr_encrypt(cipher, input, dataUnit);
        } else if (mode == "xts") {
            if (key.size() % 2 != 0) {
                throw std::runtime_error("XTS key must be two equal halves (32, 48, or 64 bytes)");
            }
            size_t half = key.size() / 2;
            AES::KeyLength halfLength = getKeyLength(half);
            AES key1(key.data(), half, halfLength);
            AES key2(key.data() + half, half, halfLength);
            output = isEncrypt ? modes.xts_encrypt(key1, key2, input, dataUnit)
                               : modes.xts_decrypt(key1, key2, input, dataUnit);
        } else {
            throw std::runtime_error("Unsupported mode: " + mode + " (use ctr or xts)");
        }

        const std::string outputEncoding =
            encode.empty() ? (outFile.empty() ? "hex" : "raw") : encode;

        if (!outFile.empty()) {
            writeFile(outFile, encodeBytes(output, outputEncoding));
        }

        if (outHex || outFile.empty()) {
            if (outHex) {
                std::cout << bytesToHex(output.data(), output.size()) << "\n";
            } else if (outputEncoding == "hex") {
                std::cout << bytesToHex(output.data(), output.size()) << "\n";
            } else if (outputEncoding == "base64") {
                std::cout << base64Encode(output) << "\n";
            } else {
                std::cout.write(reinterpret_cast<const char*>(output.data()),
                                static_cast<std::streamsize>(output.size()));
            }
        }

        if (verbose) {
            std::cerr << "mode=" << mode << " key_bits=" << (key.size() * 8)
                      << " iv=" << bytesToHex(iv.data(), iv.size())
                      << " input_bytes=" << input.size()
                      << " output_bytes=" << output.size()
                      << " encode=" << outputEncoding << "\n";
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}

} // namespace

#ifdef _WIN32

extern "C" __declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(
    const wchar_t* cmdline, int* argc);

int main() {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) {
        std::cerr << "FAIL: cannot parse command line\n";
        return 1;
    }

    std::vector<std::string> utf8Args;
    utf8Args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0,
                                            nullptr, nullptr);
        if (len <= 0) {
            LocalFree(wargv);
            std::cerr << "FAIL: cannot convert argument " << i << " to UTF-8\n";
            return 1;
        }
        std::string buf(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &buf[0], len, nullptr, nullptr);
        buf.pop_back();
        utf8Args.push_back(std::move(buf));
    }
    LocalFree(wargv);

    std::vector<char*> ptrs;
    ptrs.reserve(utf8Args.size());
    for (auto& s : utf8Args) ptrs.push_back(s.data());
    return realMain(static_cast<int>(utf8Args.size()), ptrs.data());
}

#else

int main(int argc, char* argv[]) { return realMain(argc, argv); }

#endif
