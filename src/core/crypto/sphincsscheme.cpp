#include "sphincsscheme.h"
#include <openssl/evp.h>
#include <openssl/err.h>
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
    
    // Perform signing
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
    
    // Create EVP_PKEY from public key data
    EVP_PKEY* evpKey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, 
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
    
    // Initialize verification
    if (EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, evpKey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(evpKey);
        return false;
    }
    
    // Perform verification
    int result = EVP_DigestVerify(mdctx, mSignatureData, kSignatureSize, data, dataSize);
    
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(evpKey);
    
    return result == 1;
}

bool Signature::verify(const PublicKey& publicKey, const string& data) const
{
    return verify(publicKey, reinterpret_cast<const byte_t*>(data.c_str()), data.length());
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
    return memcmp(mSignatureData, other.mSignatureData, kSignatureSize) == 0;
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

bool verifySignature(const PublicKey& publicKey, const Signature& signature, 
                    const byte_t* data, size_t dataSize)
{
    return signature.verify(publicKey, data, dataSize);
}

bool verifySignature(const PublicKey& publicKey, const Signature& signature, const string& data)
{
    return signature.verify(publicKey, data);
}

} // namespace util

} // namespace sphincs
} // namespace crypto