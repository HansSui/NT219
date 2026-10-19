# Lab 1: Comprehensive Ubuntu Execution Guide (`lab_ubuntu_runguide.md`)

> **Note:** All commands are tailored specifically for the environment on Ubuntu, with output directories correctly aligned to `crypto-labs/lab1/output/ubuntu`.

---

## 0. Initial Setup & Directory Preparation

Run all commands from the repository root: `/home/dell/home-crypto/crypto-labs/lab1`

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# Create all required output directories
mkdir -p output/ubuntu/key_bin
mkdir -p output/ubuntu/neg_results
mkdir -p output/ubuntu/benchmarks
```

---

## 1. Install Dependencies

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ clang libssl-dev
```

---

## 2. Build Project (GCC & Clang)

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# Build with GCC
scripts/build_linux.sh gcc

# Build with Clang
scripts/build_linux.sh clang
```

**Expected Build Output Executables:**
- `build_ubuntu_gcc/aestool`
- `build_ubuntu_clang/aestool`

---

## 3. Known Answer Tests (KAT)

Execute KAT vectors and store outputs in `output/ubuntu/`:

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# KAT - GCC
./build_ubuntu_gcc/aestool --kat kat/vectors.json > output/ubuntu/kat_ubuntu_gcc.txt 2>&1

# KAT - Clang
./build_ubuntu_clang/aestool --kat kat/vectors.json > output/ubuntu/kat_ubuntu_clang.txt 2>&1
```

---

## 4. Unit Tests (Catch2 via CTest)

```bash
# Unit Test - GCC
cd /home/dell/home-crypto/crypto-labs/lab1/build_ubuntu_gcc
ctest --output-on-failure > /home/dell/home-crypto/crypto-labs/lab1/output/ubuntu/test_ubuntu_gcc.txt 2>&1

# Unit Test - Clang
cd /home/dell/home-crypto/crypto-labs/lab1/build_ubuntu_clang
ctest --output-on-failure > /home/dell/home-crypto/crypto-labs/lab1/output/ubuntu/test_ubuntu_clang.txt 2>&1
```

---

## 5. Prepare Test Data for Negative Testing

Generate keys, IVs, plaintexts, and ciphertexts in `output/ubuntu/key_bin/`:

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# 1. Generate keys & IVs
head -c 32 /dev/urandom > output/ubuntu/key_bin/key.bin
head -c 32 /dev/urandom > output/ubuntu/key_bin/key_wrong.bin
head -c 16 /dev/urandom > output/ubuntu/key_bin/iv.bin
head -c 16 /dev/urandom > output/ubuntu/key_bin/iv_wrong.bin
head -c 8 /dev/urandom > output/ubuntu/key_bin/short_iv.bin

# 2. Generate sample plaintext
echo "Hello World plaintext for testing negative cases" > output/ubuntu/key_bin/plain.txt

# 3. Encrypt plain text to create valid ciphertext (CBC & GCM)
./build_ubuntu_gcc/aestool encrypt --mode cbc --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct.bin
./build_ubuntu_gcc/aestool encrypt --mode gcm --aead --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct_gcm.bin
```

---

## 6. Negative Testing (Fail-Closed Validations)

Run negative tests and capture outputs into `output/ubuntu/neg_results/`:

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# 1. Test wrong key
./build_ubuntu_gcc/aestool decrypt --mode cbc --key output/ubuntu/key_bin/key_wrong.bin --in output/ubuntu/key_bin/ct.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_key.txt

# 2. Test wrong IV
./build_ubuntu_gcc/aestool decrypt --mode cbc --key output/ubuntu/key_bin/key.bin --iv output/ubuntu/key_bin/iv_wrong.bin --in output/ubuntu/key_bin/ct.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_iv.txt

# 3. Test wrong AEAD Tag (GCM)
./build_ubuntu_gcc/aestool decrypt --mode gcm --aead --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/ct_gcm.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_tag.txt

# 4. Test wrong IV length
./build_ubuntu_gcc/aestool encrypt --mode cbc --key output/ubuntu/key_bin/key.bin --iv output/ubuntu/key_bin/short_iv.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
dd if=/dev/zero of=output/ubuntu/key_bin/large.bin bs=1K count=32
./build_ubuntu_gcc/aestool encrypt --mode ecb --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/large.bin --out output/ubuntu/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ecb.txt
```

---

## 7. Performance Benchmarks

Set CPU frequency governor to `performance` and benchmark modes:

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# Set governor to performance mode for consistent measurement
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Run Benchmarks (GCC)
./build_ubuntu_gcc/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 1024 --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_gcc_gcm_1KB.csv
./build_ubuntu_gcc/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 4096 --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_gcc_gcm_4KB.csv
./build_ubuntu_gcc/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 16384 --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_gcc_gcm_16KB.csv
./build_ubuntu_gcc/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 262144 --runs 100 --out output/ubuntu/benchmarks/bench_ubuntu_gcc_gcm_256KB.csv
./build_ubuntu_gcc/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 1048576 --runs 50 --out output/ubuntu/benchmarks/bench_ubuntu_gcc_gcm_1MB.csv
./build_ubuntu_gcc/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 8388608 --runs 20 --out output/ubuntu/benchmarks/bench_ubuntu_gcc_gcm_8MB.csv

# Run Benchmarks (Clang)
./build_ubuntu_clang/aestool bench --mode gcm --key output/ubuntu/key_bin/key.bin --size 16384 --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_clang_gcm_16KB.csv
```

`plot_bench.py` run:

```bash
cd ~/crypto-labs/lab1

mkdir -p output/ubuntu/benchmarks/graph

# Vòng lặp chạy script cho từng file CSV và đẩy kết quả ra thư mục graph
for csv_file in output/ubuntu/benchmarks/*.csv; do
    python3 scripts/plot_bench.py "$csv_file" --out-dir output/ubuntu/benchmarks/graph --platform linux
done
```

---

## 8. Export Shared Library (Optional)

```bash
cd /home/dell/home-crypto/crypto-labs/lab1
cmake -S . -B build_ubuntu_gcc_shared -DBUILD_SHARED_LIBS=ON
cmake --build build_ubuntu_gcc_shared
```