#include "sphincskeys.h"
#include "sphincsscheme.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace crypto {
namespace sphincs {

// PublicKey implementation

PublicKey::PublicKey() : mIsValid(false)
{
    mKeyData.fill(0);
}

PublicKey::PublicKey(const byte_t* keyData) : mIsValid(true)
{
    if (keyData == nullptr) {
        mIsValid = false;
        mKeyData.fill(0);
        return;
    }
    std::copy(keyData, keyData + kKeySize, mKeyData.begin());
}

PublicKey::PublicKey(const string& keyString) : mIsValid(false)
{
    mKeyData.fill(0);
    
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
        mKeyData.fill(0);
        mIsValid = false;
    }
}

PublicKey::PublicKey(const PublicKey& other) : mIsValid(other.mIsValid)
{
    mKeyData = other.mKeyData;
}

PublicKey& PublicKey::operator=(const PublicKey& other)
{
    if (this != &other) {
        mKeyData = other.mKeyData;
        mIsValid = other.mIsValid;
    }
    return *this;
}

const byte_t* PublicKey::data() const
{
    return mKeyData.data();
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
    // Use constant-time comparison to prevent timing attacks in key lookup/authentication scenarios
    // where timing could reveal which keys are being compared or authentication success/failure
    return CRYPTO_memcmp(mKeyData.data(), other.mKeyData.data(), kKeySize) == 0;
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
    SHA256(mKeyData.data(), kKeySize, hash);
    
    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << setw(2) << static_cast<unsigned>(hash[i]);
    }
    return ss.str();
}

const KeyHash::Shared PublicKey::hash() const
{
    if (!mIsValid) {
        return nullptr;
    }
    
    byte_t hashBuffer[KeyHash::kBytesSize];
    SHA256(mKeyData.data(), kKeySize, hashBuffer);
    
    return make_shared<KeyHash>(hashBuffer);
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
    std::copy(mKeyData.begin(), mKeyData.end(), buffer);
}

void PublicKey::deserialize(const byte_t* buffer)
{
    if (buffer == nullptr) {
        throw std::invalid_argument("Buffer cannot be null");
    }
    std::copy(buffer, buffer + kKeySize, mKeyData.begin());
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
    // Create SPHINCS+ key generation context (RAII-protected)
    EVPKeyContextPtr pctx(EVP_PKEY_CTX_new_from_name(nullptr, getAlgorithmName(), nullptr));
    if (!pctx) {
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to create SPHINCS+ key generation context. Ensure OpenSSL 3.5+ with SPHINCS+ support is available."));
    }
    
    // Initialize key generation
    if (EVP_PKEY_keygen_init(pctx.get()) <= 0) {
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to initialize SPHINCS+ key generation"));
    }
    
    // Generate key pair (RAII-protected)
    EVP_PKEY* raw_pkey = nullptr;
    if (EVP_PKEY_generate(pctx.get(), &raw_pkey) <= 0) {
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to generate SPHINCS+ key pair"));
    }
    EVPKeyPtr pkey(raw_pkey);  // Wrap in RAII immediately
    
    // Extract private key raw data - if this throws, pkey is automatically cleaned up
    auto guard = mKeyData.unlockAndInitGuard();
    byte_t* keyPtr = static_cast<byte_t*>(guard.address());
    size_t keyLen = kPrivateKeySize;
    
    if (EVP_PKEY_get_raw_private_key(pkey.get(), keyPtr, &keyLen) != 1 || keyLen != kPrivateKeySize) {
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to extract SPHINCS+ private key data"));
    }
    
    // Store the EVP key for later use (transfer ownership)
    mEVPKey = pkey.release();
    
    mIsValid = true;
}

void PrivateKey::generateFromSeed(const string& seedString)
{
    // Deterministically expand seedString into 128 bytes using SHA-256(counter || seed)
    // This avoids reliance on global RNG and guarantees reproducibility
    byte_t expanded[kPrivateKeySize];
    size_t produced = 0;
    uint32_t counter = 0;
    while (produced < kPrivateKeySize) {
        // Prepare input: counter (big endian) || seed bytes
        uint8_t inputPrefix[4];
        inputPrefix[0] = static_cast<uint8_t>((counter >> 24) & 0xFF);
        inputPrefix[1] = static_cast<uint8_t>((counter >> 16) & 0xFF);
        inputPrefix[2] = static_cast<uint8_t>((counter >> 8) & 0xFF);
        inputPrefix[3] = static_cast<uint8_t>(counter & 0xFF);

        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        // Domain separation constant prevents collisions between counter sequences and seed strings.
        // Without this, a seed string that equals a binary counter could collide with different inputs.
        SHA256_Update(&ctx, "VTCPD-SPHINCS-SEED", strlen("VTCPD-SPHINCS-SEED"));
        SHA256_Update(&ctx, inputPrefix, sizeof(inputPrefix));
        SHA256_Update(&ctx, reinterpret_cast<const byte_t*>(seedString.c_str()), seedString.length());
        byte_t digest[SHA256_DIGEST_LENGTH];
        SHA256_Final(digest, &ctx);

        size_t toCopy = std::min(static_cast<size_t>(SHA256_DIGEST_LENGTH), kPrivateKeySize - produced);
        memcpy(expanded + produced, digest, toCopy);
        produced += toCopy;
        counter++;
    }

    // Write into secure memory
    {
        auto guard = mKeyData.unlockAndInitGuard();
        memcpy(guard.address(), expanded, kPrivateKeySize);
    }

    // Create EVP_PKEY from deterministic raw private key bytes
    mEVPKey = EVP_PKEY_new_raw_private_key_ex(nullptr, getAlgorithmName(), nullptr, expanded, kPrivateKeySize);
    if (mEVPKey == nullptr) {
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to create SPHINCS+ EVP_PKEY from deterministic raw key. Ensure OpenSSL 3.5+ with SPHINCS+ support is available."));
    }

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
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to create SPHINCS+ EVP_PKEY from raw data. Ensure OpenSSL 3.5+ with SPHINCS+ support is available."));
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
        throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to recreate SPHINCS+ EVP_PKEY from deserialized data. Ensure OpenSSL 3.5+ with SPHINCS+ support is available."));
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
            throw std::runtime_error(sphincs::util::getOpenSSLError("Failed to create SPHINCS+ EVP_PKEY from private key data. Ensure OpenSSL 3.5+ with SPHINCS+ support is available."));
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

// KeyHash implementation

KeyHash::KeyHash(byte_t* buffer)
{
    std::copy(buffer, buffer + kBytesSize, mData.begin());
}

const byte_t* KeyHash::data() const
{
    return mData.data();
}

const string KeyHash::toString() const
{
    stringstream ss;
    ss << hex << setfill('0');
    for (size_t i = 0; i < kBytesSize; ++i) {
        ss << setw(2) << static_cast<unsigned>(mData[i]);
    }
    return ss.str();
}

bool operator==(const KeyHash &kh1, const KeyHash &kh2)
{
    // Use constant-time comparison for key hashes used in database lookups and key management
    // to prevent timing-based key enumeration attacks
    return CRYPTO_memcmp(kh1.mData.data(), kh2.mData.data(), KeyHash::kBytesSize) == 0;
}

bool operator!=(const KeyHash &kh1, const KeyHash &kh2)
{
    return !(kh1 == kh2);
}

} // namespace sphincs
} // namespace crypto