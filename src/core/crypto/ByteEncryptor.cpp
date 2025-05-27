#include "ByteEncryptor.h"

ByteEncryptor::ByteEncryptor(
    const ByteEncryptor::PublicKey::Shared &publicKey) : mPublicKey(publicKey)
{
}

ByteEncryptor::ByteEncryptor(
    const ByteEncryptor::PublicKey::Shared &publicKey,
    const ByteEncryptor::SecretKey::Shared &secretKey) : mPublicKey(publicKey),
    mSecretKey(secretKey)
{
}

ByteEncryptor::KeyPair::Shared ByteEncryptor::generateKeyPair()
{
    KeyPair::Shared keyPair = make_shared<KeyPair>();
    keyPair->publicKey = std::make_shared<PublicKey>();
    keyPair->secretKey = std::make_shared<SecretKey>();
    crypto_box_keypair(
        keyPair->publicKey->key,
        keyPair->secretKey->key);
    return keyPair;
}

ByteEncryptor::PublicKey::Shared ByteEncryptor::generateUndefinedKey()
{
    string key(ByteEncryptor::PublicKey::kBytesSize, '0');
    return std::make_shared<PublicKey>(key);
}

ByteEncryptor::Buffer ByteEncryptor::encrypt(
    byte_t* bytes,
    size_t size,
    size_t headerSize) const
{
    if (!mPublicKey) {
        return ByteEncryptor::Buffer(nullptr, 0);
    }
    size_t len = size + crypto_box_SEALBYTES + headerSize;
    ByteEncryptor::Buffer cipher(
        tryMalloc(len),
        len);
    crypto_box_seal(
        cipher.first.get() + headerSize,
        bytes,
        size,
        mPublicKey->key);
    return cipher;
}

ByteEncryptor::Buffer ByteEncryptor::decrypt(
    byte_t* cipher,
    size_t size,
    size_t headerSize) const
{
    if (!mPublicKey || !mSecretKey) {
        return ByteEncryptor::Buffer(nullptr, 0);
    }
    size_t len = (size - crypto_box_SEALBYTES) + headerSize;
    ByteEncryptor::Buffer bytes(
        tryMalloc(len),
        len);
    auto result = crypto_box_seal_open(
                      bytes.first.get() + headerSize,
                      cipher,
                      size,
                      mPublicKey->key,
                      mSecretKey->key);
    return result ? ByteEncryptor::Buffer(nullptr, 0) : bytes;
}

ByteEncryptor::Buffer ByteEncryptor::encrypt(
    const ByteEncryptor::Buffer &bytes) const
{
    return encrypt(
               bytes.first.get(),
               bytes.second);
}

ByteEncryptor::Buffer ByteEncryptor::decrypt(
    const ByteEncryptor::Buffer &cipher) const
{
    return decrypt(
               cipher.first.get(),
               cipher.second);
}

static void parseHex(
    byte_t* out,
    const string &in)
{
    char b[3], i = 0;
    b[2] = '\0';
    for (const char* p = in.c_str(), *e = p + in.length(); p < e; p += 2, ++i) {
        memcpy(b, p, 2);
        out[i] = (byte_t)std::stoul(b, nullptr, 16);
    }
}

std::string ByteEncryptor_parsePar(
    std::string &par,
    const std::string &separator)
{
    if (par.find(separator) != std::string::npos) {
        std::string options = par.substr(par.find(separator) + 1);
        par = par.substr(0, par.find(separator));
        return options;
    }
    return "";
}

ByteEncryptor::PublicKey::PublicKey(
    const string &str)
{
    parseHex(key, str);
}

ByteEncryptor::SecretKey::SecretKey(
    const string &str)
{
    parseHex(key, str);
}

ByteEncryptor::KeyPair::KeyPair(const string &str)
{
    ;
}

std::ostream &operator<<(
    std::ostream &out,
    const ByteEncryptor::PublicKey &t)
{
    /*
    * The following stream operator overloads for PublicKey and SecretKey previously used
    * sprintf/snprintf with a fixed-size stack buffer to convert bytes to hexadecimal strings.
    * This approach had a potential stack buffer overflow vulnerability if `byte_t` was a signed
    * type and processed negative values. When a signed char with a negative value (e.g., -1, bit pattern 0xff)
    * is passed to sprintf with the "%x" format specifier, it's promoted to an int (e.g., -1).
    * The "%x" specifier expects an unsigned int, so the negative int is converted to its
    * unsigned representation (e.g., 0xffffffff on a 32-bit system).
    * sprintf would then attempt to write the full hexadecimal string (e.g., "ffffffff") plus a
    * null terminator into the small fixed-size buffer, causing a stack overflow.
    *
    * Stack buffer overflows can corrupt adjacent stack memory, including other local variables,
    * saved registers, or the function's return address. This corruption can, in turn, lead to
    * further issues like heap corruption if, for example, a corrupted pointer or size is used
    * in heap operations, or if objects managing heap memory (like std::string or std::shared_ptr)
    * have their internal state corrupted.
    *
    * To mitigate this risk, the implementation was changed to use C++ stream manipulators
    * (std::hex, std::setw, std::setfill) with std::stringstream. This approach is type-safe,
    * avoids manual buffer management, and is generally preferred in modern C++ for output formatting.
    * The byte value is cast to `unsigned char` and then to `unsigned int` before streaming to ensure
    * it's treated as a numerical byte value (0-255) and formatted correctly by the hex manipulator,
    * preventing the overflow and ensuring correct two-digit hexadecimal output for each byte.
    */

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (byte_t i : t.key) {
        ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(i));
    }
    return (out << ss.str());
}

std::ostream &operator<<(
    std::ostream &out,
    const ByteEncryptor::SecretKey &t)
{
    /*
     * See the comment in the PublicKey operator<< for more details.
     */

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (byte_t i : t.key) {
        ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(i));
    }
    return (out << ss.str());
}

std::ostream &operator<<(
    std::ostream &out,
    const ByteEncryptor::KeyPair &t)
{
    out << *t.publicKey
        << "_"
        << *t.secretKey;
    return out;
}
