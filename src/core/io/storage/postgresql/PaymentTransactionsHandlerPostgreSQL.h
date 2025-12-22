#ifndef VTCPD_PAYMENTTRANSACTIONSHANDLERPOSTGRESQL_H
#define VTCPD_PAYMENTTRANSACTIONSHANDLERPOSTGRESQL_H

#include "../interfaces/PaymentTransactionsHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/Types.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <string>
#include <vector>

class PaymentTransactionsHandlerPostgreSQL : public PaymentTransactionsHandler
{
public:
    PaymentTransactionsHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    /**
     * Saves new payment transaction record.
     *
     * @param transactionUUID Unique identifier of the payment transaction
     * @param maximalClaimingBlockNumber The latest block number by which the transaction
     *        can be claimed on the blockchain
     * @param effectiveClaimingBlockNumber The extended claiming deadline that includes
     *        the dispute grace period (maximalClaimingBlockNumber + disputeGracePeriodBlocksCount)
     */
    void saveRecord(
        const TransactionUUID &transactionUUID,
        BlockNumber maximalClaimingBlockNumber,
        BlockNumber effectiveClaimingBlockNumber) override;

    void updateTransactionState(
        const TransactionUUID &transactionUUID,
        PaymentObservingState observingState) override;

    std::vector<std::pair<TransactionUUID, BlockNumber>> transactionsWithUncertainObservingState() override;

    /**
     * Retrieves transactions still inside effective claiming window (including dispute
     * grace period) and in uncertain observing state.
     *
     * @param minBlockNumber Lower exclusive bound for effective_claiming_block_number
     * @param limit Maximum number of transactions to return
     * @return Vector of (TransactionUUID, BlockNumber) ordered by claiming block
     */
    std::vector<std::pair<TransactionUUID, BlockNumber>> transactionsForObserverMonitoring(
        BlockNumber minBlockNumber,
        uint32_t limit) override;

    bool isTransactionPresent(
        const TransactionUUID &transactionUUID) override;

    void deleteRecord(
        const TransactionUUID &transactionUUID) override;

    std::vector<TransactionUUID> allTransactionsUUID() override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_PAYMENTTRANSACTIONSHANDLERPOSTGRESQL_H 
