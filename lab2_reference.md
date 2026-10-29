# Lab 2 Master Reference Guide: AES-128 & CTR Mode in Pure C++

> **Course:** NT219 — Information Security / Cryptography Laboratory  
> **Topic:** Lab 2 — AES-128 Implementation in Pure C++ (CTR Mode, FIPS-197 Compliant)  
> **Specification Reference:** `Lab2.txt` & [lab requirement.md](file:///home/han-sui/Downloads/NT219/lab%20requirement.md)  
> **Target Platforms:** Ubuntu 26.04 LTS (GCC 15.2 / Clang 21.1) & Windows 11 (MSVC 19.51 / MinGW64)  
> **Output Path:** [lab2_reference.md](file:///home/han-sui/Downloads/NT219/lab2_reference.md)

---

## 1. Executive Summary & Architectural Paradigm Shift

### 1.1 The Fundamental Constraint Difference: Lab 1 vs. Lab 2

A critical requirement of Lab 2 is the complete inversion of external cryptographic dependencies compared to Lab 1:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                             ARCHITECTURAL CONTRAST                          │
├──────────────────────────────────────┬──────────────────────────────────────┤
│                LAB 1                 │                LAB 2                 │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ • Crypto++ was STRICTLY REQUIRED     │ • External crypto libraries STRICTLY │
│   for all primitives.                │   PROHIBITED for all cipher logic.   │
│ • No custom AES round logic allowed. │ • Pure C++ STL ONLY for primitives.  │
│ • OpenSSL was forbidden.             │ • Manual implementation of:          │
│ • Goal: Library API hygiene, mode    │   SubBytes, ShiftRows, MixColumns,   │
│   selection, IV handling, AEAD.      │   AddRoundKey, KeyExpansion & CTR.   │
│                                      │ • Crypto++ / OpenSSL permitted ONLY  │
│                                      │   as ORACLES for cross-validation!   │
└──────────────────────────────────────┴──────────────────────────────────────┘
```

### 1.2 Learning Outcomes & Grade Breakdown (100 Pts + 20 Bonus)

| Component | Points | Core Deliverables & Technical Criteria |
|---|---|---|
| **FIPS-197 AES-128 Core** | **30 pts** | Manual AddRoundKey, SubBytes, ShiftRows, MixColumns, KeyExpansion (10 rounds, 128-bit key). Strict column-major state matrix. |
| **CTR Mode Implementation** | **15 pts** | NIST SP 800-38A compliant stream cipher. 128-bit IV, big-endian counter increment, arbitrary plaintext length, partial final block XOR, no padding. |
| **KATs & Cross-Validation** | **15 pts** | FIPS-197 Appendix C test vectors, NIST SP 800-38A CTR vectors, automated `--kat` runner, byte-for-byte equivalence with Crypto++/OpenSSL oracle. |
| **Engineering & Documentation** | **10 pts** | CMake build system, fail-closed CLI, unit test suite (Catch2 + CTest), clean modular structure. |
| **Performance Evaluation** | **10 pts** | Benchmarks at 1 MiB, 100 MiB, 1 GiB. Table-based vs computed S-box analysis, memory footprint, cache line behavior, branch prediction. |
| **Security Engineering Discussion** | **20 pts** | Timing attacks, S-box cache leakage, bitslicing vs AES-NI, keystream reuse catastrophe, CTR malleability and lack of authentication. |
| **Bonus A: AES-192 / AES-256** | **+10 pts** | 12-round and 14-round key schedules, extra SubWord step for AES-256 ($i \equiv 4 \pmod 8$), KAT verification. |
| **Bonus B: AES-XTS Mode** | **+5 pts** | IEEE P1619 two-key cipher, tweak Galois multiplication in $GF(2^{128})$, disk sector encryption architecture. |
| **Bonus C: Hardware AES-NI** | **+5 pts** | CPUID runtime capability detection, `_mm_aesenc_si128` intrinsics, constant-time software vs hardware analysis. |

---

## 2. Mathematical Foundations & FIPS-197 Core Specification

### 2.1 Finite Field Arithmetic in $GF(2^8)$

AES operates on elements of the Galois Field $GF(2^8)$, represented as polynomials of degree $< 8$ with coefficients in $\{0, 1\}$.

#### Irreducible Reduction Polynomial
All multiplications are performed modulo the irreducible degree-8 polynomial:
$$m(x) = x^8 + x^4 + x^3 + x + 1 \quad (\text{Binary: } 100011011_2 = \text{0x11B})$$

#### Field Addition & Subtraction
Addition and subtraction in $GF(2^8)$ are identical and correspond to the bitwise XOR ($\oplus$) operation:
$$a(x) \oplus b(x) = \sum_{i=0}^7 (a_i \oplus b_i) x^i$$

#### Multiplication by $x$ (`xtime`)
Multiplying a byte $b = (b_7 b_6 b_5 b_4 b_3 b_2 b_1 b_0)_2$ by $x$ (value `0x02`):
1. Shift $b$ left by 1 bit: $b \ll 1$.
2. If the original high bit $b_7 == 1$, XOR with reduction constant `0x1B`:
```cpp
inline uint8_t xtime(uint8_t b) noexcept {
    return static_cast<uint8_t>((b << 1) ^ ((b & 0x80) ? 0x1B : 0x00));
}
```

#### Arbitrary Field Multiplication (`gmul`)
Using Russian Peasant Multiplication:
```cpp
uint8_t gmul(uint8_t a, uint8_t b) noexcept {
    uint8_t p = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) p ^= a;
        bool hi = (a & 0x80);
        a <<= 1;
        if (hi) a ^= 0x1B;
        b >>= 1;
    }
    return p;
}
```

---

### 2.2 S-Box Construction: Inversion & Affine Transformation

The AES S-box is a non-linear byte substitution $S: GF(2^8) \to GF(2^8)$.

#### Step 1: Multiplicative Inverse
For any input byte $a \in GF(2^8)$:
* If $a = 0$, $a^{-1} = 0$.
* If $a \neq 0$, $a^{-1} = a^{2^{8}-2} = a^{254} \pmod{m(x)}$ (by Fermat's Little Theorem).

#### Step 2: Affine Transformation over $GF(2)$
Given the inverse byte $b = a^{-1} = (b_7, b_6, \dots, b_0)^T$, the S-box output $s = (s_7, s_6, \dots, s_0)^T$ is:
$$\begin{pmatrix} s_0 \\ s_1 \\ s_2 \\ s_3 \\ s_4 \\ s_5 \\ s_6 \\ s_7 \end{pmatrix} = 
\begin{pmatrix}
1 & 0 & 0 & 0 & 1 & 1 & 1 & 1 \\
1 & 1 & 0 & 0 & 0 & 1 & 1 & 1 \\
1 & 1 & 1 & 0 & 0 & 0 & 1 & 1 \\
1 & 1 & 1 & 1 & 0 & 0 & 0 & 1 \\
1 & 1 & 1 & 1 & 1 & 0 & 0 & 0 \\
0 & 1 & 1 & 1 & 1 & 1 & 0 & 0 \\
0 & 0 & 1 & 1 & 1 & 1 & 1 & 0 \\
0 & 0 & 0 & 1 & 1 & 1 & 1 & 1
\end{pmatrix}
\begin{pmatrix} b_0 \\ b_1 \\ b_2 \\ b_3 \\ b_4 \\ b_5 \\ b_6 \\ b_7 \end{pmatrix}
\oplus
\begin{pmatrix} 1 \\ 1 \\ 0 \\ 0 \\ 0 \\ 1 \\ 1 \\ 0 \end{pmatrix}$$
Equivalent bitwise formulation:
$$s_i = b_i \oplus b_{(i+4)\bmod 8} \oplus b_{(i+5)\bmod 8} \oplus b_{(i+6)\bmod 8} \oplus b_{(i+7)\bmod 8} \oplus c_i \quad (\text{where } c = \text{0x63})$$

---

### 2.3 State Matrix Representation: Column-Major Ordering

> [!CAUTION]
> **CRITICAL PITFALL: Matrix Transposition Error**  
> FIPS-197 Section 3.4 mandates that the 16-byte block $in_0, in_1, \dots, in_{15}$ is mapped to the $4 \times 4$ State matrix in **column-major order**:
> $$s[r, c] = in[r + 4c] = in[4c + r] \quad (0 \le r < 4, 0 \le c < 4)$$
> 
> $$\text{State} = \begin{bmatrix}
> s_{0,0} & s_{0,1} & s_{0,2} & s_{0,3} \\
> s_{1,0} & s_{1,1} & s_{1,2} & s_{1,3} \\
> s_{2,0} & s_{2,1} & s_{2,2} & s_{2,3} \\
> s_{3,0} & s_{3,1} & s_{3,2} & s_{3,3}
> \end{bmatrix} = \begin{bmatrix}
> in_0 & in_4 & in_8  & in_{12} \\
> in_1 & in_5 & in_9  & in_{13} \\
> in_2 & in_6 & in_{10} & in_{14} \\
> in_3 & in_7 & in_{11} & in_{15}
> \end{bmatrix}$$
> 
> Accessing the input as `in[r * 4 + c]` transposes the matrix, causing intermediate diffusion steps (`ShiftRows` and `MixColumns`) to fail completely against NIST KAT vectors.

#### Correct State Conversion Functions
```cpp
using Block = std::array<uint8_t, 16>;
using State = std::array<std::array<uint8_t, 4>, 4>; // State[col][row]

