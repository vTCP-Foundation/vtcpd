#ifndef VTCPD_LAMPORTKEYS_H
#define VTCPD_LAMPORTKEYS_H

#include "memory.h"

#include <sodium.h>
#include <memory>
#include <string>


namespace crypto {
namespace lamport {

using namespace std;

class BaseKey : boost::noncopyable
{
    friend class Signature;

public:
    /**
     * @return Generic size of the key in bytes.
     */
    static constexpr size_t keySize()
    {
        return 16 * 1024;
    }

protected:
    static constexpr size_t kRandomNumbersSlotsCount = 256 * 2;
    static constexpr size_t kRandomNumberSlotSize = 256 / 8;
};

class KeyHash
{
public:
    typedef shared_ptr<KeyHash> Shared;

public:
    KeyHash() : mData{} {}

    /**
     * Constructs KeyHash by copying data from the provided buffer
     * Note: The class copies the data into internal memory and does not take ownership
     * of the buffer passed. The caller remains responsible for freeing the buffer.
     * @param buffer Pointer to bytes to copy from. Must be at least kBytesSize bytes long.
     */
    KeyHash(
        byte_t* buffer);

    // Overloaded constructor to accept const buffer without requiring widespread const_casts.
    KeyHash(
        const byte_t* buffer) : KeyHash(const_cast<byte_t*>(buffer)) {}

    const byte_t* data() const;

    const string toString() const;

    friend bool operator==(
        const KeyHash &kh1,
        const KeyHash &kh2);

    friend bool operator!=(
        const KeyHash &kh1,
        const KeyHash &kh2);

public:
    static constexpr size_t kBytesSize = 32;

private:
    byte_t mData[kBytesSize];
};

class PublicKey : public BaseKey
{
    friend class PrivateKey;
    friend class Signature;

public:
    typedef shared_ptr<PublicKey> Shared;

    PublicKey() : mData(nullptr) {}

    PublicKey(
        byte_t* data);

    PublicKey(
        const byte_t* data) : PublicKey(const_cast<byte_t*>(data)) {}

    ~PublicKey() noexcept;

    const byte_t* data() const;

    const KeyHash::Shared hash() const;

public:
    using BaseKey::BaseKey;

private:
    byte_t* mData;
};

class PrivateKey : public BaseKey
{
    friend class Signature;

public:
    explicit PrivateKey();

    /**
     * Constructs PrivateKey using deterministic random generation with seed string.
     * @param seedString String to use as seed (will be hashed to 32 bytes if needed).
     */
    PrivateKey(
        const string& seedString);

    PrivateKey(
        byte_t* data);

    PrivateKey(
        const byte_t* data) : PrivateKey(const_cast<byte_t*>(data)) {}

    PublicKey::Shared derivePublicKey();

    void crop();

    const memory::SecureSegment *data() const;

    /**
     * Returns string representation of private key hash for logging purposes.
     * @return Hash of private key as hex string
     */
    const string toString() const;

private:
    memory::SecureSegment mData;
    bool mIsCropped;
};

}
}

#endif // VTCPD_LAMPORTKEYS_H
