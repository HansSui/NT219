#include "x509_inspect.hpp"

#include "util.hpp"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace x509 {
namespace {

template <typename T, void (*Free)(T*)>
using Ptr = std::unique_ptr<T, decltype(Free)>;

std::string bioString(BIO* bio) {
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio, &data);
    return length > 0 ? std::string(data, static_cast<std::size_t>(length)) : std::string();
}

std::string objectText(const ASN1_OBJECT* object) {
    char oid[128];
    OBJ_obj2txt(oid, sizeof oid, object, 1);
    const char* name = OBJ_nid2ln(OBJ_obj2nid(object));
    return std::string(name ? name : "unknown") + " (" + oid + ")";
}

std::string nameText(const X509_NAME* name) {
    if (!name) return "(none)";
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) throw std::runtime_error("out of memory");
    X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);
    const std::string out = bioString(bio);
    BIO_free(bio);
    return out.empty() ? "(empty)" : out;
}

std::string timeText(const ASN1_TIME* time) {
    if (!time) return "(none)";
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) throw std::runtime_error("out of memory");
    ASN1_TIME_print(bio, time);
    const std::string out = bioString(bio);
    BIO_free(bio);
    return out;
}

Ptr<X509, X509_free> parseCertificate(const std::string& data) {
    if (data.empty()) throw std::runtime_error("malformed certificate: empty input");
    BIO* bio = BIO_new_mem_buf(data.data(), static_cast<int>(data.size()));
    if (!bio) throw std::runtime_error("out of memory");
    X509* pem = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (pem) return Ptr<X509, X509_free>(pem, X509_free);

    ERR_clear_error();
    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(data.data());
    const unsigned char* end = cursor + data.size();
    X509* der = d2i_X509(nullptr, &cursor, static_cast<long>(data.size()));
    if (!der || cursor != end) {
        X509_free(der);
        throw std::runtime_error("malformed certificate: not valid PEM or DER");
    }
    return Ptr<X509, X509_free>(der, X509_free);
}

struct Issuer {
    Ptr<X509, X509_free> certificate{nullptr, X509_free};
    Ptr<EVP_PKEY, EVP_PKEY_free> key{nullptr, EVP_PKEY_free};
    bool fromCertificate = false;
};

Issuer parseIssuer(const std::string& data) {
    if (data.empty()) return {};
    BIO* bio = BIO_new_mem_buf(data.data(), static_cast<int>(data.size()));
    if (!bio) throw std::runtime_error("out of memory");
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (cert) {
        Issuer issuer;
        issuer.certificate.reset(cert);
        issuer.key.reset(X509_get_pubkey(cert));
        issuer.fromCertificate = true;
        if (!issuer.key) throw std::runtime_error("issuer certificate has no public key");
        return issuer;
    }

    ERR_clear_error();
    bio = BIO_new_mem_buf(data.data(), static_cast<int>(data.size()));
    if (!bio) throw std::runtime_error("out of memory");
    EVP_PKEY* pemKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (pemKey) {
        Issuer issuer;
        issuer.key.reset(pemKey);
        return issuer;
    }

    ERR_clear_error();
    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(data.data());
    const unsigned char* end = cursor + data.size();
    X509* derCert = d2i_X509(nullptr, &cursor, static_cast<long>(data.size()));
    if (derCert && cursor == end) {
        Issuer issuer;
        issuer.certificate.reset(derCert);
        issuer.key.reset(X509_get_pubkey(derCert));
        issuer.fromCertificate = true;
        if (!issuer.key) throw std::runtime_error("issuer certificate has no public key");
        return issuer;
    }
    X509_free(derCert);

    cursor = reinterpret_cast<const unsigned char*>(data.data());
    end = cursor + data.size();
    EVP_PKEY* derKey = d2i_PUBKEY(nullptr, &cursor, static_cast<long>(data.size()));
    if (derKey && cursor == end) {
        Issuer issuer;
        issuer.key.reset(derKey);
        return issuer;
    }
    EVP_PKEY_free(derKey);
    throw std::runtime_error("malformed issuer key: not valid PEM or DER");
}

std::string keyParameters(EVP_PKEY* key) {
    const int type = EVP_PKEY_base_id(key);
    if (type == EVP_PKEY_RSA || type == EVP_PKEY_RSA_PSS) {
        BIGNUM* n = nullptr;
        BIGNUM* e = nullptr;
        EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_N, &n);
        EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_E, &e);
        std::ostringstream out;
        out << "modulus_bits=" << (n ? BN_num_bits(n) : EVP_PKEY_get_bits(key));
        if (e) {
            char* exponent = BN_bn2dec(e);
            out << ", exponent=" << (exponent ? exponent : "?");
            OPENSSL_free(exponent);
        }
        BN_free(n);
        BN_free(e);
        return out.str();
    }
    if (type == EVP_PKEY_EC) {
        char group[128] = {0};
        std::size_t length = 0;
        if (EVP_PKEY_get_utf8_string_param(key, OSSL_PKEY_PARAM_GROUP_NAME, group,
                                           sizeof group, &length) == 1)
            return std::string("curve=") + group;
        return "curve=(unknown)";
    }
    if (type == EVP_PKEY_ED25519 || type == EVP_PKEY_ED448) return "none";
    return "none";
}

