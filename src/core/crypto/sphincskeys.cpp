#include "sphincskeys.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace crypto {
namespace sphincs {

// PublicKey implementation

PublicKey::PublicKey() : mIsValid(false)
{
    memset(mKeyData, 0, kKeySize);
}

PublicKey::PublicKey(const byte_t* keyData) : mIsValid(true)
{
    if (keyData == nullptr) {
        mIsValid = false;
        memset(mKeyData, 0, kKeySize);
        return;
    }
    memcpy(mKeyData, keyData, kKeySize);
}

PublicKey::PublicKey(const string& keyString) : mIsValid(false)
{
    memset(mKeyData, 0, kKeySize);
    
    if (keyString.length() != kKeySize * 2) {
        return; // Invalid hex string length
    }
    
    // Validate that all characters are valid hex characters
    for (char c : keyString) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return; // Invalid hex character
        }
    }
    
    try {
        for (size_t i = 0; i < kKeySize; ++i) {
            string byteString = keyString.substr(i * 2, 2);
            mKeyData[i] = static_cast<byte_t>(stoul(byteString, nullptr, 16));
        }
        mIsValid = true;
    } catch (const std::exception&) {
        memset(mKeyData, 0, kKeySize);
        mIsValid = false;
    }
}

PublicKey::PublicKey(const PublicKey& other) : mIsValid(other.mIsValid)
{
    memcpy(mKeyData, other.mKeyData, kKeySize);
}

PublicKey& PublicKey::operator=(const PublicKey& other)
{
    if (this != &other) {
        memcpy(mKeyData, other.mKeyData, kKeySize);
        mIsValid = other.mIsValid;
    }
    return *this;
}

const byte_t* PublicKey::data() const
{
    return mKeyData;
}

string PublicKey::toString() const
{
    if (!mIsValid) {
        return "";
    }
    
    stringstream ss;
    ss << hex << setfill('0');
    for (size_t i = 0; i < kKeySize; ++i) {
        ss << setw(2) << static_cast<unsigned>(mKeyData[i]);
    }
    return ss.str();
}

bool PublicKey::operator==(const PublicKey& other) const
{
    if (!mIsValid || !other.mIsValid) {
        return false;
    }
    return memcmp(mKeyData, other.mKeyData, kKeySize) == 0;
}

bool PublicKey::operator!=(const PublicKey& other) const
{
    return !(*this == other);
}

string PublicKey::hashString() const
{
    if (!mIsValid) {
        return "";
    }
    
    byte_t hash[SHA256_DIGEST_LENGTH];
    SHA256(mKeyData, kKeySize, hash);
    
    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << setw(2) << static_cast<unsigned>(hash[i]);
    }
    return ss.str();
}

bool PublicKey::isValid() const
{
    return mIsValid;
}

void PublicKey::serialize(byte_t* buffer) const
{
    if (buffer == nullptr) {
        throw std::invalid_argument("Buffer cannot be null");
    }
    memcpy(buffer, mKeyData, kKeySize);
}

void PublicKey::deserialize(const byte_t* buffer)
{
    if (buffer == nullptr) {
        throw std::invalid_argument("Buffer cannot be null");
    }
    memcpy(mKeyData, buffer, kKeySize);
    mIsValid = true;
}

// PrivateKey implementation

PrivateKey::PrivateKey() : mKeyData(memory::SecureSegment(kPrivateKeySize)), mEVPKey(nullptr), mIsValid(false)
{
    generateRandom();
}

PrivateKey::PrivateKey(const string& seedString) : mKeyData(memory::SecureSegment(kPrivateKeySize)), mEVPKey(nullptr), mIsValid(false)
{
    generateFromSeed(seedString);
}

PrivateKey::PrivateKey(const byte_t* keyData) : mKeyData(memory::SecureSegment(kPrivateKeySize)), mEVPKey(nullptr), mIsValid(false)
{
    if (keyData != nullptr) {
        initFromData(keyData);
    }
}

PrivateKey::~PrivateKey()
{
    secureWipe();
    if (mEVPKey != nullptr) {
        EVP_PKEY_free(mEVPKey);
        mEVPKey = nullptr;
    }
}

void PrivateKey::generateRandom()
{
    // Create SPHINCS+ key generation context
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_from_name(nullptr, getAlgorithmName(), nullptr);
    if (pctx == nullptr) {
        throw std::runtime_error("Failed to create SPHINCS+ key generation context. Ensure OpenSSL 3.5+ with SPHINCS+ support is available.");
    }
    
    // Initialize key generation
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to initialize SPHINCS+ key generation");
    }
    
    // Generate key pair
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_generate(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to generate SPHINCS+ key pair");
    }
    
    // Extract private key raw data
    auto guard = mKeyData.unlockAndInitGuard();
    byte_t* keyPtr = static_cast<byte_t*>(guard.address());
    size_t keyLen = kPrivateKeySize;
    
    if (EVP_PKEY_get_raw_private_key(pkey, keyPtr, &keyLen) != 1 || keyLen != kPrivateKeySize) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to extract SPHINCS+ private key data");
    }
    
    // Store the EVP key for later use
    mEVPKey = pkey;
    
    EVP_PKEY_CTX_free(pctx);
    mIsValid = true;
}

