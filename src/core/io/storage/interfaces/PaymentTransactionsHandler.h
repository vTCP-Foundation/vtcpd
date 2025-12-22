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

/**
 * Represents the observing state of a payment transaction in the database.
 * These values are stored in the `observing_state` column of the `payment_transactions` table.
 */
enum class PaymentObservingState : int
{
    /**
     * Initial state when transaction is created.
     */
    Init = 0,

    /**
     * Transaction has been approved locally and committed.
     */
    Committed = 1,

    /**
     * Participants' votes are present on the blockchain.
     * Transaction has been fully confirmed by the observer.
     */
    ParticipantsVotesPresent = 2,

    /**
     * Transaction was rejected by the observer.
     * This can happen when claiming time has expired or other validation failures.
     */
    RejectedByObserving = 3
};

class PaymentTransactionsHandler
{
public:
    virtual ~PaymentTransactionsHandler() = default;

    /**
     * Saves a payment transaction record to the database.
     *
     * @param transactionUUID Unique identifier of the payment transaction
     * @param maximalClaimingBlockNumber The latest block number by which the transaction
     *        can be claimed on the blockchain
     * @param effectiveClaimingBlockNumber The extended claiming deadline that includes
     *        the dispute grace period. This value equals maximalClaimingBlockNumber +
     *        disputeGracePeriodBlocksCount and represents the actual block number until
     *        which the transaction should be monitored by the observer for potential disputes
     */
    virtual void saveRecord(
        const TransactionUUID &transactionUUID,
        BlockNumber maximalClaimingBlockNumber,
        BlockNumber effectiveClaimingBlockNumber) = 0;

    virtual void updateTransactionState(
        const TransactionUUID &transactionUUID,
        PaymentObservingState observingState) = 0;

    // TODO: Remove this method. It is not used.
    virtual vector<pair<TransactionUUID, BlockNumber>> transactionsWithUncertainObservingState() = 0;

    /**
     * Retrieves transactions that still have time left in their effective claiming window
     * (including dispute grace period) and are in uncertain observing state (state = 0),
     * ordered by claiming block.
     *
     * The method filters transactions where effective_claiming_block_number > minBlockNumber,
     * ensuring that transactions are monitored until the dispute grace period expires.
     *
     * @param minBlockNumber Minimal block number threshold; only transactions with
     *        effective_claiming_block_number greater than this value are returned
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
