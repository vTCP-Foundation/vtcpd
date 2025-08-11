#ifndef VTCPD_INTERFACES_PAYMENTKEYSHANDLER_H
#define VTCPD_INTERFACES_PAYMENTKEYSHANDLER_H

#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../crypto/sphincskeys.h"
#include <sqlite3.h>

using namespace crypto::sphincs;

class PaymentKeysHandler
{
public:
    virtual ~PaymentKeysHandler() = default;

    // Save a reusable payment key (no transaction linkage).
    virtual void saveOwnKey(
        const PublicKey::Shared publicKey,
        const PrivateKey *privateKey) = 0;

    // Get the most recently saved private key (by max id).
    virtual PrivateKey* getOwnPrivateKey() = 0;

    // Get the most recently saved public key (by max id).
    virtual PublicKey::Shared getOwnPublicKey() = 0;

    // Delete a key by its numeric id.
    virtual void deleteKeyByID(
        const uint64_t id) = 0;

    // Presence check for startup.
    virtual bool hasAnyKeys() = 0;

    // Latest key numeric identifier (by max id).
    virtual uint64_t latestKeyID() = 0;
};

#endif //VTCPD_INTERFACES_PAYMENTKEYSHANDLER_H 