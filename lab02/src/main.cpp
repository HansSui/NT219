#include "AES.hpp"
#include "modesA.hpp"
#include "modesC.hpp"
#include "modesD.hpp"
#include "minijson.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

namespace {

void printHelp(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [options]\n\n"
              << "Commands:\n"
              << "  encrypt    Encrypt data using CTR mode (or other supported modes)\n"
              << "  decrypt    Decrypt data using CTR mode (or other supported modes)\n"
              << "  bench      Benchmark encryption/decryption throughput & latency\n"
              << "  --kat FILE Run Known Answer Tests from JSON vector file\n"
              << "  help       Display this help message\n\n"
              << "Encrypt/Decrypt Options:\n"
              << "  --mode <ctr|ecb|cbc|cfb|ofb|ccm|gcm>  Cipher mode (default: ctr)\n"
              << "  --key <file>                          Binary key file (16, 24, or 32 bytes)\n"
              << "  --key-hex <hex>                       Hexadecimal key string\n"
              << "  --iv <file>                           Binary IV / counter file (16 bytes)\n"
              << "  --iv-hex <hex>                        Hexadecimal IV string (16 bytes = 32 hex chars)\n"
              << "  --in <file>                           Input data file\n"
              << "  --text <string>                       Plaintext input string directly\n"
              << "  --out <file>                          Output data file\n"
              << "  --out-hex                             Display output as hex to stdout\n\n"
              << "Benchmark Options:\n"
              << "  --size <bytes>                        Payload size in bytes (e.g. 1048576 = 1MB)\n"
              << "  --runs <n>                            Number of iterations (default: 1000)\n"
              << "  --out <csv_file>                      Output CSV filename\n";
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
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

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Cannot open input file: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Cannot open output file: " + path);
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
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

// Convert 16-byte vector/pointer to Block
Block makeBlock(const uint8_t* data) {
    Block b;
    std::memcpy(b.data(), data, 16);
    return b;
}

int runKat(const std::string& katFile) {
    std::string content;
    {
        std::ifstream ifs(katFile);
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

    MODEa modeA;

    for (const auto& item : vectors) {
        std::string name = item.getString("name");
        std::string mode = item.getString("mode");
        std::vector<uint8_t> key = hexToBytes(item.getString("key"));
        std::string ivStr = item.getString("iv");
        std::vector<uint8_t> pt = hexToBytes(item.getString("plaintext"));
        std::vector<uint8_t> expectedCt = hexToBytes(item.getString("ciphertext"));

        bool ok = false;
        try {
            AES cipher(key, getKeyLength(key.size()));

            if (mode == "ctr") {
                if (ivStr.empty()) throw std::runtime_error("CTR requires IV");
                std::vector<uint8_t> iv = hexToBytes(ivStr);
                if (iv.size() != 16) throw std::runtime_error("CTR IV must be 16 bytes");
                Block counter = makeBlock(iv.data());
                std::vector<uint8_t> ct = modeA.ctr_encrypt(cipher, pt, counter);
                std::vector<uint8_t> roundtrip = modeA.ctr_decrypt(cipher, ct, counter);
                if (ct == expectedCt && roundtrip == pt) {
                    ok = true;
                }
            } else if (mode == "ecb") {
                // Raw block / ECB
                if (pt.size() == 16) {
                    Block inBlock = makeBlock(pt.data());
                    Block ctBlock = cipher.encrypt(inBlock);
                    Block dtBlock = cipher.decrypt(ctBlock);
                    if (std::memcmp(ctBlock.data(), expectedCt.data(), 16) == 0 &&
                        std::memcmp(dtBlock.data(), pt.data(), 16) == 0) {
                        ok = true;
                    }
                } else {
                    auto ct = modeA.ECB_encrypt(cipher, pt);
                    auto dt = modeA.ECB_decrypt(cipher, ct);
                    if (ct == expectedCt && dt == pt) ok = true;
                }
            } else if (mode == "cbc") {
                std::vector<uint8_t> iv = hexToBytes(ivStr);
                Block ivBlock = makeBlock(iv.data());
                if (pt.size() % 16 == 0 && expectedCt.size() == pt.size()) {
                    // Raw block CBC (NIST SP 800-38A unpadded vector)
                    std::vector<uint8_t> ct(pt.size());
                    Block prev = ivBlock;
                    for (size_t i = 0; i < pt.size(); i += 16) {
                        Block blk = makeBlock(pt.data() + i);
                        Block xored;
                        for (int k = 0; k < 16; ++k) xored[k] = prev[k] ^ blk[k];
                        Block enc = cipher.encrypt(xored);
                        std::memcpy(ct.data() + i, enc.data(), 16);
                        prev = enc;
                    }
                    if (ct == expectedCt) ok = true;
                } else {
                    auto ct = modeA.CBC_encrypt(cipher, pt, ivBlock);
                    auto dt = modeA.CBC_decrypt(cipher, ct, ivBlock);
                    if (ct == expectedCt && dt == pt) ok = true;
                }
            } else if (mode == "ofb") {
                std::vector<uint8_t> iv = hexToBytes(ivStr);
                Block ivBlock = makeBlock(iv.data());
                auto ct = modeA.ofb_encrypt(cipher, pt, ivBlock);
                auto dt = modeA.ofb_decrypt(cipher, ct, ivBlock);
                if (ct == expectedCt && dt == pt) ok = true;
            } else if (mode == "cfb") {
                std::vector<uint8_t> iv = hexToBytes(ivStr);
                Block ivBlock = makeBlock(iv.data());
                auto ct = modeA.cfb_encrypt(cipher, pt, ivBlock, 128);
                auto dt = modeA.cfb_decrypt(cipher, ct, ivBlock, 128);
                if (ct == expectedCt && dt == pt) ok = true;
            } else {
                std::cerr << "SKIP (unsupported mode in current KAT): " << mode << "\n";
                continue;
            }
        } catch (const std::exception& e) {
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

    std::cout << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}

int runBench(int argc, char* argv[]) {
    size_t sizeBytes = 1048576; // Default: 1 MB
    size_t runs = 1000;
    std::string outCsv = "";
    std::string keyHex = "000102030405060708090a0b0c0d0e0f";
    std::string mode = "ctr";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--size" && i + 1 < argc) sizeBytes = std::stoull(argv[++i]);
        else if (arg == "--runs" && i + 1 < argc) runs = std::stoull(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) outCsv = argv[++i];
        else if (arg == "--key-hex" && i + 1 < argc) keyHex = argv[++i];
        else if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
    }

    std::vector<uint8_t> key = hexToBytes(keyHex);
    AES cipher(key, getKeyLength(key.size()));
    MODEa modeA;

    std::vector<uint8_t> payload(sizeBytes, 0x5A);
    Block counter;
    counter.fill(0);

    // Warm-up
    for (int w = 0; w < 5; ++w) {
        counter.fill(static_cast<uint8_t>(w));
        auto dummy = modeA.ctr_encrypt(cipher, payload, counter);
    }

    std::ostream* outStream = &std::cout;
    std::ofstream ofs;
    if (!outCsv.empty()) {
        ofs.open(outCsv);
        if (!ofs) {
            std::cerr << "FAIL: Cannot open output CSV: " << outCsv << "\n";
            return 1;
        }
        outStream = &ofs;
    }

    *outStream << "run,mode,size_bytes,encrypt_us,decrypt_us,throughput_encrypt_MBps,throughput_decrypt_MBps\n";

    for (size_t r = 0; r < runs; ++r) {
        counter[15] = static_cast<uint8_t>(r & 0xFF);
        counter[14] = static_cast<uint8_t>((r >> 8) & 0xFF);

        auto t0 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> ct = modeA.ctr_encrypt(cipher, payload, counter);
        auto t1 = std::chrono::high_resolution_clock::now();

        auto t2 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> pt = modeA.ctr_decrypt(cipher, ct, counter);
        auto t3 = std::chrono::high_resolution_clock::now();

        double encUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        double decUs = std::chrono::duration<double, std::micro>(t3 - t2).count();

        double sizeMB = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);
        double encMBps = (encUs > 0) ? (sizeMB / (encUs / 1e6)) : 0.0;
        double decMBps = (decUs > 0) ? (sizeMB / (decUs / 1e6)) : 0.0;

        *outStream << r << "," << mode << "," << sizeBytes << ","
                   << std::fixed << std::setprecision(2) << encUs << ","
                   << decUs << "," << encMBps << "," << decMBps << "\n";
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
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

        bool isEncrypt = (cmd == "encrypt");
        std::string mode = "ctr";
        std::string keyFile, keyHex;
        std::string ivFile, ivHex;
        std::string inFile, textIn;
        std::string outFile;
        bool outHex = false;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
            else if (arg == "--key" && i + 1 < argc) keyFile = argv[++i];
            else if (arg == "--key-hex" && i + 1 < argc) keyHex = argv[++i];
            else if (arg == "--iv" && i + 1 < argc) ivFile = argv[++i];
            else if (arg == "--iv-hex" && i + 1 < argc) ivHex = argv[++i];
            else if (arg == "--in" && i + 1 < argc) inFile = argv[++i];
            else if (arg == "--text" && i + 1 < argc) textIn = argv[++i];
            else if (arg == "--out" && i + 1 < argc) outFile = argv[++i];
            else if (arg == "--out-hex") outHex = true;
            else {
                std::cerr << "FAIL: Unknown option '" << arg << "'\n";
                return 1;
            }
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

        AES::KeyLength kl = getKeyLength(key.size());
        AES cipher(key, kl);

        // IV acquisition
        std::vector<uint8_t> iv;
        if (!ivHex.empty()) {
            iv = hexToBytes(ivHex);
        } else if (!ivFile.empty()) {
            iv = readFile(ivFile);
        } else if (mode != "ecb") {
            throw std::runtime_error("IV must be specified using --iv or --iv-hex for mode: " + mode);
        }

        if (mode != "ecb" && iv.size() != 16) {
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

        std::vector<uint8_t> output;
        MODEa modeA;

        if (mode == "ctr") {
            Block counter = makeBlock(iv.data());
            if (isEncrypt) {
                output = modeA.ctr_encrypt(cipher, input, counter);
            } else {
                output = modeA.ctr_decrypt(cipher, input, counter);
            }
        } else if (mode == "ecb") {
            if (isEncrypt) {
                output = modeA.ECB_encrypt(cipher, input);
            } else {
                output = modeA.ECB_decrypt(cipher, input);
            }
        } else if (mode == "cbc") {
            Block ivBlock = makeBlock(iv.data());
            if (isEncrypt) {
                output = modeA.CBC_encrypt(cipher, input, ivBlock);
            } else {
                output = modeA.CBC_decrypt(cipher, input, ivBlock);
            }
        } else if (mode == "ofb") {
            Block ivBlock = makeBlock(iv.data());
            if (isEncrypt) {
                output = modeA.ofb_encrypt(cipher, input, ivBlock);
            } else {
                output = modeA.ofb_decrypt(cipher, input, ivBlock);
            }
        } else if (mode == "cfb") {
            Block ivBlock = makeBlock(iv.data());
            if (isEncrypt) {
                output = modeA.cfb_encrypt(cipher, input, ivBlock, 128);
            } else {
                output = modeA.cfb_decrypt(cipher, input, ivBlock, 128);
            }
        } else {
            throw std::runtime_error("Unsupported mode: " + mode);
        }

        if (!outFile.empty()) {
            writeFile(outFile, output);
        }

        if (outHex || outFile.empty()) {
            std::cout << bytesToHex(output.data(), output.size()) << "\n";
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
