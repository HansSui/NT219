// X.509 field extraction and one-link signature verification.
#pragma once

#include <string>

namespace x509 {

struct Info {
    std::string subject;
    std::string issuer;
    std::string spkiAlgorithm;
    std::string spkiParameters;
    std::string signatureAlgorithm;
    std::string signatureParameters;
    std::string notBefore;
    std::string notAfter;
    std::string keyUsage;
    std::string sans;
    std::string tbsSignatureAlgorithm;
    std::string algorithmConsistency;
    std::string signature;
    std::string issuerName;
    std::string trust;
};

// Throws on malformed certificates, structural inconsistencies, a supplied
// issuer key that does not verify, and any request for verified trust that
// cannot be satisfied.
Info inspect(const std::string& certificate, const std::string& issuer, bool requireVerified);

std::string format(const Info& info);

}  // namespace x509
