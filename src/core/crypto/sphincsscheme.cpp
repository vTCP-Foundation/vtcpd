#include "sphincsscheme.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace crypto {
namespace sphincs {

// Signature implementation

Signature::Signature() : mIsValid(false)
{
    mSignatureData.fill(0);
}

Signature::Signature(const byte_t* signatureData) : mIsValid(true)
{
    if (signatureData == nullptr) {
        mIsValid = false;
        mSignatureData.fill(0);
        return;
    }
    std::copy(signatureData, signatureData + kSignatureSize, mSignatureData.begin());
}

Signature::Signature(const string& signatureString) : mIsValid(false)
{
    mSignatureData.fill(0);

    if (signatureString.length() != kSignatureSize * 2) {
        return; // Invalid hex string length
    }

    // Validate that all characters are valid hex characters
    for (char c : signatureString) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return; // Invalid hex character
        }
    }

    try {
        for (size_t i = 0; i < kSignatureSize; ++i) {
            string byteString = signatureString.substr(i * 2, 2);
            mSignatureData[i] = static_cast<byte_t>(stoul(byteString, nullptr, 16));
        }
        mIsValid = true;
    } catch (const std::exception&) {
        mSignatureData.fill(0);
        mIsValid = false;
    }
}

Signature::Signature(const Signature& other) : mIsValid(other.mIsValid)
{
    mSignatureData = other.mSignatureData;
}

Signature& Signature::operator=(const Signature& other)
{
    if (this != &other) {
        mSignatureData = other.mSignatureData;
        mIsValid = other.mIsValid;
    }
    return *this;
}

bool Signature::sign(const PrivateKey& privateKey, const byte_t* data, size_t dataSize)
{
    if (!privateKey.isValid() || data == nullptr || dataSize == 0) {
        mIsValid = false;
        return false;
    }

    EVP_PKEY* evpKey = privateKey.getEVPKey();
    if (evpKey == nullptr) {
        mIsValid = false;
        return false;
    }

    // Create signing context (RAII-protected)
    EVPMDContextPtr mdctx(EVP_MD_CTX_new());
    if (!mdctx) {
        mIsValid = false;
        return false;
    }

    // Initialize signing
    if (EVP_DigestSignInit(mdctx.get(), nullptr, nullptr, nullptr, evpKey) <= 0) {
        mIsValid = false;
        return false;
    }

    // REQUIRED: Set deterministic mode must be used by default (by vtcp protocol specification).
    // If provider does not support deterministic mode, then the signature should be rejected.
    // This is a critical security requirement.
    EVP_PKEY_CTX* pctx = EVP_MD_CTX_pkey_ctx(mdctx.get());
    if (pctx == nullptr) {
        mIsValid = false;
        return false;
    }

    int deterministic_flag = 1;
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_int("deterministic", &deterministic_flag);
    params[1] = OSSL_PARAM_construct_end();

    // Deterministic mode is REQUIRED - fail if provider doesn't support it
    if (EVP_PKEY_CTX_set_params(pctx, params) <= 0) {
        mIsValid = false;
        return false;
    }

    // Perform SPHINCS+ signing
    size_t sigLen = kSignatureSize;
    if (EVP_DigestSign(mdctx.get(), mSignatureData.data(), &sigLen, data, dataSize) <= 0) {
        mIsValid = false;
        return false;
    }

    if (sigLen != kSignatureSize) {
        mIsValid = false;
        return false;
    }

    mIsValid = true;
    return true;
}

bool Signature::sign(const PrivateKey& privateKey, const string& data)
{
    return sign(privateKey, reinterpret_cast<const byte_t*>(data.c_str()), data.length());
}