State bytes_to_state(const Block& block) noexcept {
    State state{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            state[c][r] = block[c * 4 + r]; // Column-major!
        }
    }
    return state;
}

Block state_to_bytes(const State& state) noexcept {
    Block block{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            block[c * 4 + r] = state[c][r]; // Column-major!
        }
    }
    return block;
}
```

---

### 2.4 The Four AES Round Transformations

```
Input Block ──► AddRoundKey (Round 0)
                      │
   ┌──────────────────┴──────────────────┐
   │ Loop: Round 1 to Nr - 1             │
   │  1. SubBytes   (Non-linear S-box)   │
   │  2. ShiftRows  (Permutation)        │
   │  3. MixColumns (MDS Diffusion)      │
   │  4. AddRoundKey(Round Key w[r])     │
   └──────────────────┬──────────────────┘
                      │
   ┌──────────────────┴──────────────────┐
   │ Final Round (Round Nr)              │
   │  1. SubBytes                        │
   │  2. ShiftRows                       │
   │  3. AddRoundKey(NO MixColumns!)     │
   └──────────────────┬──────────────────┘
                      ▼
               Ciphertext Block
```

#### 1. `SubBytes`
Applies the S-box independently to all 16 state bytes: $s'_{c, r} = \text{SBOX}[s_{c, r}]$.

#### 2. `ShiftRows`
Cyclically shifts the rows of the state to the left by offsets equal to their row indices:
* Row 0 ($r=0$): No shift.
* Row 1 ($r=1$): Circular shift left by 1 byte.
* Row 2 ($r=2$): Circular shift left by 2 bytes.
* Row 3 ($r=3$): Circular shift left by 3 bytes.

#### 3. `MixColumns`
Operates on each column independently as a 4-term polynomial over $GF(2^8)$ multiplied by $c(x) = \{03\}x^3 + \{01\}x^2 + \{01\}x + \{02\} \pmod{x^4 + 1}$:
$$\begin{pmatrix} s'_{0, c} \\ s'_{1, c} \\ s'_{2, c} \\ s'_{3, c} \end{pmatrix} = 
\begin{pmatrix}
02 & 03 & 01 & 01 \\
01 & 02 & 03 & 01 \\
01 & 01 & 02 & 03 \\
03 & 01 & 01 & 02
\end{pmatrix}
\begin{pmatrix} s_{0, c} \\ s_{1, c} \\ s_{2, c} \\ s_{3, c} \end{pmatrix}$$
Implementation using `xtime`:
```cpp
void mix_columns(State& state) noexcept {
    for (int c = 0; c < 4; ++c) {
        uint8_t a0 = state[c][0], a1 = state[c][1], a2 = state[c][2], a3 = state[c][3];
        state[c][0] = xtime(a0 ^ a1) ^ a1 ^ a2 ^ a3;
        state[c][1] = xtime(a1 ^ a2) ^ a2 ^ a3 ^ a0;
        state[c][2] = xtime(a2 ^ a3) ^ a3 ^ a0 ^ a1;
        state[c][3] = xtime(a3 ^ a0) ^ a0 ^ a1 ^ a2;
    }
}
```

#### 4. `AddRoundKey`
Each column of the State is XORed with the corresponding 4-byte word of the expanded round key:
$$s'_{c, r} = s_{c, r} \oplus w[\text{round} \cdot 4 + c]_r$$

---

### 2.5 Key Expansion Schedule

Parameters across AES key lengths:
| Parameter | Description | AES-128 | AES-192 (Bonus) | AES-256 (Bonus) |
|---|---|---|---|---|
| **$Nk$** | Key length in 32-bit words | 4 words (16 B) | 6 words (24 B) | 8 words (32 B) |
| **$Nb$** | Block size in 32-bit words | 4 words (16 B) | 4 words (16 B) | 4 words (16 B) |
| **$Nr$** | Number of cipher rounds | **10 rounds** | **12 rounds** | **14 rounds** |
| **$W$** | Total expanded 32-bit words | 44 words (176 B) | 52 words (208 B) | 60 words (240 B) |

#### Schedule Algorithm
For word index $i = 0$ to $Nb(Nr + 1) - 1$:
1. If $i < Nk$: $w[i] = \text{key}[4i \dots 4i+3]$.
2. If $i \ge Nk$:
   * Let $\text{temp} = w[i - 1]$.
   * If $i \equiv 0 \pmod{Nk}$:
     $$\text{temp} = \text{SubWord}(\text{RotWord}(\text{temp})) \oplus \text{Rcon}[i / Nk]$$
   * **AES-256 Special Rule:** If $Nk > 6$ and $i \equiv 4 \pmod{Nk}$:
     $$\text{temp} = \text{SubWord}(\text{temp})$$
   * $w[i] = w[i - Nk] \oplus \text{temp}$.

---

## 3. NIST SP 800-38A CTR Mode Specification

CTR mode turns the AES block cipher into a deterministic stream cipher.

```
       Counter Block T₁                   Counter Block T₂
     ┌──────────────────┐               ┌──────────────────┐
     │ Nonce ║ Counter₁ │               │ Nonce ║ Counter₂ │
     └────────┬─────────┘               └────────┬─────────┘
              │                                  │
              ▼                                  ▼
      ┌───────────────┐                  ┌───────────────┐
      │   AES_K (•)   │                  │   AES_K (•)   │
      └───────┬───────┘                  └───────┬───────┘
              │ Keystream S₁                     │ Keystream S₂
              ▼                                  ▼
