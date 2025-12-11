#ifndef VTCPD_PAYMENTTRANSACTIONSHANDLERSQLITE_H
#define VTCPD_PAYMENTTRANSACTIONSHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/PaymentTransactionsHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>
/**
 * Handles storage and retrieval of payment transaction records in SQLite database.
 *
 * Manages transaction UUID tracking with observing states and block numbers for
 * payment processing. Provides CRUD operations with proper error handling and
 * memory management using RAII patterns.
 */
class PaymentTransactionsHandlerSQLite : public PaymentTransactionsHandler
{
public:
    /**
     * Constructor with database connection validation.
     *
     * @param dbConnection SQLite database connection (cannot be null)
     * @param tableName Target table name (cannot be empty)
     * @param logger Logger instance for output
     * @throws ValueError if parameters are invalid
     * @throws IOError if table/index creation fails
     */
    PaymentTransactionsHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves new payment transaction record.
     */
    void saveRecord(
        const TransactionUUID &transactionUUID,
        BlockNumber maximalClaimingBlockNumber) override;

    /**
     * Updates transaction observing state.
     */
    void updateTransactionState(
        const TransactionUUID &transactionUUID,
        int observingTransactionState) override;

    /**
     * Retrieves transactions with uncertain observing state (state = 0).
     */
    vector<pair<TransactionUUID, BlockNumber>> transactionsWithUncertainObservingState() override;

    /**
     * Retrieves transactions that are still within their claiming period and
     * awaiting observer confirmation (state = 0), ordered by claiming block.
     *
     * @param minBlockNumber Lower bound for maximal_claiming_block_number (exclusive)
     * @param limit Maximum number of records to return
     * @return Vector of (TransactionUUID, BlockNumber) sorted by claiming block
     */
    vector<pair<TransactionUUID, BlockNumber>> transactionsForObserverMonitoring(
        BlockNumber minBlockNumber,
        uint32_t limit) override;

    /**
     * Checks if transaction exists in database.
     */
    bool isTransactionPresent(
        const TransactionUUID &transactionUUID) override;

    /**
     * Deletes transaction record from database.
     */
    void deleteRecord(
        const TransactionUUID &transactionUUID) override;

    /**
     * Retrieves all transaction UUIDs.
     */
    vector<TransactionUUID> allTransactionsUUID() override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};
#endif //VTCPD_PAYMENTTRANSACTIONSHANDLERSQLITE_H
