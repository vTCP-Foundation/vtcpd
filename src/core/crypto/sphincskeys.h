#ifndef VTCPD_SPHINCSKEYS_H
#define VTCPD_SPHINCSKEYS_H

#include "memory.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <memory>
#include <string>
#include <cstring>

namespace crypto {
namespace sphincs {

using namespace std;

/**
 * @brief Hash of a SPHINCS+ public key for identification and storage
 * 
 * This class provides a consistent way to hash and identify SPHINCS+ public keys.
 * It generates a 32-byte SHA-256 hash of the public key data for use in
 * database storage, key lookups, and audit operations.
 */
class KeyHash
{
public:
    typedef shared_ptr<KeyHash> Shared;

public:
    /**
     * @brief Default constructor - creates uninitialized hash
     */
    KeyHash() : mData{} {}

    /**
     * @brief Constructs KeyHash by copying data from the provided buffer
     * Note: The class copies the data into internal memory and does not take ownership
     * of the buffer passed. The caller remains responsible for freeing the buffer.
     * @param buffer Pointer to bytes to copy from. Must be at least kBytesSize bytes long.
     */
    KeyHash(byte_t* buffer);

    /**
     * @brief Overloaded constructor to accept const buffer without requiring widespread const_casts
     */
    KeyHash(const byte_t* buffer) : KeyHash(const_cast<byte_t*>(buffer)) {}

    /**
     * @brief Get raw hash data
     * @return Pointer to hash bytes
     */
    const byte_t* data() const;

    /**
     * @brief Convert hash to hex string representation
     * @return Hex-encoded hash string
     */
    const string toString() const;

    /**
     * @brief Compare hashes for equality
     */
    friend bool operator==(const KeyHash &kh1, const KeyHash &kh2);

    /**
     * @brief Compare hashes for inequality
     */
    friend bool operator!=(const KeyHash &kh1, const KeyHash &kh2);

public:
    static constexpr size_t kBytesSize = 32; // SHA-256 hash size

private:
    byte_t mData[kBytesSize];
};

/**
 * @brief Get the SPHINCS+ algorithm name
 * @return "SLH-DSA-SHA2-256s" - the SPHINCS+ algorithm used
 */
constexpr const char* getAlgorithmName() {
    return "SLH-DSA-SHA2-256s";
}

/**
 * @brief Base class for SPHINCS+ keys providing common functionality
 * 
 * This implementation uses the real SPHINCS+ SLH-DSA-SHA2-256s algorithm
 * from OpenSSL 3.5+, providing:
 * - True post-quantum cryptographic security
 * - Deterministic signatures (same input + key = same signature)
 * - Small signature size variant (256s)
 * - Full compatibility with OpenSSL EVP interface
 */
class BaseKey
{
public:
    /**
     * @return Public key size in bytes for SPHINCS+ SLH-DSA-SHA2-256s
     */
    static constexpr size_t keySize()
    {
        return 64; // SPHINCS+ SLH-DSA-SHA2-256s public key size
    }

protected:
    static constexpr size_t kKeySize = 64;
};

/**
 * @brief SPHINCS+ Public Key class
 * Provides deterministic public key operations using OpenSSL EVP interface
 */
class PublicKey : public BaseKey
{
public:
    typedef shared_ptr<PublicKey> Shared;

    /**
     * @brief Default constructor - creates uninitialized key
     */
    PublicKey();

    /**
     * @brief Constructor from raw key data
     * @param keyData Raw key bytes (must be keySize() bytes)
     */
    explicit PublicKey(const byte_t* keyData);

    /**
     * @brief Constructor from string representation
     * @param keyString Hex-encoded key string
     */
    explicit PublicKey(const string& keyString);

    /**
     * @brief Copy constructor
     */
    PublicKey(const PublicKey& other);

    /**
     * @brief Assignment operator
     */
    PublicKey& operator=(const PublicKey& other);

    /**
     * @brief Destructor
     */
    ~PublicKey() = default;

    /**
     * @brief Get raw key data
     * @return Pointer to key bytes
     */
    const byte_t* data() const;