std::string keyUsageText(X509* cert) {
    ASN1_BIT_STRING* usage =
        static_cast<ASN1_BIT_STRING*>(X509_get_ext_d2i(cert, NID_key_usage, nullptr, nullptr));
    if (!usage) return "(none)";
    static const char* names[] = {
        "digitalSignature", "nonRepudiation", "keyEncipherment", "dataEncipherment",
        "keyAgreement",      "keyCertSign",    "cRLSign",         "encipherOnly",
        "decipherOnly",
    };
    std::string out;
    for (int i = 0; i < 9; ++i) {
        if (ASN1_BIT_STRING_get_bit(usage, i)) {
            if (!out.empty()) out += ", ";
            out += names[i];
        }
    }
    ASN1_BIT_STRING_free(usage);
    return out.empty() ? "(none)" : out;
}

std::string sanText(X509* cert) {
    GENERAL_NAMES* names = static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
    if (!names) return "(none)";
    std::string out;
    for (int i = 0; i < sk_GENERAL_NAME_num(names); ++i) {
        BIO* bio = BIO_new(BIO_s_mem());
        if (!bio) {
            GENERAL_NAMES_free(names);
            throw std::runtime_error("out of memory");
        }
        GENERAL_NAME_print(bio, sk_GENERAL_NAME_value(names, i));
        const std::string item = bioString(bio);
        BIO_free(bio);
        if (!out.empty()) out += ", ";
        out += item;
    }
    GENERAL_NAMES_free(names);
    return out.empty() ? "(none)" : out;
}

std::string signatureParameters(const X509_ALGOR* algorithm) {
    const ASN1_OBJECT* object = nullptr;
    int type = 0;
    const void* value = nullptr;
    X509_ALGOR_get0(&object, &type, &value, algorithm);
    (void)object;
    (void)value;
    if (type == V_ASN1_UNDEF) return "none";
    if (type == V_ASN1_NULL) return "NULL";
    return "present (ASN.1 type " + std::to_string(type) + ")";
}

const char* keyTypeName(int type) {
    switch (type) {
        case EVP_PKEY_RSA: return "RSA";
        case EVP_PKEY_RSA_PSS: return "RSA-PSS";
        case EVP_PKEY_EC: return "EC";
        case EVP_PKEY_ED25519: return "Ed25519";
        case EVP_PKEY_ED448: return "Ed448";
        case EVP_PKEY_DSA: return "DSA";
        default: return "unknown";
    }
}

int expectedKeyType(const ASN1_OBJECT* signatureAlgorithm) {
    switch (OBJ_obj2nid(signatureAlgorithm)) {
        // Legacy MD5 certs are parsed for inspection; the OID is known, so the
        // key-type check stays meaningful. Verification policy is separate.
        case NID_md5WithRSAEncryption:
        case NID_sha1WithRSAEncryption:
        case NID_sha224WithRSAEncryption:
        case NID_sha256WithRSAEncryption:
        case NID_sha384WithRSAEncryption:
        case NID_sha512WithRSAEncryption:
        case NID_rsassaPss:
            return EVP_PKEY_RSA;
        case NID_ecdsa_with_SHA1:
        case NID_ecdsa_with_SHA224:
        case NID_ecdsa_with_SHA256:
        case NID_ecdsa_with_SHA384:
        case NID_ecdsa_with_SHA512:
            return EVP_PKEY_EC;
        case NID_ED25519: return EVP_PKEY_ED25519;
        case NID_ED448: return EVP_PKEY_ED448;
        case NID_dsa_with_SHA256: return EVP_PKEY_DSA;
        default: return NID_undef;
    }
}

}  // namespace