bool Signature::verify(const PublicKey& publicKey, const byte_t* data, size_t dataSize) const
{
    if (!mIsValid || !publicKey.isValid() || data == nullptr || dataSize == 0) {
        return false;
    }

    // Create EVP_PKEY from SPHINCS+ public key data (RAII-protected)
    EVPKeyPtr evpKey(EVP_PKEY_new_raw_public_key_ex(nullptr, getAlgorithmName(), nullptr,
                     publicKey.data(), publicKey.keySize()));
    if (!evpKey) {
        return false;
    }

    // Create verification context (RAII-protected)
    EVPMDContextPtr mdctx(EVP_MD_CTX_new());
    if (!mdctx) {
        return false;
    }

    // Initialize SPHINCS+ verification (no hash function needed as it's built-in)
    if (EVP_DigestVerifyInit(mdctx.get(), nullptr, nullptr, nullptr, evpKey.get()) <= 0) {
        return false;
    }

    // Perform SPHINCS+ verification
    int result = EVP_DigestVerify(mdctx.get(), mSignatureData.data(), kSignatureSize, data, dataSize);

    // Map OpenSSL return codes: 1 = success (valid), 0 = invalid, <0 = error
    if (result == 1) {
        return true;
    }
    if (result == 0) {
        return false;
    }
    return false;
}

const byte_t* Signature::data() const
{
    return mSignatureData.data();
}

string Signature::toString() const
{
    if (!mIsValid) {
        return "";
    }

    stringstream ss;
    ss << hex << setfill('0');
    for (size_t i = 0; i < kSignatureSize; ++i) {
        ss << setw(2) << static_cast<unsigned>(mSignatureData[i]);
    }
    return ss.str();
}

bool Signature::operator==(const Signature& other) const
{
    if (!mIsValid || !other.mIsValid) {
        return false;
    }
    // Use constant-time comparison for signatures to prevent timing attacks that could
    // reveal signature validation success/failure or leak information about signature content
    return CRYPTO_memcmp(mSignatureData.data(), other.mSignatureData.data(), kSignatureSize) == 0;
}

bool Signature::operator!=(const Signature& other) const
{
    return !(*this == other);
}

bool Signature::isValid() const
{
    return mIsValid;
}

void Signature::serialize(byte_t* buffer) const
{
    if (buffer == nullptr) {
        throw std::invalid_argument("Buffer cannot be null");
    }
    std::copy(mSignatureData.begin(), mSignatureData.end(), buffer);
}

void Signature::deserialize(const byte_t* buffer)
{
    if (buffer == nullptr) {
        throw std::invalid_argument("Buffer cannot be null");
    }
    std::copy(buffer, buffer + kSignatureSize, mSignatureData.begin());
    mIsValid = true;
}

void Signature::clear()
{
    mSignatureData.fill(0);
    mIsValid = false;
}

// Utility functions implementation

namespace util {

pair<PrivateKey::Shared, PublicKey::Shared> generateKeyPair()
{
    auto privateKey = make_shared<PrivateKey>();
    auto publicKey = privateKey->derivePublicKey();

    if (!privateKey->isValid() || !publicKey || !publicKey->isValid()) {
        return make_pair(nullptr, nullptr);
    }

    return make_pair(privateKey, publicKey);
}

pair<PrivateKey::Shared, PublicKey::Shared> generateKeyPairFromSeed(const string& seedString)
{
    auto privateKey = make_shared<PrivateKey>(seedString);
    auto publicKey = privateKey->derivePublicKey();

    if (!privateKey->isValid() || !publicKey || !publicKey->isValid()) {
        return make_pair(nullptr, nullptr);
    }

    return make_pair(privateKey, publicKey);
}

Signature::Shared signData(const PrivateKey& privateKey, const byte_t* data, size_t dataSize)
{
    auto signature = make_shared<Signature>();

    if (!signature->sign(privateKey, data, dataSize)) {
        return nullptr;
    }

    return signature;
}

Signature::Shared signData(const PrivateKey& privateKey, const string& data)
{
    return signData(privateKey, reinterpret_cast<const byte_t*>(data.c_str()), data.length());
}

string getOpenSSLError(const string& baseMessage)
{
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return baseMessage;
    }
    
    stringstream ss;
    ss << baseMessage;
    
    while (err != 0) {
        char errorString[256];
        ERR_error_string_n(err, errorString, sizeof(errorString));
        ss << " OpenSSL error: " << errorString;
        err = ERR_get_error();
        if (err != 0) {
            ss << ";";
        }
    }
    
    return ss.str();
}

} // namespace util

} // namespace sphincs
} // namespace crypto