    /**
     * @brief Convert key to hex string representation
     * @return Hex-encoded key string
     */
    string toString() const;

    /**
     * @brief Compare keys for equality
     */
    bool operator==(const PublicKey& other) const;

    /**
     * @brief Compare keys for inequality
     */
    bool operator!=(const PublicKey& other) const;

    /**
     * @brief Generate hash of the public key for identification
     * @return Key hash as hex string
     */
    string hashString() const;

    /**
     * @brief Generate hash of the public key for database storage and lookups
     * @return Shared pointer to KeyHash object
     */
    const KeyHash::Shared hash() const;

    /**
     * @brief Check if key is valid/initialized
     */
    bool isValid() const;

    /**
     * @brief Serialize key to byte array
     * @param buffer Output buffer (must be at least keySize() bytes)
     */
    void serialize(byte_t* buffer) const;

    /**
     * @brief Deserialize key from byte array
     * @param buffer Input buffer (must be at least keySize() bytes)
     */
    void deserialize(const byte_t* buffer);

private:
    byte_t mKeyData[kKeySize];
    bool mIsValid;
};

/**
 * @brief SPHINCS+ Private Key class
 * Provides secure private key storage and deterministic key operations using real SPHINCS+
 */
class PrivateKey : public BaseKey
{
public:
    typedef shared_ptr<PrivateKey> Shared;

    /**
     * @return Private key size in bytes for SPHINCS+ SLH-DSA-SHA2-256s
     */
    static constexpr size_t privateKeySize()
    {
        return 128; // SPHINCS+ SLH-DSA-SHA2-256s private key size
    }

    /**
     * @brief Default constructor - generates new random private key
     */
    PrivateKey();

    /**
     * @brief Constructor from seed string (deterministic generation)
     * @param seedString String used to deterministically generate private key
     */
    explicit PrivateKey(const string& seedString);

    /**
     * @brief Constructor from raw key data
     * @param keyData Raw private key bytes (must be privateKeySize() bytes)
     */
    explicit PrivateKey(const byte_t* keyData);

    /**
     * @brief Copy constructor (deleted for security)
     */
    PrivateKey(const PrivateKey& other) = delete;

    /**
     * @brief Assignment operator (deleted for security)
     */
    PrivateKey& operator=(const PrivateKey& other) = delete;

    /**
     * @brief Destructor - securely wipes private key data
     */
    ~PrivateKey();

    /**
     * @brief Derive public key from this private key
     * @return Shared pointer to corresponding public key
     */
    PublicKey::Shared derivePublicKey() const;

    /**
     * @brief Check if private key is valid/initialized
     */
    bool isValid() const;

    /**
     * @brief Serialize private key to secure memory segment
     * @return Secure memory segment containing key data
     */
    memory::SecureSegment serialize() const;

    /**
     * @brief Deserialize private key from secure memory segment
     * @param secureData Secure memory segment containing key data
     */
    void deserialize(const memory::SecureSegment& secureData);

    /**
     * @brief Get OpenSSL EVP_PKEY for signing operations
     * Note: This is used internally for signing - external access not provided for security
     */
    EVP_PKEY* getEVPKey() const;

private:
    /**
     * @brief Initialize private key from raw data
     * @param keyData Raw key bytes
     */
    void initFromData(const byte_t* keyData);

    /**
     * @brief Generate random private key using SPHINCS+ algorithm
     */
    void generateRandom();

    /**
     * @brief Generate deterministic private key from seed using SPHINCS+
     * @param seedString Seed for deterministic generation
     */
    void generateFromSeed(const string& seedString);

    /**
     * @brief Securely wipe private key data
     */
    void secureWipe();

private:
    static constexpr size_t kPrivateKeySize = 128; // SPHINCS+ SLH-DSA-SHA2-256s private key size
    memory::SecureSegment mKeyData;
    mutable EVP_PKEY* mEVPKey;
    bool mIsValid;
};

} // namespace sphincs
} // namespace crypto

#endif // VTCPD_SPHINCSKEYS_H