// Test-only OpenSSL AES-128-CTR throughput oracle. Never linked into aestool.
#include <openssl/err.h>
#include <openssl/evp.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string openssl_error() {
    char buffer[256];
    ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
    return buffer;
}

std::vector<uint8_t> evp_ctr(const std::vector<uint8_t>& key,
                             const std::vector<uint8_t>& iv,
                             const std::vector<uint8_t>& input) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) throw std::runtime_error("EVP_CIPHER_CTX_new: " + openssl_error());

    std::vector<uint8_t> output(input.size() + EVP_MAX_BLOCK_LENGTH);
    int out_len = 0;
    int final_len = 0;
    const bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), iv.data()) == 1 &&
        // shortcut: one EVP update caps at INT_MAX bytes; benchmark sizes <= 1 GiB.
        EVP_EncryptUpdate(ctx, output.data(), &out_len, input.data(),
                          static_cast<int>(input.size())) == 1 &&
        EVP_EncryptFinal_ex(ctx, output.data() + out_len, &final_len) == 1;

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) throw std::runtime_error("OpenSSL EVP: " + openssl_error());

    output.resize(static_cast<size_t>(out_len + final_len));
    return output;
}

std::vector<uint8_t> add_blocks(std::vector<uint8_t> counter, uint64_t blocks) {
    for (int i = 15; i >= 0 && blocks != 0; --i) {
        const uint64_t sum = static_cast<uint64_t>(counter[static_cast<size_t>(i)]) + (blocks & 0xff);
        counter[static_cast<size_t>(i)] = static_cast<uint8_t>(sum);
        blocks = (blocks >> 8) + (sum >> 8);
    }
    return counter;
}

std::vector<uint8_t> evp_ctr_threaded(const std::vector<uint8_t>& key,
                                      const std::vector<uint8_t>& iv,
                                      const std::vector<uint8_t>& input,
                                      int threads) {
    if (threads <= 1 || input.size() < 65536) return evp_ctr(key, iv, input);

    const size_t blocks = (input.size() + 15) / 16;
    const size_t per_thread = (blocks + static_cast<size_t>(threads) - 1) /
                              static_cast<size_t>(threads);
    std::vector<std::vector<uint8_t>> parts(static_cast<size_t>(threads));
    std::vector<std::thread> workers;

    for (int t = 0; t < threads; ++t) {
        const size_t start_block = static_cast<size_t>(t) * per_thread;
        if (start_block >= blocks) break;
        const size_t end_block = std::min(blocks, start_block + per_thread);
        const size_t begin = start_block * 16;
        const size_t finish = std::min(input.size(), end_block * 16);
        std::vector<uint8_t> chunk(input.begin() + static_cast<std::ptrdiff_t>(begin),
                                   input.begin() + static_cast<std::ptrdiff_t>(finish));
        std::vector<uint8_t> chunk_iv = add_blocks(iv, start_block);
        workers.emplace_back([&key, &parts, t, chunk = std::move(chunk), chunk_iv = std::move(chunk_iv)]() mutable {
            parts[static_cast<size_t>(t)] = evp_ctr(key, chunk_iv, chunk);
        });
    }
    for (auto& worker : workers) worker.join();

    std::vector<uint8_t> out;
    out.reserve(input.size());
    for (const auto& part : parts) out.insert(out.end(), part.begin(), part.end());
    return out;
}

double elapsed_seconds(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main(int argc, char** argv) {
    size_t size = 1 << 20;
    size_t runs = 30;
    double warmup = 1.0;
    int threads = 1;
    std::string out_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--size" && i + 1 < argc) size = std::stoull(argv[++i]);
        else if (arg == "--runs" && i + 1 < argc) runs = std::stoull(argv[++i]);
        else if (arg == "--warmup" && i + 1 < argc) warmup = std::stod(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) threads = std::stoi(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) out_path = argv[++i];
        else {
            std::cerr << "usage: bench_openssl [--size BYTES] [--runs N] "
                         "[--warmup SECONDS] [--threads N] [--out CSV]\n";
            return 2;
        }
    }
    if (size == 0 || runs == 0 || threads < 1 || threads > 256) {
        std::cerr << "size/runs must be positive; threads must be 1..256\n";
        return 2;
    }

    const std::vector<uint8_t> key(16, 0x2b);
    const std::vector<uint8_t> iv(16, 0x00);
    std::vector<uint8_t> plaintext(size);
    for (size_t i = 0; i < size; ++i) plaintext[i] = static_cast<uint8_t>(i * 31 + 7);

    try {
        if (threads > 1) {
            std::vector<uint8_t> probe(65539);  // deliberately not a multiple of 16
            for (size_t i = 0; i < probe.size(); ++i) probe[i] = static_cast<uint8_t>(i * 17 + 3);
            if (evp_ctr_threaded(key, iv, probe, threads) != evp_ctr(key, iv, probe)) {
                std::cerr << "threaded CTR does not match single-thread CTR\n";
                return 1;
            }
        }

        if (warmup > 0) {
            const auto start = std::chrono::steady_clock::now();
            while (elapsed_seconds(start) < warmup) evp_ctr_threaded(key, iv, plaintext, threads);
        }

        std::ofstream file;
        std::ostream* out = &std::cout;
        if (!out_path.empty()) {
            file.open(out_path);
            if (!file) {
                std::cerr << "cannot open " << out_path << "\n";
                return 1;
            }
            out = &file;
        }

        *out << "run,mode,size_bytes,encrypt_us,decrypt_us,"
                "throughput_encrypt_MBps,throughput_decrypt_MBps\n";
        *out << std::fixed << std::setprecision(6);

        const double size_mib = static_cast<double>(size) / (1024.0 * 1024.0);
        for (size_t run = 0; run < runs; ++run) {
            const auto enc_start = std::chrono::steady_clock::now();
            const std::vector<uint8_t> ciphertext = evp_ctr_threaded(key, iv, plaintext, threads);
            const double enc_us =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - enc_start).count();

            const auto dec_start = std::chrono::steady_clock::now();
            const std::vector<uint8_t> decrypted = evp_ctr_threaded(key, iv, ciphertext, threads);
            const double dec_us =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - dec_start).count();

            if (decrypted != plaintext) {
                std::cerr << "OpenSSL CTR round-trip failed\n";
                return 1;
            }

            *out << run << ",ctr," << size << ',' << enc_us << ',' << dec_us << ','
                 << (size_mib / (enc_us / 1e6)) << ',' << (size_mib / (dec_us / 1e6)) << '\n';
        }

        std::cerr << "bench_openssl: size=" << size << " runs=" << runs
                  << " threads=" << threads << "\n";
    } catch (const std::exception& error) {
        std::cerr << "bench_openssl: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
