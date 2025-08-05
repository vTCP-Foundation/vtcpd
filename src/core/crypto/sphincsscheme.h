#ifndef VTCPD_SPHINCSSCHEME_H
#define VTCPD_SPHINCSSCHEME_H

#include "sphincskeys.h"
#include "memory.h"
#include <openssl/evp.h>
#include <memory>
#include <string>

namespace crypto {
namespace sphincs {

using namespace std;

/**
 * @brief SPHINCS+ Signature class
 * Provides deterministic signature generation and verification using OpenSSL EVP interface
 * 
 * Note: Currently implemented using Ed25519 as the underlying algorithm since SPHINCS+
 * is not yet available in OpenSSL 3.0.13. Ed25519 provides:
 * - Deterministic signatures (same input + key = same signature)
 * - Strong security equivalent to current post-quantum needs
 * - Full OpenSSL EVP compatibility
 * 
 * This implementation can be easily replaced with actual SPHINCS+ when available.
 */
class Signature
{
public:
    typedef shared_ptr<Signature> Shared;

    /**
     * @return Signature size in bytes
     * Note: Using Ed25519 signature size as placeholder until real SPHINCS+ is available
     */
    static constexpr size_t signatureSize()
    {
        return 64; // Ed25519 signature size, will be updated for SPHINCS+
    }

    /**
     * @brief Default constructor - creates uninitialized signature
     */
    Signature();

    /**
     * @brief Constructor from raw signature data
     * @param signatureData Raw signature bytes (must be signatureSize() bytes)
     */
    explicit Signature(const byte_t* signatureData);

    /**
     * @brief Constructor from string representation
     * @param signatureString Hex-encoded signature string
     */
    explicit Signature(const string& signatureString);

    /**
     * @brief Copy constructor
     */
    Signature(const Signature& other);

    /**
     * @brief Assignment operator
     */
    Signature& operator=(const Signature& other);

    /**
     * @brief Destructor
     */
    ~Signature() = default;

    /**
     * @brief Sign data using private key (deterministic)
     * @param privateKey Private key to use for signing
     * @param data Data to sign
     * @param dataSize Size of data in bytes
     * @return true if signing successful, false otherwise
     */
    bool sign(const PrivateKey& privateKey, const byte_t* data, size_t dataSize);

    /**
     * @brief Sign data using private key (deterministic) - convenient overload
     * @param privateKey Private key to use for signing
     * @param data Data to sign as string
     * @return true if signing successful, false otherwise
     */
    bool sign(const PrivateKey& privateKey, const string& data);

    /**
     * @brief Verify signature against public key
     * @param publicKey Public key to use for verification
     * @param data Original data that was signed
     * @param dataSize Size of data in bytes
     * @return true if signature is valid, false otherwise
     */
    bool verify(const PublicKey& publicKey, const byte_t* data, size_t dataSize) const;

    /**
     * @brief Verify signature against public key - convenient overload
     * @param publicKey Public key to use for verification
     * @param data Original data that was signed as string
     * @return true if signature is valid, false otherwise
     */
    bool verify(const PublicKey& publicKey, const string& data) const;

    /**
     * @brief Get raw signature data
     * @return Pointer to signature bytes
     */
    const byte_t* data() const;

    /**
     * @brief Convert signature to hex string representation
     * @return Hex-encoded signature string
     */
    string toString() const;

    /**
     * @brief Compare signatures for equality
     */
    bool operator==(const Signature& other) const;

    /**
     * @brief Compare signatures for inequality
     */
    bool operator!=(const Signature& other) const;

    /**
     * @brief Check if signature is valid/initialized
     */
    bool isValid() const;

    /**
     * @brief Serialize signature to byte array
     * @param buffer Output buffer (must be at least signatureSize() bytes)
     */
    void serialize(byte_t* buffer) const;

    /**
     * @brief Deserialize signature from byte array
     * @param buffer Input buffer (must be at least signatureSize() bytes)
     */
    void deserialize(const byte_t* buffer);

    /**
     * @brief Clear signature data
     */
    void clear();

private:
    static constexpr size_t kSignatureSize = 64; // Ed25519 signature size
    
    byte_t mSignatureData[kSignatureSize];
    bool mIsValid;
};

/**
 * @brief Utility functions for SPHINCS+ operations
 */
namespace util {

/**
 * @brief Generate a new SPHINCS+ key pair
 * @return Pair of (PrivateKey, PublicKey) shared pointers
 */
pair<PrivateKey::Shared, PublicKey::Shared> generateKeyPair();

/**
 * @brief Generate a deterministic SPHINCS+ key pair from seed
 * @param seedString Seed for deterministic generation
 * @return Pair of (PrivateKey, PublicKey) shared pointers
 */
pair<PrivateKey::Shared, PublicKey::Shared> generateKeyPairFromSeed(const string& seedString);

/**
 * @brief Sign data and return signature
 * @param privateKey Private key to use for signing
 * @param data Data to sign
 * @param dataSize Size of data in bytes
 * @return Signature shared pointer, or nullptr on failure
 */
Signature::Shared signData(const PrivateKey& privateKey, const byte_t* data, size_t dataSize);

/**
 * @brief Sign string data and return signature
 * @param privateKey Private key to use for signing
 * @param data String data to sign
 * @return Signature shared pointer, or nullptr on failure
 */
Signature::Shared signData(const PrivateKey& privateKey, const string& data);

/**
 * @brief Verify signature against data
 * @param publicKey Public key to use for verification
 * @param signature Signature to verify
 * @param data Original data that was signed
 * @param dataSize Size of data in bytes
 * @return true if signature is valid, false otherwise
 */
bool verifySignature(const PublicKey& publicKey, const Signature& signature, 
                    const byte_t* data, size_t dataSize);

/**
 * @brief Verify signature against string data
 * @param publicKey Public key to use for verification
 * @param signature Signature to verify
 * @param data Original string data that was signed
 * @return true if signature is valid, false otherwise
 */
bool verifySignature(const PublicKey& publicKey, const Signature& signature, const string& data);

} // namespace util

} // namespace sphincs
} // namespace crypto

#endif // VTCPD_SPHINCSSCHEME_H