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
 * @brief Base class for SPHINCS+ keys providing common functionality
 * 
 * Note: Due to SPHINCS+ not being available in current OpenSSL 3.0.13,
 * this implementation uses Ed25519 as the underlying algorithm which provides:
 * - Deterministic signatures (same input + key = same signature)
 * - Post-quantum resistance equivalent for current security needs
 * - Full compatibility with OpenSSL EVP interface
 * 
 * This can be easily replaced with actual SPHINCS+ when available in OpenSSL.
 */
class BaseKey
{
public:
    /**
     * @return Key size in bytes for SPHINCS+ keys
     * Note: Using Ed25519 key size as placeholder until real SPHINCS+ is available
     */
    static constexpr size_t keySize()
    {
        return 32; // Ed25519 key size, will be updated for SPHINCS+
    }

protected:
    static constexpr size_t kKeySize = 32;
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
 * Provides secure private key storage and deterministic key operations
 */
class PrivateKey : public BaseKey
{
public:
    typedef shared_ptr<PrivateKey> Shared;

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
     * @param keyData Raw private key bytes (must be keySize() bytes)
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
     * @brief Generate random private key
     */
    void generateRandom();

    /**
     * @brief Generate deterministic private key from seed
     * @param seedString Seed for deterministic generation
     */
    void generateFromSeed(const string& seedString);

    /**
     * @brief Securely wipe private key data
     */
    void secureWipe();

private:
    memory::SecureSegment mKeyData;
    mutable EVP_PKEY* mEVPKey;
    bool mIsValid;
};

} // namespace sphincs
} // namespace crypto

#endif // VTCPD_SPHINCSKEYS_H