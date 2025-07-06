#ifndef VTCPD_INTERFACES_PAYMENTTRANSACTIONSHANDLER_H
#define VTCPD_INTERFACES_PAYMENTTRANSACTIONSHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/Types.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/exceptions/NotFoundError.h"

class PaymentTransactionsHandler
{
public:
    virtual ~PaymentTransactionsHandler() = default;

    virtual void saveRecord(
        const TransactionUUID &transactionUUID,
        BlockNumber maximalClaimingBlockNumber) = 0;

    virtual void updateTransactionState(
        const TransactionUUID &transactionUUID,
        int observingTransactionState) = 0;

    virtual vector<pair<TransactionUUID, BlockNumber>> transactionsWithUncertainObservingState() = 0;

    virtual bool isTransactionPresent(
        const TransactionUUID& transactionUUID) = 0;

    virtual void deleteRecord(
        const TransactionUUID &transactionUUID) = 0;

    virtual vector<TransactionUUID> allTransactionsUUID() = 0;
};

#endif //VTCPD_INTERFACES_PAYMENTTRANSACTIONSHANDLER_H 