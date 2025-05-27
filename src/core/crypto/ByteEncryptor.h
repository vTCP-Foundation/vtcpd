#ifndef VTCPD_BYTEENCRYPTOR_H
#define VTCPD_BYTEENCRYPTOR_H

#include <sodium.h>
#include "../common/Types.h"
#include "../common/memory/MemoryUtils.h"

#include <iomanip>


class ByteEncryptor
{
public:
    typedef pair<BytesShared, size_t> Buffer;
    struct PublicKey
    {
        typedef std::shared_ptr<PublicKey> Shared;
        static const size_t kBytesSize = crypto_box_PUBLICKEYBYTES;
        PublicKey()
        {
            // Zeroing the key in default constructor to avoid any potential security issues.
            memset(key, 0, kBytesSize);
        }
        explicit PublicKey(const string &str);
        byte_t key[kBytesSize];
    };

    struct SecretKey
    {
        typedef std::shared_ptr<SecretKey> Shared;
        static const size_t kBytesSize = crypto_box_SECRETKEYBYTES;
        SecretKey()
        {
            // Zeroing the key in default constructor to avoid any potential security issues.
            memset(key, 0, kBytesSize);
        }
        explicit SecretKey(const string &str);
        byte_t key[kBytesSize];
    };

    struct KeyPair
    {
        typedef std::shared_ptr<KeyPair> Shared;
        KeyPair() = default;
        explicit KeyPair(const string &str);
        PublicKey::Shared publicKey = nullptr;
        SecretKey::Shared secretKey = nullptr;
    };

public:
    explicit ByteEncryptor(
        const PublicKey::Shared &publicKey);
    ByteEncryptor(
        const PublicKey::Shared &publicKey,
        const SecretKey::Shared &secretKey);

public:
    static KeyPair::Shared generateKeyPair();
    static PublicKey::Shared generateUndefinedKey();

public:
    Buffer encrypt(byte_t* bytes, size_t size, size_t headerSize = 0) const;
    Buffer decrypt(byte_t* cipher, size_t size, size_t headerSize = 0) const;
    Buffer encrypt(const Buffer &bytes) const;
    Buffer decrypt(const Buffer &cipher) const;

protected:
    PublicKey::Shared mPublicKey = nullptr;
    SecretKey::Shared mSecretKey = nullptr;
};

std::ostream &operator<<(std::ostream &out, const ByteEncryptor::PublicKey &t);
std::ostream &operator<<(std::ostream &out, const ByteEncryptor::SecretKey &t);
std::ostream &operator<<(std::ostream &out, const ByteEncryptor::KeyPair &t);

#endif // VTCPD_BYTEENCRYPTOR_H
