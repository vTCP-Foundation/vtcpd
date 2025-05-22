#ifndef VTCPD_LAMPORTKEYS_H
#define VTCPD_LAMPORTKEYS_H

#include "memory.h"

#include <sodium.h>
#include <memory>


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
    KeyHash() = default;

    /**
     * Constructs KeyHash by copying data from the provided buffer
     * Note: The class copies the data into internal memory and does not take ownership
     * of the buffer passed. The caller remains responsible for freeing the buffer.
     * @param buffer Pointer to bytes to copy from. Must be at least kBytesSize bytes long.
     */
    KeyHash(
        byte_t* buffer);

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

    PublicKey() = default;

    PublicKey(
        byte_t* data);

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

    PrivateKey(
        byte_t* data);

    PublicKey::Shared derivePublicKey();

    void crop();

    const memory::SecureSegment *data() const;

private:
    memory::SecureSegment mData;
    bool mIsCropped;
};

}
}

#endif // VTCPD_LAMPORTKEYS_H
