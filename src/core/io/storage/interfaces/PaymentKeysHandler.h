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
    
    virtual void saveOwnKey(
        const TransactionUUID &transactionUUID,
        const PublicKey::Shared publicKey,
        const PrivateKey *privateKey) = 0;
    
    virtual PrivateKey* getOwnPrivateKey(
        const TransactionUUID &transactionUUID) = 0;
    
    virtual void deleteKeyByTransactionUUID(
        const TransactionUUID &transactionUUID) = 0;
    
    virtual vector<TransactionUUID> allTransactionUUIDs() = 0;
};

#endif //VTCPD_INTERFACES_PAYMENTKEYSHANDLER_H 