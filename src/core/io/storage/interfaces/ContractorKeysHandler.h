#ifndef VTCPD_INTERFACES_CONTRACTORKEYSHANDLER_H
#define VTCPD_INTERFACES_CONTRACTORKEYSHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/memory/MemoryUtils.h"

#include <memory>
#include <vector>

using namespace std;
using namespace crypto::sphincs;

class ContractorKeysHandler
{
public:
    virtual ~ContractorKeysHandler() = default;

    virtual void saveKey(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber,
        const PublicKey::Shared publicKey,
        const KeyNumber number) = 0;

    virtual const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID) = 0;

    virtual void invalidKey(
        const TrustLineID trustLineID,
        const KeyNumber number) = 0;

    virtual void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) = 0;

    virtual PublicKey::Shared keyByNumber(
        const TrustLineID trustLineID,
        const KeyNumber number) = 0;

    virtual PublicKey::Shared keyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) = 0;

    virtual const KeyHash::Shared keyHashByNumber(
        const TrustLineID trustLineID,
        const KeyNumber number) = 0;

    virtual KeysCount availableKeysCnt(
        const TrustLineID trustLineID) = 0;

    virtual KeysCount sequenceKeysCnt(
        const TrustLineID trustLineID,
        KeyNumber keysSetSequenceNumber) = 0;

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

#endif //VTCPD_INTERFACES_CONTRACTORKEYSHANDLER_H 