#ifndef VTCPD_SPHINCSSCHEME_H
#define VTCPD_SPHINCSSCHEME_H

#include "sphincskeys.h"
#include "memory.h"
#include <openssl/evp.h>
#include <memory>
#include <string>
#include <array>
#include <type_traits>

namespace crypto {
namespace sphincs {

using namespace std;

/**
 * @brief SPHINCS+ Signature class
 * Provides deterministic signature generation and verification using real SPHINCS+ SLH-DSA-SHA2-256s
 *
 * This implementation uses the real SPHINCS+ SLH-DSA-SHA2-256s algorithm
 * from OpenSSL 3.5+, providing:
 * - True post-quantum cryptographic security
 * - Deterministic signatures (same input + key = same signature)
 * - Small signature size variant (256s)
 * - Full compatibility with OpenSSL EVP interface
 *
 * @section Security Security Considerations
 * - No pre-hashing: Raw data is passed directly to SPHINCS+ EVP functions
 * - Constant-time comparisons prevent timing attacks on signature validation
 * - Deterministic signing ensures reproducible signatures for the same input
 * - Input validation prevents processing of malformed signature data
 * - EVP interface provides proper error handling and resource management
 *
 * @section Threading Thread Safety
 * - Individual Signature objects are NOT thread-safe for concurrent modification
 * - Multiple threads can safely read from the same signature object
 * - Sign/verify operations can be performed concurrently with different objects
 * - OpenSSL EVP operations are internally synchronized
 *
 * @section Performance Performance Notes
 * - Signature size: 29,792 bytes (large but secure)
 * - Verification is faster than signing (typical for SPHINCS+)
 * - Signature data storage is stack-based (fixed-size arrays)
 *
 * @note Large Object Consideration
 * At 29KB per signature, copying is expensive. Consider move semantics
 * or pass by reference when performance is critical.
 */
class Signature
{
public:
    typedef shared_ptr<Signature> Shared;

    /**
     * @return Signature size in bytes for SPHINCS+ SLH-DSA-SHA2-256s
     */
    static constexpr size_t signatureSize()
    {
        return 29792; // SPHINCS+ SLH-DSA-SHA2-256s signature size
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
     * @brief Get raw signature data
     * @return Pointer to signature bytes
     */
    const byte_t* data() const noexcept;

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
    bool isValid() const noexcept;

    /**
     * @brief Serialize signature to byte array
     * @param buffer Output buffer (must be at least signatureSize() bytes)
     * @throws std::invalid_argument if buffer is null
     */
    void serialize(byte_t* buffer) const;

    /**
     * @brief Deserialize signature from byte array
     * @param buffer Input buffer (must be at least signatureSize() bytes)
     * @throws std::invalid_argument if buffer is null
     */
    void deserialize(const byte_t* buffer);

    /**
     * @brief Clear signature data
     */
    void clear();

private:
    static constexpr size_t kSignatureSize = 29792; // SPHINCS+ SLH-DSA-SHA2-256s signature size

    std::array<byte_t, kSignatureSize> mSignatureData;
    bool mIsValid;
};

/**
 * @brief Utility functions for SPHINCS+ operations
 *
 * @note Security:
 * - All utility functions maintain the same security guarantees as their class counterparts
 * - Key generation uses cryptographically secure randomness
 * - Deterministic generation from seeds is consistent and secure
 *
 * @note Thread Safety:
 * - All utility functions are thread-safe and can be called concurrently
 * - Each function call operates on independent resources
 * - No shared mutable state between function calls
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
 * @brief Get detailed OpenSSL error information
 * @param baseMessage Base error message to prepend
 * @return Formatted error string with OpenSSL error details
 */
string getOpenSSLError(const string& baseMessage);

} // namespace util

} // namespace sphincs
} // namespace crypto

#endif // VTCPD_SPHINCSSCHEME_H