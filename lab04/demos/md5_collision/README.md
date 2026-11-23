# MD5 collision demonstration (Option A: two benign PNGs)

Both files are valid 64×64 RGBA PNGs with identical pixels. They differ only in
128 trailing bytes after the PNG `IEND` chunk, so decoders display the same
image. Their MD5 digests are identical:

```
c1a8659eeb61d1d1258f1a6e1e1b55e1  png1.png
c1a8659eeb61d1d1258f1a6e1e1b55e1  png2.png
```

The collision blocks are freshly generated each run, so a regenerated pair has
the same two digests as each other but a different shared value.

Reproduce with the static HashClash release (no build toolchain needed):

```bash
curl -L -o hashclash.tar.gz \
  https://github.com/cr-marcstevens/hashclash/releases/download/hashclash-static-release-v1.2b/hashclash-static-release-v1.2b.tar.gz
tar xzf hashclash.tar.gz
./hashclash-static-release-v1.2b/bin/md5_fastcoll -q -p prefix.png -o png1.png png2.png
md5sum png1.png png2.png
```

`prefix.png` is a valid PNG padded to a 64-byte boundary; `md5_fastcoll` appends
two colliding 64-byte blocks to each output. The collision blocks are ignored by
PNG parsers because they follow `IEND`. `run_demo.sh` regenerates the pair and
checks the chunks and digests.

## Why MD5 is broken

MD5 is a Merkle–Damgård hash with a 128-bit state. Wang et al. (2004) found
message-difference paths that produce collisions in about 2^39 MD5 compressions;
`md5_fastcoll` implements this practical identical-prefix attack. MD5 is not
merely weak against preimages: collisions can be constructed without any search
over the digest value.

- **Collision resistance:** find any two distinct messages with the same digest.
  MD5 has none. SHA-1 also has practical collisions (SHAttered, 2017).
- **Preimage resistance:** find a message with a given digest. MD5 is still hard
  for full preimages, but collision breaks are enough to forge certificates,
  signatures, and file-integrity claims.

## Historical incidents

- 2004: Wang et al. announce practical MD5 collisions.
- 2008: rogue CA certificate using an MD5 collision (Sotirov et al.).
- 2012: Flame malware used an MD5 chosen-prefix collision to sign components.
- 2017: SHAttered demonstrates SHA-1 collisions; browsers and CAs had already
  begun banning SHA-1.
- CAs and modern protocols now ban MD5 and SHA-1 for signatures.

## Scope

These files are test artifacts generated offline from an image we created. No
third-party data or live system is targeted. The demo shows why `hashtool`
refuses MD5 and SHA-1 algorithm identifiers.
