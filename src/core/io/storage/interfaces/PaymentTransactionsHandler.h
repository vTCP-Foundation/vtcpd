#ifndef VTCPD_INTERFACES_PAYMENTTRANSACTIONSHANDLER_H
#define VTCPD_INTERFACES_PAYMENTTRANSACTIONSHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/Types.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include <utility>
#include <vector>

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

    /**
     * Retrieves transactions that still have time left in their claiming window
     * and are in uncertain observing state (state = 0), ordered by claiming block.
     *
     * @param minBlockNumber Minimal block number threshold; only greater values are returned
     * @param limit Maximum number of transactions to fetch
     * @return Vector of (TransactionUUID, BlockNumber) sorted by maximal_claiming_block_number ascending
     */
    virtual vector<pair<TransactionUUID, BlockNumber>> transactionsForObserverMonitoring(
        BlockNumber minBlockNumber,
        uint32_t limit) = 0;

    virtual bool isTransactionPresent(
        const TransactionUUID& transactionUUID) = 0;

    virtual void deleteRecord(
        const TransactionUUID &transactionUUID) = 0;

    virtual vector<TransactionUUID> allTransactionsUUID() = 0;
};

#endif //VTCPD_INTERFACES_PAYMENTTRANSACTIONSHANDLER_H 
