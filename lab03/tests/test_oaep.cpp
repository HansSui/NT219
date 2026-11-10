#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "rsa.hpp"
#include "util.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <string>

namespace {

const rsa::Key& testKey() {
    static const rsa::Key key = rsa::generateKey(3072);
    return key;
}

const rsa::Key& secondKey() {
    static const rsa::Key key = rsa::generateKey(3072);
    return key;
}

std::string weakPublicPem() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY* key = nullptr;
    REQUIRE(EVP_PKEY_keygen_init(ctx) > 0);
    REQUIRE(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) > 0);
    REQUIRE(EVP_PKEY_keygen(ctx, &key) > 0);
    BIO* bio = BIO_new(BIO_s_mem());
    REQUIRE(PEM_write_bio_PUBKEY(bio, key) == 1);
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio, &data);
    std::string out(data, static_cast<std::size_t>(length));
    BIO_free(bio);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(ctx);
    return out;
}

}  // namespace

TEST_CASE("RSA-OAEP direct round-trip, with and without a label") {
    const std::string message = "small OAEP payload";
    for (const std::string& label : {std::string(), std::string("context-v1")}) {
        const std::string ciphertext = rsa::oaepEncrypt(testKey(), message, label);
        REQUIRE(ciphertext.size() == static_cast<std::size_t>(testKey().bits() / 8));
        REQUIRE(rsa::oaepDecrypt(testKey(), ciphertext, label) == message);
    }
}

TEST_CASE("OAEP plaintext limit is k - 2*hLen - 2") {
    const std::size_t limit = rsa::maxPlaintext(testKey());
    REQUIRE(limit == 318);
    const std::string maximum(limit, 'm');
    REQUIRE(rsa::oaepDecrypt(testKey(), rsa::oaepEncrypt(testKey(), maximum, ""), "") == maximum);
    REQUIRE_THROWS(rsa::oaepEncrypt(testKey(), std::string(limit + 1, 'm'), ""));
}

TEST_CASE("Manual OAEP interoperates with EVP OAEP in both directions") {
    const std::string message = "manual OAEP payload";
    for (const std::string& label : {std::string(), std::string("context-v1")}) {
        const std::string manual = rsa::oaepEncryptManual(testKey(), message, label);
        REQUIRE(manual.size() == static_cast<std::size_t>(testKey().bits() / 8));
        REQUIRE(rsa::oaepDecrypt(testKey(), manual, label) == message);

        const std::string library = rsa::oaepEncrypt(testKey(), message, label);
        REQUIRE(rsa::oaepDecryptManual(testKey(), library, label) == message);
    }
}

TEST_CASE("Manual OAEP enforces the limit and fails closed") {
    const std::size_t limit = rsa::maxPlaintext(testKey());
    const std::string maximum(limit, 'm');
    REQUIRE(rsa::oaepDecryptManual(
                testKey(), rsa::oaepEncryptManual(testKey(), maximum, ""), "") == maximum);
    REQUIRE_THROWS(rsa::oaepEncryptManual(testKey(), std::string(limit + 1, 'm'), ""));

    const std::string ciphertext = rsa::oaepEncryptManual(testKey(), "secret", "right-label");
    REQUIRE_THROWS(rsa::oaepDecryptManual(testKey(), ciphertext, "wrong-label"));
    REQUIRE_THROWS(rsa::oaepDecryptManual(secondKey(), ciphertext, "right-label"));

    std::string tampered = ciphertext;
    tampered[0] ^= 0x01;
    REQUIRE_THROWS(rsa::oaepDecryptManual(testKey(), tampered, "right-label"));
    REQUIRE_THROWS(rsa::oaepDecryptManual(testKey(), "short", "right-label"));
}

TEST_CASE("Wrong OAEP label and wrong private key fail") {
    const std::string ciphertext = rsa::oaepEncrypt(testKey(), "secret", "right-label");
    REQUIRE_THROWS(rsa::oaepDecrypt(testKey(), ciphertext, "wrong-label"));
    REQUIRE_THROWS(rsa::oaepDecrypt(secondKey(), ciphertext, "right-label"));
}

TEST_CASE("Tampered or malformed RSA ciphertext fails") {
    std::string ciphertext = rsa::oaepEncrypt(testKey(), "secret", "");
    ciphertext[0] ^= 0x01;
    REQUIRE_THROWS(rsa::oaepDecrypt(testKey(), ciphertext, ""));
    REQUIRE_THROWS(rsa::oaepDecrypt(testKey(), "short", ""));
}

TEST_CASE("PEM and DER key encodings round-trip") {
    const std::string publicPem = rsa::publicPem(testKey());
    const std::string privatePem = rsa::privatePem(testKey());
    const std::string publicDer = rsa::publicDer(testKey());
    const std::string privateDer = rsa::privateDer(testKey());

    const rsa::Key fromPublicPem = rsa::loadPublicKey(publicPem);
    const rsa::Key fromPrivatePem = rsa::loadPrivateKey(privatePem);
    const rsa::Key fromPublicDer = rsa::loadPublicKey(publicDer);
    const rsa::Key fromPrivateDer = rsa::loadPrivateKey(privateDer);
    REQUIRE(fromPublicPem.bits() == 3072);
    REQUIRE(fromPrivatePem.bits() == 3072);
    REQUIRE(fromPublicDer.bits() == 3072);
    REQUIRE(fromPrivateDer.bits() == 3072);

    const std::string ciphertext = rsa::oaepEncrypt(fromPublicPem, "formats", "");
    REQUIRE(rsa::oaepDecrypt(fromPrivateDer, ciphertext, "") == "formats");
}

TEST_CASE("Keys below 3072 bits are rejected") {
    REQUIRE_THROWS(rsa::generateKey(2048));
    REQUIRE_THROWS(rsa::loadPublicKey(weakPublicPem()));
}

TEST_CASE("Malformed key encodings are rejected") {
    REQUIRE_THROWS(rsa::loadPublicKey("not a key"));
    REQUIRE_THROWS(rsa::loadPrivateKey("not a key"));
}
