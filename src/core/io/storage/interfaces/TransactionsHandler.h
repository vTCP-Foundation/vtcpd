#ifndef VTCPD_INTERFACES_TRANSACTIONSHANDLER_H
#define VTCPD_INTERFACES_TRANSACTIONSHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/Types.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"

class TransactionsHandler
{
public:
    virtual ~TransactionsHandler() = default;

    virtual void saveRecord(
        const TransactionUUID &transactionUUID,
        BytesShared transaction,
        size_t transactionBytesCount) = 0;

    virtual BytesShared getTransaction(
        const TransactionUUID &transactionUUID) = 0;

    virtual bool isTransactionSerialized(
        const TransactionUUID &transactionUUID) = 0;

    virtual void deleteRecordIfExists(
        const TransactionUUID &transactionUUID) = 0;

    virtual vector<BytesShared> allTransactions() = 0;
};

#endif //VTCPD_INTERFACES_TRANSACTIONSHANDLER_H 