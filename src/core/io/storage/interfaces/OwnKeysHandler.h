#ifndef VTCPD_INTERFACES_OWNKEYSHANDLER_H
#define VTCPD_INTERFACES_OWNKEYSHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/lamportkeys.h"
#include "../../../crypto/lamportscheme.h"
#include "../../../common/memory/MemoryUtils.h"

#include <memory>
#include <vector>

using namespace std;
using namespace crypto::lamport;

class OwnKeysHandler
{
public:
    virtual ~OwnKeysHandler() = default;

    virtual void saveKey(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber,
        const PublicKey::Shared publicKey,
        const PrivateKey *privateKey,
        const KeyNumber number) = 0;

    virtual const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID) = 0;

    virtual pair<std::unique_ptr<PrivateKey>, KeyNumber> nextAvailableKey(
        const TrustLineID trustLineID) = 0;

    virtual void invalidKey(
        const TrustLineID trustLineID,
        const KeyNumber number,
        const Signature::Shared signature) = 0;

    virtual void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash,
        const Signature::Shared signature) = 0;

    virtual const PublicKey::Shared getPublicKey(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) = 0;

    virtual const PublicKey::Shared getPublicKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) = 0;

    virtual const KeyHash::Shared getPublicKeyHash(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) = 0;

    virtual const KeyNumber getKeyNumberByHash(
        const KeyHash::Shared keyHash) = 0;

    virtual KeysCount availableKeysCnt(
        const TrustLineID trustLineID) = 0;

    virtual void removeUnusedKeys(
        const TrustLineID trustLineID) = 0;

    virtual vector<PublicKey::Shared> publicKeysBySetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const = 0;

    virtual void deleteKeysByTrustLineID(
        const TrustLineID trustLineID) = 0;

    virtual void deleteKeyByHashExceptSequenceNumber(
        KeyHash::Shared keyHash,
        const KeyNumber keysSetSequenceNumber) = 0;

    virtual vector<KeyHash::Shared> publicKeyHashesLessThanSetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const = 0;
};

#endif //VTCPD_INTERFACES_OWNKEYSHANDLER_H 