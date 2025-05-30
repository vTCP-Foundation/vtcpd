#ifndef VTCPD_LAMPORTSCHEME_H
#define VTCPD_LAMPORTSCHEME_H

#include "lamportkeys.h"

#include <bitset>
#include <cstring>
#include <memory>

namespace crypto {
namespace lamport {

using namespace std;

class Signature
{
public:
    typedef shared_ptr<Signature> Shared;

public:
    explicit Signature(
        byte_t* data,
        size_t dataSize,
        PrivateKey *pKey);

    Signature(
        byte_t* data);

    ~Signature();

    static const size_t signatureSize();

public:
    bool check(
        byte_t* data,
        size_t dataSize,
        PublicKey::Shared pubKey) noexcept;

    const byte_t* data() const;

protected:
    void collectSignature(
        byte_t* key,
        byte_t* sign,
        byte_t* messageHash) noexcept;

public:
    static const size_t kSize =
        PrivateKey::kRandomNumberSlotSize * PrivateKey::kRandomNumbersSlotsCount / 2;

protected:
    unsigned char* mData;
};

}
}

#endif // VTCPD_LAMPORTSCHEME_H
