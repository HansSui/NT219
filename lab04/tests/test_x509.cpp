#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "x509_inspect.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>
#include <string>

namespace {

template <typename T, void (*Free)(T*)>
using Ptr = std::unique_ptr<T, decltype(Free)>;

std::string bioString(BIO* bio) {
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio, &data);
    return length > 0 ? std::string(data, static_cast<std::size_t>(length)) : std::string();
}

struct TestCert {
    Ptr<EVP_PKEY, EVP_PKEY_free> key{nullptr, EVP_PKEY_free};
    Ptr<X509, X509_free> cert{nullptr, X509_free};
    std::string pem;
    std::string der;
};

TestCert makeCert(const char* commonName) {
    EVP_PKEY_CTX* keyContext = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    REQUIRE(keyContext != nullptr);
    REQUIRE(EVP_PKEY_keygen_init(keyContext) > 0);
    REQUIRE(EVP_PKEY_CTX_set_rsa_keygen_bits(keyContext, 2048) > 0);
    EVP_PKEY* rawKey = nullptr;
    REQUIRE(EVP_PKEY_keygen(keyContext, &rawKey) > 0);
    EVP_PKEY_CTX_free(keyContext);

    TestCert out;
    out.key.reset(rawKey);
    out.cert.reset(X509_new());
    REQUIRE(out.cert);
    X509_set_version(out.cert.get(), 2);
    ASN1_INTEGER_set(X509_get_serialNumber(out.cert.get()), 1);
    X509_gmtime_adj(X509_getm_notBefore(out.cert.get()), 0);
    X509_gmtime_adj(X509_getm_notAfter(out.cert.get()), 86400);

    // Build the name separately: OpenSSL 4.0 returns a const X509_NAME*.
    X509_NAME* name = X509_NAME_new();
    REQUIRE(name != nullptr);
    REQUIRE(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                       reinterpret_cast<const unsigned char*>(commonName), -1, -1,
                                       0) == 1);
    REQUIRE(X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                                       reinterpret_cast<const unsigned char*>("NT219"), -1, -1,
                                       0) == 1);
    REQUIRE(X509_set_subject_name(out.cert.get(), name) == 1);
    REQUIRE(X509_set_issuer_name(out.cert.get(), name) == 1);
    X509_NAME_free(name);
    REQUIRE(X509_set_pubkey(out.cert.get(), out.key.get()) == 1);

    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, out.cert.get(), out.cert.get(), nullptr, nullptr, 0);
    auto addExtension = [&](int nid, const char* value) {
        X509_EXTENSION* extension =
            X509V3_EXT_conf_nid(nullptr, &context, nid, const_cast<char*>(value));
        REQUIRE(extension != nullptr);
        REQUIRE(X509_add_ext(out.cert.get(), extension, -1) == 1);
        X509_EXTENSION_free(extension);
    };
    addExtension(NID_basic_constraints, "critical,CA:FALSE");
    addExtension(NID_key_usage, "critical,digitalSignature,keyEncipherment");
    addExtension(NID_subject_alt_name, "DNS:lab4.example,DNS:www.lab4.example");

    REQUIRE(X509_sign(out.cert.get(), out.key.get(), EVP_sha256()) > 0);

    BIO* bio = BIO_new(BIO_s_mem());
    REQUIRE(bio);
    REQUIRE(PEM_write_bio_X509(bio, out.cert.get()) == 1);
    out.pem = bioString(bio);
    BIO_free(bio);

    unsigned char* der = nullptr;
    const int derLength = i2d_X509(out.cert.get(), &der);
    REQUIRE(derLength > 0);
    out.der.assign(reinterpret_cast<char*>(der), static_cast<std::size_t>(derLength));
    OPENSSL_free(der);
    return out;
}

std::string publicPem(EVP_PKEY* key) {
    BIO* bio = BIO_new(BIO_s_mem());
    REQUIRE(bio);
    REQUIRE(PEM_write_bio_PUBKEY(bio, key) == 1);
    const std::string out = bioString(bio);
    BIO_free(bio);
    return out;
}

}  // namespace

TEST_CASE("All required X.509 fields are extracted") {
    const TestCert cert = makeCert("lab4.example");
    const x509::Info info = x509::inspect(cert.pem, "", false);
    REQUIRE(info.subject.find("CN=lab4.example") != std::string::npos);
    REQUIRE(info.issuer.find("CN=lab4.example") != std::string::npos);
    REQUIRE(info.spkiAlgorithm.find("rsaEncryption") != std::string::npos);
    REQUIRE(info.spkiParameters.find("modulus_bits=2048") != std::string::npos);
    REQUIRE(info.signatureAlgorithm.find("sha256WithRSAEncryption") != std::string::npos);
    REQUIRE(info.signatureParameters == "NULL");
    REQUIRE_FALSE(info.notBefore.empty());
    REQUIRE_FALSE(info.notAfter.empty());
    REQUIRE(info.keyUsage.find("digitalSignature") != std::string::npos);
    REQUIRE(info.keyUsage.find("keyEncipherment") != std::string::npos);
    REQUIRE(info.sans.find("DNS:lab4.example") != std::string::npos);
    REQUIRE(info.algorithmConsistency == "match");
    REQUIRE(info.signature.find("UNVERIFIED") != std::string::npos);
    REQUIRE(info.trust.find("NOT TRUSTED") != std::string::npos);
}

TEST_CASE("A supplied issuer verifies the certificate") {
    const TestCert cert = makeCert("lab4.example");
    const x509::Info verified = x509::inspect(cert.pem, cert.pem, true);
    REQUIRE(verified.signature == "verified against supplied issuer key");
    REQUIRE(verified.issuerName == "match");
    REQUIRE(x509::inspect(cert.der, cert.der, true).signature ==
            "verified against supplied issuer key");

    const std::string publicKey = publicPem(cert.key.get());
    const x509::Info withKey = x509::inspect(cert.pem, publicKey, true);
    REQUIRE(withKey.signature == "verified against supplied issuer key");
    REQUIRE(withKey.issuerName.find("not checked") != std::string::npos);
}

TEST_CASE("Unverified trust and wrong issuers fail closed") {
    const TestCert cert = makeCert("lab4.example");
    REQUIRE_THROWS(x509::inspect(cert.pem, "", true));
    const TestCert other = makeCert("other.example");
    REQUIRE_THROWS(x509::inspect(cert.pem, other.pem, true));
    REQUIRE_THROWS(x509::inspect(cert.pem, publicPem(other.key.get()), true));
}

TEST_CASE("Tampered and malformed certificates fail") {
    const TestCert cert = makeCert("lab4.example");
    std::string tampered = cert.der;
    tampered[tampered.size() - 5] ^= 0x01;
    REQUIRE_THROWS(x509::inspect(tampered, cert.pem, true));
    REQUIRE_THROWS(x509::inspect("not a certificate", "", false));
    REQUIRE_THROWS(x509::inspect("", "", false));
}
