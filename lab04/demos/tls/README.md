# TLS deployment evidence (Apache/Nginx)

The lab requires an HTTPS deployment on a hostname you own or are authorized to
administer, using TLS 1.2/1.3 and a trusted root. This directory contains the
config snippets and the evidence-collection script. Running it requires the
host and certificate; the repository contains no private key.

## Nginx (recommended)

See `nginx.conf.snippet`. Replace `example.com`, the certificate paths and the
web root. Use an ECDSA leaf certificate (ZeroSSL or another public CA) and its
full chain:

```
ssl_certificate     /etc/ssl/lab4/fullchain.pem;  # leaf + intermediate
ssl_certificate_key /etc/ssl/lab4/privkey.pem;    # never commit
```

Test and reload:

```bash
sudo nginx -t
sudo systemctl reload nginx
```

## Apache

See `apache.conf.snippet`; enable `ssl`, `headers` and `http2`, then:

```bash
sudo apachectl configtest
sudo systemctl reload apache2
```

## Trust chain

A browser trusts a small set of root CAs shipped in its trust store. The server
sends the leaf certificate and usually one intermediate; the root is not sent
because the client already has it. Validation walks:

```
leaf (example.com, ECDSA P-256, SAN) 
  -> intermediate (signed by the CA root)
  -> root (trusted by the OS/browser store)
```

The leaf signature must verify with the intermediate key, the intermediate with
the root key, the names must chain, the validity window must contain the current
time, and the leaf must be valid for the requested hostname via SAN. The
certificate must not be revoked and must satisfy the CA/Browser Forum Baseline
Requirements. Our `hashtool cert --issuer` verifies only the first link; chain
and root trust are deliberately reported as **not evaluated**.

## Evidence to collect

Run on the host:

```bash
cd lab04/demos/tls
./collect_evidence.sh example.com
```

This writes `tls1_2.txt`, `tls1_3.txt`, `http_headers.txt` and
`certificate_chain.pem`. Add a browser screenshot (`screenshot.png`) and the
exact deployed config (`deployed.conf`). `collect_evidence.sh` is read-only; it
does not modify the server.
