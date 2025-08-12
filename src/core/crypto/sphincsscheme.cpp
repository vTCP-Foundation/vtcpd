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

namespace crypto {
namespace sphincs {

// Signature implementation

Signature::Signature() : mIsValid(false)
{
    memset(mSignatureData, 0, kSignatureSize);
}

Signature::Signature(const byte_t* signatureData) : mIsValid(true)
{
    if (signatureData == nullptr) {
        mIsValid = false;
        memset(mSignatureData, 0, kSignatureSize);
        return;
    }
    memcpy(mSignatureData, signatureData, kSignatureSize);
}

Signature::Signature(const string& signatureString) : mIsValid(false)
{
    memset(mSignatureData, 0, kSignatureSize);
    
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
        memset(mSignatureData, 0, kSignatureSize);
        mIsValid = false;
    }
}

Signature::Signature(const Signature& other) : mIsValid(other.mIsValid)
{
    memcpy(mSignatureData, other.mSignatureData, kSignatureSize);
}

Signature& Signature::operator=(const Signature& other)
{
    if (this != &other) {
        memcpy(mSignatureData, other.mSignatureData, kSignatureSize);
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
    
    // Derive deterministic seed from SK and data, and seed OpenSSL RNG.
    // This ensures SPHINCS+ optrand becomes deterministic for this call.
    // We set provider optrand explicitly via pkey ctx to ensure deterministic signing
    byte_t optrand[32];
    {
        byte_t sk[PrivateKey::privateKeySize()];
        size_t skLen = sizeof(sk);
        if (EVP_PKEY_get_raw_private_key(evpKey, sk, &skLen) == 1 && skLen == sizeof(sk)) {
            SHA256_CTX shaCtx;
            SHA256_Init(&shaCtx);
            SHA256_Update(&shaCtx, sk, sizeof(sk));
            SHA256_Update(&shaCtx, data, dataSize);
            byte_t digest[SHA256_DIGEST_LENGTH];
            SHA256_Final(digest, &shaCtx);
            memcpy(optrand, digest, sizeof(optrand));
        } else {
            memset(optrand, 0, sizeof(optrand));
        }
    }

    // Create signing context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        mIsValid = false;
        return false;
    }
    
    // Initialize signing
    if (EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, evpKey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        mIsValid = false;
        return false;
    }
    // Set optrand parameter if supported by provider (OpenSSL 3.5+ deterministic SLH-DSA)
    {
        EVP_PKEY_CTX* pctx = EVP_MD_CTX_pkey_ctx(mdctx);
        if (pctx != nullptr) {
            OSSL_PARAM params[2];
            params[0] = OSSL_PARAM_construct_octet_string("optrand", optrand, sizeof(optrand));
            params[1] = OSSL_PARAM_END;
            /* Ignore failure here if provider doesn't support the param */
            EVP_PKEY_CTX_set_params(pctx, params);
        }
    }
    
    // Perform SPHINCS+ signing
    size_t sigLen = kSignatureSize;
    if (EVP_DigestSign(mdctx, mSignatureData, &sigLen, data, dataSize) <= 0) {
        EVP_MD_CTX_free(mdctx);
        mIsValid = false;
        return false;
    }
    
    EVP_MD_CTX_free(mdctx);
    
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
    
    // Create EVP_PKEY from SPHINCS+ public key data
    EVP_PKEY* evpKey = EVP_PKEY_new_raw_public_key_ex(nullptr, getAlgorithmName(), nullptr,
                                                      publicKey.data(), publicKey.keySize());
    if (evpKey == nullptr) {
        return false;
    }
    
    // Create verification context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        EVP_PKEY_free(evpKey);
        return false;
    }
    
    // Initialize SPHINCS+ verification (no hash function needed as it's built-in)
    if (EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, evpKey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(evpKey);
        return false;
    }
    
    // Perform SPHINCS+ verification
    int result = EVP_DigestVerify(mdctx, mSignatureData, kSignatureSize, data, dataSize);
    
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(evpKey);
    
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
    return mSignatureData;
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
    return CRYPTO_memcmp(mSignatureData, other.mSignatureData, kSignatureSize) == 0;
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
    memcpy(buffer, mSignatureData, kSignatureSize);
}

void Signature::deserialize(const byte_t* buffer)
{
    if (buffer == nullptr) {
        throw std::invalid_argument("Buffer cannot be null");
    }
    memcpy(mSignatureData, buffer, kSignatureSize);
    mIsValid = true;
}

void Signature::clear()
{
    memset(mSignatureData, 0, kSignatureSize);
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

} // namespace util

} // namespace sphincs
} // namespace crypto