void PrivateKey::generateFromSeed(const string& seedString)
{
    // For deterministic generation, derive seed material
    byte_t seed[32];
    SHA256(reinterpret_cast<const byte_t*>(seedString.c_str()), seedString.length(), seed);
    
    // Create SPHINCS+ key generation context
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_from_name(nullptr, getAlgorithmName(), nullptr);
    if (pctx == nullptr) {
        throw std::runtime_error("Failed to create SPHINCS+ key generation context. Ensure OpenSSL 3.5+ with SPHINCS+ support is available.");
    }
    
    // Initialize key generation
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to initialize SPHINCS+ key generation");
    }
    
    // For deterministic behavior, seed the RNG
    RAND_seed(seed, sizeof(seed));
    
    // Generate key pair
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_generate(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to generate SPHINCS+ key pair");
    }
    
    // Extract private key raw data
    auto guard = mKeyData.unlockAndInitGuard();
    byte_t* keyPtr = static_cast<byte_t*>(guard.address());
    size_t keyLen = kPrivateKeySize;
    
    if (EVP_PKEY_get_raw_private_key(pkey, keyPtr, &keyLen) != 1 || keyLen != kPrivateKeySize) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to extract SPHINCS+ private key data");
    }
    
    // Store the EVP key for later use
    mEVPKey = pkey;
    
    EVP_PKEY_CTX_free(pctx);
    mIsValid = true;
}

void PrivateKey::initFromData(const byte_t* keyData)
{
    auto guard = mKeyData.unlockAndInitGuard();
    byte_t* keyPtr = static_cast<byte_t*>(guard.address());
    
    memcpy(keyPtr, keyData, kPrivateKeySize);
    
    // Create EVP_PKEY from raw SPHINCS+ key data
    mEVPKey = EVP_PKEY_new_raw_private_key_ex(nullptr, getAlgorithmName(), nullptr, keyData, kPrivateKeySize);
    if (mEVPKey == nullptr) {
        throw std::runtime_error("Failed to create SPHINCS+ EVP_PKEY from raw data. Ensure OpenSSL 3.5+ with SPHINCS+ support is available.");
    }
    
    mIsValid = true;
}

PublicKey::Shared PrivateKey::derivePublicKey() const
{
    if (!mIsValid) {
        return nullptr;
    }
    
    EVP_PKEY* evpKey = getEVPKey();
    if (evpKey == nullptr) {
        return nullptr;
    }
    
    // Extract public key from EVP_PKEY
    size_t pubKeyLen = kKeySize;
    byte_t pubKeyData[kKeySize];
    
    if (EVP_PKEY_get_raw_public_key(evpKey, pubKeyData, &pubKeyLen) != 1) {
        return nullptr;
    }
    
    return make_shared<PublicKey>(pubKeyData);
}

bool PrivateKey::isValid() const
{
    return mIsValid;
}

memory::SecureSegment PrivateKey::serialize() const
{
    if (!mIsValid) {
        throw std::runtime_error("Cannot serialize invalid private key");
    }
    
    memory::SecureSegment result(kPrivateKeySize);
    auto srcGuard = mKeyData.unlockAndInitGuard();
    auto dstGuard = result.unlockAndInitGuard();
    
    memcpy(dstGuard.address(), srcGuard.address(), kPrivateKeySize);
    
    return result;
}

void PrivateKey::deserialize(const memory::SecureSegment& secureData)
{
    if (secureData.size() != kPrivateKeySize) {
        throw std::invalid_argument("Invalid secure data size for SPHINCS+ private key");
    }
    
    auto srcGuard = secureData.unlockAndInitGuard();
    auto dstGuard = mKeyData.unlockAndInitGuard();
    
    memcpy(dstGuard.address(), srcGuard.address(), kPrivateKeySize);
    
    // Clear cached EVP key to force regeneration
    if (mEVPKey != nullptr) {
        EVP_PKEY_free(mEVPKey);
        mEVPKey = nullptr;
    }
    
    // Recreate SPHINCS+ EVP key from raw data
    const byte_t* keyPtr = static_cast<const byte_t*>(srcGuard.address());
    mEVPKey = EVP_PKEY_new_raw_private_key_ex(nullptr, getAlgorithmName(), nullptr, keyPtr, kPrivateKeySize);
    if (mEVPKey == nullptr) {
        throw std::runtime_error("Failed to recreate SPHINCS+ EVP_PKEY from deserialized data. Ensure OpenSSL 3.5+ with SPHINCS+ support is available.");
    }
    
    mIsValid = true;
}

EVP_PKEY* PrivateKey::getEVPKey() const
{
    if (!mIsValid) {
        return nullptr;
    }
    
    if (mEVPKey == nullptr) {
        // Create SPHINCS+ EVP key from raw private key data
        auto guard = mKeyData.unlockAndInitGuard();
        const byte_t* keyPtr = static_cast<const byte_t*>(guard.address());
        
        mEVPKey = EVP_PKEY_new_raw_private_key_ex(nullptr, getAlgorithmName(), nullptr, keyPtr, kPrivateKeySize);
        if (mEVPKey == nullptr) {
            throw std::runtime_error("Failed to create SPHINCS+ EVP_PKEY from private key data. Ensure OpenSSL 3.5+ with SPHINCS+ support is available.");
        }
    }
    
    return mEVPKey;
}

void PrivateKey::secureWipe()
{
    if (mKeyData.size() > 0) {
        auto guard = mKeyData.unlockAndInitGuard();
        OPENSSL_cleanse(guard.address(), mKeyData.size());
    }
    mIsValid = false;
}

} // namespace sphincs
} // namespace crypto