Info inspect(const std::string& certificate, const std::string& issuerData,
             bool requireVerified) {
    Ptr<X509, X509_free> cert = parseCertificate(certificate);
    const X509_ALGOR* outer = nullptr;
    X509_get0_signature(nullptr, &outer, cert.get());
    const X509_ALGOR* inner = X509_get0_tbs_sigalg(cert.get());
    if (!outer || !inner) throw std::runtime_error("certificate has no signature algorithm");

    const ASN1_OBJECT* outerObject = nullptr;
    const ASN1_OBJECT* innerObject = nullptr;
    int type = 0;
    const void* value = nullptr;
    X509_ALGOR_get0(&outerObject, &type, &value, outer);
    (void)type;
    (void)value;
    X509_ALGOR_get0(&innerObject, &type, &value, inner);

    if (X509_ALGOR_cmp(outer, inner) != 0)
        throw std::runtime_error("certificate TBS and outer signature algorithms differ");

    Ptr<EVP_PKEY, EVP_PKEY_free> publicKey(X509_get_pubkey(cert.get()), EVP_PKEY_free);
    if (!publicKey) throw std::runtime_error("certificate has no public key");
    const int keyType = EVP_PKEY_base_id(publicKey.get());
    const int wantedType = expectedKeyType(outerObject);

    Info info;
    info.subject = nameText(X509_get_subject_name(cert.get()));
    info.issuer = nameText(X509_get_issuer_name(cert.get()));
    // The SPKI algorithm OID is authoritative; use the subject key info, not the signature.
    ASN1_OBJECT* spkiObject = nullptr;
    X509_ALGOR* spkiAlgorithmStruct = nullptr;
    const unsigned char* keyBits = nullptr;
    int keyBitsLength = 0;
    X509_PUBKEY_get0_param(&spkiObject, &keyBits, &keyBitsLength, &spkiAlgorithmStruct,
                           X509_get_X509_PUBKEY(cert.get()));
    (void)keyBits;
    (void)keyBitsLength;
    (void)spkiAlgorithmStruct;
    info.spkiAlgorithm = std::string(keyTypeName(keyType)) + " (" +
                         (spkiObject ? objectText(spkiObject) : "unknown") + ")";
    info.spkiParameters = keyParameters(publicKey.get());
    info.signatureAlgorithm = objectText(outerObject);
    info.signatureParameters = signatureParameters(outer);
    info.notBefore = timeText(X509_get0_notBefore(cert.get()));
    info.notAfter = timeText(X509_get0_notAfter(cert.get()));
    info.keyUsage = keyUsageText(cert.get());
    info.sans = sanText(cert.get());
    info.tbsSignatureAlgorithm = objectText(innerObject);

    if (wantedType == NID_undef) {
        throw std::runtime_error("unsupported certificate signature algorithm");
    } else if (wantedType == keyType ||
               (wantedType == EVP_PKEY_RSA && keyType == EVP_PKEY_RSA_PSS)) {
        info.algorithmConsistency = "match";
    } else {
        info.algorithmConsistency = "mismatch";
        throw std::runtime_error("signature algorithm does not match public key type");
    }

    if (!issuerData.empty()) {
        Issuer issuer = parseIssuer(issuerData);
        if (issuer.fromCertificate) {
            const bool nameMatch =
                X509_NAME_cmp(X509_get_issuer_name(cert.get()),
                              X509_get_subject_name(issuer.certificate.get())) == 0;
            info.issuerName = nameMatch ? "match" : "mismatch";
            if (!nameMatch) throw std::runtime_error("issuer name does not match issuer certificate");
        } else {
            info.issuerName = "not checked (public key supplied)";
        }
        // shortcut: one-link signature check only, no chain/root/CRL/OCSP;
        // upgrade to X509_STORE_CTX when full path validation is required.
        if (X509_verify(cert.get(), issuer.key.get()) != 1)
            throw std::runtime_error("certificate signature is invalid");
        info.signature = "verified against supplied issuer key";
        info.trust = "signature verified; chain/root trust NOT evaluated";
    } else {
        if (requireVerified)
            throw std::runtime_error("verified trust required but no issuer key was supplied");
        info.signature = "UNVERIFIED (issuer key not provided)";
        info.issuerName = "not checked";
        info.trust = "NOT TRUSTED (signature unverified)";
    }
    return info;
}

std::string format(const Info& info) {
    std::ostringstream out;
    out << "subject: " << info.subject << '\n'
        << "issuer: " << info.issuer << '\n'
        << "spki_algorithm: " << info.spkiAlgorithm << '\n'
        << "spki_parameters: " << info.spkiParameters << '\n'
        << "signature_algorithm: " << info.signatureAlgorithm << '\n'
        << "signature_parameters: " << info.signatureParameters << '\n'
        << "not_before: " << info.notBefore << '\n'
        << "not_after: " << info.notAfter << '\n'
        << "key_usage: " << info.keyUsage << '\n'
        << "san: " << info.sans << '\n'
        << "tbs_signature_algorithm: " << info.tbsSignatureAlgorithm << '\n'
        << "algorithm_consistency: " << info.algorithmConsistency << '\n'
        << "signature: " << info.signature << '\n'
        << "issuer_name: " << info.issuerName << '\n'
        << "trust: " << info.trust << '\n';
    return out.str();
}

}  // namespace x509