Plaintext P₁ ───⊕───► Ciphertext C₁    Plaintext P₂ ───⊕───► Ciphertext C₂
```

### 3.1 Mathematical Formulation
For block index $j = 1, 2, \dots, n$:
$$S_j = \text{AES}_K(T_j)$$
$$C_j = P_j \oplus S_j \quad (\text{Encryption})$$
$$P_j = C_j \oplus S_j \quad (\text{Decryption})$$

> [!IMPORTANT]
> **No Inverse Cipher Needed for CTR Decryption**  
> Decryption in CTR mode uses the **forward AES cipher** ($\text{AES}_K$) to recompute the identical keystream $S_j$, which is then XORed with the ciphertext $C_j$. The inverse AES transformations (`InvSubBytes`, `InvShiftRows`, `InvMixColumns`) are never invoked.

### 3.2 Counter Increment & Endianness Specification
Per NIST SP 800-38A Section 6.5, the 128-bit counter block $T_j$ consists of a Nonce and a Counter. In standard NIST vectors, the full 128-bit block is incremented as an unsigned **big-endian** integer:
```cpp
void inc128(Block& ctr) noexcept {
    for (int i = 15; i >= 0; --i) {
        if (++ctr[i] != 0) break; // Carry propagation stops on non-overflow
    }
}
```
* **Endianness:** Big-endian (the least-significant byte is at array index 15).
* **Overflow Behavior:** When the counter reaches $2^{128} - 1$, continuing without re-keying causes counter wrap-around to zero, triggering catastrophic keystream reuse. The implementation must detect this condition and terminate.

### 3.3 Handling Arbitrary-Length Plaintexts (Partial Final Block)
CTR mode does **NOT** require PKCS#7 padding. For a final partial block of length $L < 16$ bytes:
$$C_{\text{final}}[0 \dots L-1] = P_{\text{final}}[0 \dots L-1] \oplus S_n[0 \dots L-1]$$
Unused bytes $S_n[L \dots 15]$ of the final keystream block are simply discarded. Ciphertext length equals plaintext length exactly: $|C| = |P|$.

---

## 4. Infrastructure Mapping: Combining `lab_requirement.md`

Lab 2 adopts the identical robust directory layout and automation suite established in `lab01`:

```
lab02/
├── CMakeLists.txt              # Unified build file (MSVC, GCC, Clang)
├── README.md                   # Lab 2 guide, CLI documentation & usage
├── BENCHMARKS.md               # Controlled benchmark methodology & log specs
├── src/
│   ├── AES.hpp / AES.cpp       # Pure C++ FIPS-197 AES core (128/192/256)
│   ├── modesA.hpp / modesA.cpp # CTR stream cipher & counter mechanics
│   ├── modesC.hpp / modesC.cpp # CCM mode implementation
│   ├── modesD.hpp / modesD.cpp # GCM mode implementation
│   ├── minijson.hpp            # Zero-dependency JSON parser for KAT
│   └── main.cpp                # aestool CLI (encrypt, decrypt, bench, --kat)
├── tests/
│   ├── catch.hpp               # Vendored Catch2 v2.13.10 single header
│   ├── test_aes.cpp            # FIPS-197 unit tests (Appendix C vectors)
│   ├── test_ctr.cpp            # NIST SP 800-38A CTR & negative unit tests
│   ├── test_crossval.cpp       # Oracle parity test against Crypto++/OpenSSL
│   ├── cli_test.sh             # Linux CLI integration smoke test
│   └── cli_test.cmake          # Windows CMake CLI smoke test
├── kat/
│   └── vectors.json            # Official NIST/RFC test vectors
├── scripts/
│   ├── build_linux.sh          # Linux GCC/Clang build & test script
│   ├── build_windows.bat       # Windows MSVC/MinGW build & test script
│   ├── run_benchmarks.sh       # Linux benchmark suite (1 MiB, 100 MiB)
│   ├── run_benchmarks.bat      # Windows benchmark runner
│   ├── summarize_bench.py      # Statistical analysis (mean/median/SD/CI95)
│   └── plot_bench.py           # Matplotlib visualization script
└── output/
    ├── ubuntu/test_results/    # KAT, CTest & environment evidence logs
    ├── ubuntu/benchmarks/      # CSV benchmark data & generated graphs
    ├── windows/test_results/   # MSVC KAT, CTest & CLI logs
    └── windows/benchmarks/     # Controlled Windows benchmark CSVs
