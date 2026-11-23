// Build two MD5/RSA self-signed certificates whose TBS differs only inside a
// custom extension that carries a fastcoll MD5 collision pair.
// shortcut: identical TBS prefix, not a chosen-prefix attack; upgrade with
// HashClash cpc.sh (about 2^39 MD5 work) for different identities.
#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot read " + path);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

EVP_PKEY* generateKey() {
    EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!context) throw std::runtime_error("key context failed");
    if (EVP_PKEY_keygen_init(context) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(context, 2048) <= 0)
        throw std::runtime_error("key setup failed");
    EVP_PKEY* key = nullptr;
    if (EVP_PKEY_keygen(context, &key) <= 0) throw std::runtime_error("key generation failed");
    EVP_PKEY_CTX_free(context);
    return key;
}

X509* build(EVP_PKEY* key, const std::string& value, bool& collision, std::string& tbs) {
    X509* cert = X509_new();
    if (!cert) throw std::runtime_error("certificate allocation failed");
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    ASN1_TIME_set_string(X509_getm_notBefore(cert), "20260101000000Z");
    ASN1_TIME_set_string(X509_getm_notAfter(cert), "20270101000000Z");
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("MD5 collision demo"),
                               -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("NT219"), -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509_set_pubkey(cert, key);

    ASN1_OBJECT* oid = OBJ_txt2obj("1.2.3.4.5", 1);
    ASN1_OCTET_STRING* data = ASN1_OCTET_STRING_new();
    ASN1_OCTET_STRING_set(data, reinterpret_cast<const unsigned char*>(value.data()),
                          static_cast<int>(value.size()));
    X509_EXTENSION* extension = X509_EXTENSION_create_by_OBJ(nullptr, oid, 0, data);
    if (!extension || X509_add_ext(cert, extension, -1) != 1)
        throw std::runtime_error("extension failed");
    X509_EXTENSION_free(extension);
    ASN1_OCTET_STRING_free(data);
    ASN1_OBJECT_free(oid);

    if (X509_sign(cert, key, EVP_md5()) <= 0) {
        X509_free(cert);
        throw std::runtime_error("MD5 signing failed; ensure the security level allows MD5");
    }
    unsigned char* encoded = nullptr;
    const int length = i2d_re_X509_tbs(cert, &encoded);
    if (length <= 0) throw std::runtime_error("TBS encoding failed");
    tbs.assign(reinterpret_cast<char*>(encoded), static_cast<std::size_t>(length));
    OPENSSL_free(encoded);
    collision = true;
    return cert;
}

std::string md5(const std::string& data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_Digest(data.data(), data.size(), digest, &length, EVP_md5(), nullptr);
    std::string out;
    static const char* hex = "0123456789abcdef";
    for (unsigned int i = 0; i < length; ++i) {
        out.push_back(hex[digest[i] >> 4]);
        out.push_back(hex[digest[i] & 15]);
    }
    return out;
}

void writePem(X509* cert, const std::string& path) {
    BIO* bio = BIO_new_file(path.c_str(), "w");
    if (!bio || PEM_write_bio_X509(bio, cert) != 1)
        throw std::runtime_error("cannot write " + path);
    BIO_free(bio);
}

}  // namespace

int main() {
    try {
        EVP_PKEY* key = generateKey();
        X509* first = nullptr;
        X509* second = nullptr;
        const std::string dummy(128, 'D');
        const char* hashclash = std::getenv("HASHCLASH");
        if (!hashclash) hashclash = "md5_fastcoll";
        for (int pad = 0; pad < 64; ++pad) {
            const std::string prefix(static_cast<std::size_t>(pad), '\0');
            bool unused = false;
            std::string templateTbs;
            X509* templateCert = build(key, prefix + dummy, unused, templateTbs);
            const std::size_t offset = templateTbs.find(dummy);
            X509_free(templateCert);
            if (offset == std::string::npos || offset % 64 != 0) continue;

            {
                std::ofstream prefixFile("prefix.bin", std::ios::binary);
                prefixFile.write(templateTbs.data(), static_cast<std::streamsize>(offset));
            }
            const std::string command = std::string("\"") + hashclash +
                "\" -q -p prefix.bin -o collision1.bin collision2.bin >/dev/null 2>&1";
            if (std::system(command.c_str()) != 0) continue;

            const std::string out1 = readFile("collision1.bin");
            const std::string out2 = readFile("collision2.bin");
            if (out1.size() != offset + 128 || out2.size() != offset + 128) continue;
            const std::string block1 = out1.substr(offset);
            const std::string block2 = out2.substr(offset);

            std::string tbs1;
            std::string tbs2;
            X509* a = build(key, prefix + block1, unused, tbs1);
            X509* b = build(key, prefix + block2, unused, tbs2);
            if (md5(tbs1) == md5(tbs2)) {
                first = a;
                second = b;
                std::printf("aligned with %d pad bytes at TBS offset %zu; MD5(TBS)=%s\n",
                            pad, offset, md5(tbs1).c_str());
                break;
            }
            X509_free(a);
            X509_free(b);
        }
        if (!first) throw std::runtime_error("no aligned collision found");

        writePem(first, "certA.pem");
        writePem(second, "certB.pem");
        BIO* keyBio = BIO_new_file("demo_key.pem", "w");
        PEM_write_bio_PrivateKey(keyBio, key, nullptr, nullptr, 0, nullptr, nullptr);
        BIO_free(keyBio);
        std::printf("wrote certA.pem certB.pem demo_key.pem\n");
        X509_free(first);
        X509_free(second);
        EVP_PKEY_free(key);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
