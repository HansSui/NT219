# MD5-signed certificate collision (sandbox fallback)

`certA.pem` and `certB.pem` are two MD5/RSA self-signed certificates with the
same 2048-bit test key, the same subject, and **the same MD5 signature value**:

```
Signature Algorithm: md5WithRSAEncryption
MD5(TBS_A) = 775c78b64596396e9a0e02bdc76def4c
MD5(TBS_B) = 775c78b64596396e9a0e02bdc76def4c
signature   = 5b:99:3f:b6:ef:30:cb:b7:da:8d:c6:a6:ee:70:6f:10...
```

The TBS differs only inside the custom extension `1.2.3.4.5`. That extension
contains a 43-byte alignment pad followed by a 128-byte MD5 collision block.
`md5_fastcoll` generated the blocks for the exact certificate TBS prefix, so the
two complete TBS values collide.

A regenerated pair has a new key, collision blocks and therefore a new shared
MD5 value, but the equality property always holds. This is an
**identical-prefix** certificate collision, not a full chosen-prefix
certificate attack. A chosen-prefix demo needs two different identities (two
different TBS prefixes) plus a birthday/path search, roughly 2^39 MD5 work; it
is not practical on this 8-core CPU host in a lab session. The fallback still
shows the catastrophic property: a verifier that checks only the MD5 signature
accepts two different TBS values as equally signed. The +10 chosen-prefix bonus
is therefore not claimed; the core 20-point MD5 collision demo is separate.

## Inspect with `hashtool`

The tool parses the legacy OID and verifies the pair against the shared key
(legacy hashes are accepted here only for inspection of the demo):

```console
$ hashtool cert --in certB.pem --issuer certA.pem --require-verified
...
signature_algorithm: md5WithRSAEncryption (1.2.840.113549.1.1.4)
algorithm_consistency: match
signature: verified against supplied issuer key
issuer_name: match
```

## Reproduce

```bash
cd lab04/demos/chosen_prefix_cert
./run_demo.sh
```

The script downloads the static HashClash release, generates a collision pair
for the certificate prefix, compiles `make_certs.cpp`, and signs both certs with
MD5. It needs OpenSSL 3.x/4.x and allows MD5 signing through the test config.

## Why this matters

MD5 signatures are deterministic PKCS#1 v1.5 signatures over the TBS. If two
TBS values collide under MD5, one signature validates both. An attacker who can
get a CA to sign a certificate whose TBS contains collision blocks can reuse the
signature on a second TBS. Modern CAs ban MD5; the demo remains a sandbox-only
illustration.

## Policy mitigations

- Ban MD5 and SHA-1 in certificate signatures and in all protocol handshakes.
- Enforce the CA/Browser Forum Baseline Requirements and certificate
  transparency so a second certificate with an unexpected identity is visible.
- Use algorithm agility and reject weak signature OIDs before verification in
  production policy. `hashtool` accepts the MD5 OID only so this sandbox
  artifact can be inspected; a real verifier must reject it before calling
  `X509_verify`.
- Never trust a signature that verifies only because a broken hash collided.