```

### 4.1 Compiler & Linker Settings across OSes

| Platform | Compiler | Standards & Flags | CRT Linkage | Role in Lab 2 |
|---|---|---|---|---|
| **Ubuntu Linux** | GCC 15.2 (`g++`) | `-std=c++17 -Wall -Wextra -O3` | Dynamic (`glibc`) | Primary benchmark and Linux verification |
| **Ubuntu Linux** | Clang 21.1 (`clang++`) | `-std=c++17 -Wall -Wextra -O3` | Dynamic (`glibc`) | Alternative code generation & CTest sanity |
| **Windows** | MSVC 19.51 (VS 2026/2022) | `/std:c++17 /W4 /O2 /Ob2` | Static CRT (`/MT`) | Official Windows controlled benchmark runner |
| **Windows** | MinGW GCC 16.2 (`g++`) | `-std=c++17 -Wall -Wextra -O3` | Static/MSVCRT | Cross-compiler portability verification |

### 4.2 Cross-Validation Oracle Architecture
In `lab02/CMakeLists.txt`, `test_crossval.cpp` is built conditionally:
* `aestool` remains 100% pure C++ with zero external crypto links.
* `test_crossval` discovers the local Crypto++ install (`libcryptopp-dev` on Linux or `D:\cryptolibrary` on Windows) and executes an automated parity test:
```cpp
// Assert exact identity between pure C++ implementation and Crypto++ oracle
REQUIRE(our_ciphertext == cryptopp_ciphertext);
```

---

## 5. In-Depth Security Engineering Discussion (Mandatory Section)

### 5.1 AES Core Vulnerabilities & Side-Channel Leakage

#### The Timing & Cache-Attack Vulnerability of S-Box Lookups
Standard software implementations store the 256-byte S-box in memory:
```cpp
uint8_t out = SBOX[in];
```
In modern x86/ARM CPUs, memory is cached in 64-byte cache lines. A 256-byte table occupies exactly 4 cache lines ($256 / 64 = 4$).
1. **Cache-Collision Attacks (Bernstein, 2005):** The memory address accessed during round 1 is:
   $$\text{addr} = \text{Base} + (\text{Plaintext}_i \oplus \text{Key}_i)$$
   Different key bytes cause accesses to different cache lines. An attacker observing cache hits vs. cache misses (or total execution time) can deduce whether two lookups hit the same cache line, directly revealing key byte relationships.
2. **Flush+Reload & Prime+Probe:**
   * **Prime+Probe:** The attacker fills (primes) the L1 cache lines with their own data. The victim executes AES. The attacker measures access times to see which cache lines were evicted (probed).
   * **T-Table Leakage:** High-performance implementations combine `SubBytes`, `ShiftRows`, and `MixColumns` into four 1024-byte lookup tables ($T_0, T_1, T_2, T_3 = 4 \text{ KB}$). This spans 64 cache lines, creating massive cache-line access granularity and exposing keys to unprivileged co-located processes.

#### Constant-Time Software Mitigations: Bitslicing
Bitslicing represents the cipher state as 128 individual 1-bit variables across SIMD registers (e.g., SSE/AVX). The S-box is implemented entirely through logical bitwise equations (AND, XOR, OR) without a single table lookup.
* **Boyar-Peralta S-box:** Computes the AES S-box in 32 linear gates and 83 non-linear gates.
* **Advantage:** Execution latency is strictly independent of data and key values, eliminating all cache and timing leakage.

#### Hardware Acceleration: AES-NI
x86 AES-NI instructions (`_mm_aesenc_si128`) execute a full round transformation inside dedicated silicon hardware execution units in fixed cycle time (typically 3–4 cycles). No memory lookup tables are accessed, completely immune to cache-timing attacks.

---

### 5.2 CTR Mode Vulnerabilities & Stream Cipher Mechanics

#### 1. Keystream Reuse & The Two-Time Pad Catastrophe
If two distinct messages $P_1$ and $P_2$ are encrypted using the same key $K$ and same counter $T_1$:
$$C_1 = P_1 \oplus S, \quad C_2 = P_2 \oplus S$$
An eavesdropper XORs the ciphertexts:
$$C_1 \oplus C_2 = (P_1 \oplus S) \oplus (P_2 \oplus S) = P_1 \oplus P_2$$
The keystream $S$ is eliminated entirely. Using statistical language models, space-frequency analysis, or known-plaintext fragments, both plaintexts can be completely recovered.

#### 2. Bit-Flipping Malleability & Lack of Authenticity
CTR mode provides **confidentiality only**; it provides zero integrity or authenticity.
Suppose an attacker intercepts $C = P \oplus S$. The attacker wishes to alter $P$ by adding $\Delta$:
$$C' = C \oplus \Delta$$
During decryption:
$$P' = C' \oplus S = (C \oplus \Delta) \oplus S = (P \oplus S) \oplus \Delta \oplus S = P \oplus \Delta$$
The modification is injected into the plaintext with 100% mathematical certainty, without corrupting adjacent bytes or raising any decryption error.
* **Conclusion:** CTR mode must **always** be coupled with an authentication mechanism (such as HMAC-SHA256 in an Encrypt-then-MAC scheme) or replaced with an AEAD mode (GCM, CCM).

---

## 6. Advanced Topics (+20 Bonus Implementation Plan)

### 6.1 Bonus A: AES-192 & AES-256 Support (+10 Pts)
* **AES-192:** $Nk=6$, $Nr=12$. Round keys total 52 words (208 bytes).
* **AES-256:** $Nk=8$, $Nr=14$. Round keys total 60 words (240 bytes).
  * Requires extra `SubWord` on $i \equiv 4 \pmod 8$:
```cpp
if (nk > 6 && i % nk == 4) {
    temp = sub_word(temp);
}
```
* **Validation:** Verified using NIST FIPS-197 Appendix C.2 and C.3 vectors in `tests/test_aes.cpp`.

### 6.2 Bonus B: AES-XTS Disk Encryption (+5 Pts)
* **Standard:** IEEE P1619. Designed specifically for block-level storage devices (sector encryption).
* **Two-Key Architecture:** Uses two 128-bit keys ($K_1, K_2$).
  * $K_2$ encrypts the sector index / tweak: $T = \text{AES}_{K_2}(\text{SectorNumber})$.
  * For block $j$ within the sector, $T_j = T \cdot \alpha^j$ in $GF(2^{128})$ with reduction polynomial $x^{128} + x^7 + x^2 + x + 1$.
  * Ciphertext: $C_j = \text{AES}_{K_1}(P_j \oplus T_j) \oplus T_j$.
* **Ciphertext Stealing (CTS):** Handled for sectors that are not exact multiples of 16 bytes.

### 6.3 Bonus C: Hardware AES-NI Optimization (+5 Pts)
* **CPUID Detection:** Verify CPU support for AES-NI (CPUID `Function 1`, `ECX Bit 25`).
* **Intrinsics (`<wmmintrin.h>`):**
  * `_mm_aeskeygenassist_si128`: Hardware key expansion assistance.
  * `_mm_aesenc_si128`: Complete single round (SubBytes + ShiftRows + MixColumns + AddRoundKey).
  * `_mm_aesenclast_si128`: Final round (SubBytes + ShiftRows + AddRoundKey).
* **Performance Gain:** Delivers throughput > 4–6 GB/s, representing a 5x–10x speedup over pure software table lookups.

---

## 7. Performance Benchmarking Methodology

### 7.1 Required Measurement Matrix
Per `Lab2.txt` Section 4, the test matrix comprises:
* **Cipher:** AES-128 CTR mode.
* **Payload Sizes:**
  * 1 MiB ($1,048,576$ bytes) — Fits in L3 cache; measures raw algorithm execution.
  * 100 MiB ($104,857,600$ bytes) — Exceeds CPU cache; reflects memory bandwidth limits.
  * 1 GiB ($1,073,741,824$ bytes) — Sustained streaming throughput evaluation.
* **Repetitions:** Minimum 100 runs per size to ensure statistical significance.

### 7.2 Benchmark Command Examples

```bash
# Ubuntu Linux benchmark
./lab02/build/ubuntu-gcc/aestool bench --mode ctr --size 1048576 --runs 100 \
    --out lab02/output/ubuntu/benchmarks/bench_ctr_1MB.csv

