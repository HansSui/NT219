# Length-extension attack on `MAC = SHA256(k || m)`

The demo key is 16 bytes (`0123456789abcdef`), the original message is
`amount=100&to=alice`, the appended suffix is `&to=mallory`, and the original
MAC is:

```
8c916ae440302161253d96254dc9e19ce4944c076b400a84bf5bd83f0cdae780
```

The forged MAC, computed without using the key, is:

```
082e543e7765551ed42a7390f266a5b239f325cb591c8c666b0479ba91add5fe
```

`forged_message.bin` is what the attacker sends as the new message. It excludes
the key and consists of `m || SHA-256 padding || m'`:

```
m              = 19 bytes  "amount=100&to=alice"
padding        = 29 bytes  80 00...00 00 00 00 00 00 00 01 18
m'             = 11 bytes  "&to=mallory"
forged message = 59 bytes
```

## Padding diagram

SHA-256 pads the *unknown* key+message to a 64-byte block:

```
block 0 (64 bytes):
  k (16) | m (19) | 0x80 | 20 zero bytes | 64-bit bit length = 0x118
  <---------------- 35 bytes -----------> <------ 29 bytes ------>
block 1:
  m' (11) | 0x80 | 52 zero bytes | 64-bit bit length for k||forged
```

The attacker starts from the published MAC, which is exactly the chaining value
after block 0. Reconstructing the eight 32-bit state words and continuing the
compression with block 1 produces the forged MAC. `hashtool lenext` performs
this reconstruction; `sha256_ext.cpp` is the small SHA-256 implementation used
for it.

## Reproduce

```bash
cd lab04
./demos/length_extension/run_demo.sh
```

The script computes the original MAC with `hashtool`, forges the extension,
then recomputes SHA-256 over `key || forged_message` to prove the MAC matches.
No key is used by `hashtool lenext`.

## Mitigation

- Do not build a MAC as `H(k || m)`. Merkle–Damgård hashes expose their internal
  state, so an attacker can continue hashing after the padding.
- Use HMAC: `HMAC(k, m) = H((k' xor opad) || H((k' xor ipad) || m))`. The outer
  hash hides the inner state, and the construction is secure for the standard
  assumptions.
- Or use a prefix-free construction / a keyed sponge such as KMAC, where the
  key is absorbed before the message and domain separation prevents extension.
- SHA-256 itself is not broken here; the *naive MAC construction* is.