./lab02/build/ubuntu-gcc/aestool bench --mode ctr --size 104857600 --runs 10 \
    --out lab02/output/ubuntu/benchmarks/bench_ctr_100MB.csv
```

```bat
rem Windows MSVC benchmark
lab02\build\windows-msvc\Release\aestool.exe bench --mode ctr --size 1048576 --runs 100 ^
    --out lab02\output\windows\benchmarks\bench_msvc_ctr_1MB.csv
```

---

## 8. Master Verification & Action Checklist

Use this checklist to guarantee maximum grade completion:

- [x] **Pure C++ AES-128 Implementation**
  - [x] S-box table lookup & $GF(2^8)$ multiplication (`xtime`, `gmul`).
  - [x] Column-major State conversion (`bytes_to_state`, `state_to_bytes`).
  - [x] Round transformations: `SubBytes`, `ShiftRows`, `MixColumns`, `AddRoundKey`.
  - [x] Key expansion for 128-bit key (10 rounds, 44 words).
- [x] **CTR Mode Implementation (NIST SP 800-38A)**
  - [x] 128-bit big-endian counter increment (`inc128`).
  - [x] Symmetric encryption/decryption using forward cipher only.
  - [x] Partial final block support without PKCS#7 padding.
  - [x] Input validation (fail-closed on 15-byte key or 8-byte IV).
- [x] **Validation & Testing Harness**
  - [x] Known Answer Tests (KATs) for FIPS-197 Appendix C.1/C.2/C.3.
  - [x] NIST SP 800-38A CTR KAT vector.
  - [x] Cross-validation target (`test_crossval`) verifying 100% byte parity with Crypto++.
  - [x] Negative testing (wrong key, wrong IV, tampered ciphertext bit-flipping).
  - [x] Automated CTest test suite integration (3/3 tests passing).
- [x] **Infrastructure & Tooling**
  - [x] CMake build system configured for Ubuntu (GCC/Clang) and Windows (MSVC/MinGW).
  - [x] CLI `aestool` supporting `encrypt`, `decrypt`, `bench`, and `--kat`.
  - [x] Shell and Batch automation scripts (`build_linux.sh`, `build_windows.bat`, etc.).
- [ ] **Report & Evidence Finalization**
  - [ ] Run benchmark scripts on Ubuntu and Windows.
  - [ ] Generate summary statistics (`summarize_bench.py`) and throughput plots (`plot_bench.py`).
  - [ ] Compile theoretical security analysis (cache timing attacks, CTR malleability, two-time pad).
  - [ ] Export final report to `REPORT.pdf`.
