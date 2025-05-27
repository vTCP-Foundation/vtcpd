#ifndef VTCPD_PAYMENTTRANSACTIONSHANDLER_H
#define VTCPD_PAYMENTTRANSACTIONSHANDLER_H

#include "../../logger/Logger.h"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../common/Types.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"

#include <sqlite3.h>

/**
 * Handles storage and retrieval of payment transaction records in SQLite database.
 *
 * Manages transaction UUID tracking with observing states and block numbers for
 * payment processing. Provides CRUD operations with proper error handling and
 * memory management using RAII patterns.
 */
class PaymentTransactionsHandler
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
    PaymentTransactionsHandler(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves new payment transaction record.
     *
     * @param transactionUUID Unique transaction identifier
     * @param maximalClaimingBlockNumber Block number for claiming
     * @throws IOError if database operation fails
     */
    void saveRecord(
        const TransactionUUID &transactionUUID,
        BlockNumber maximalClaimingBlockNumber);

    /**
     * Updates transaction observing state.
     *
     * @param transactionUUID Target transaction identifier
     * @param observingTransactionState New observing state value
     * @throws IOError if database operation fails
     * @throws ValueError if no rows affected
     */
    void updateTransactionState(
        const TransactionUUID &transactionUUID,
        int observingTransactionState);

    /**
     * Retrieves transactions with uncertain observing state (state = 0).
     *
     * @return Vector of transaction UUID and block number pairs
     * @throws IOError if database operation fails
     */
    vector<pair<TransactionUUID, BlockNumber>> transactionsWithUncertainObservingState();

    /**
     * Checks if transaction exists in database.
     *
     * @param transactionUUID Transaction identifier to check
     * @return true if transaction exists, false otherwise
     * @throws IOError if database operation fails
     */
    bool isTransactionPresent(
        const TransactionUUID& transactionUUID);

    /**
     * Deletes transaction record from database.
     *
     * @param transactionUUID Transaction identifier to delete
     * @throws IOError if database operation fails
     */
    void deleteRecord(
        const TransactionUUID& transactionUUID);

    /**
     * Retrieves all transaction UUIDs from database.
     *
     * @return Vector of all transaction UUIDs
     * @throws IOError if database operation fails
     */
    vector<TransactionUUID> allTransactionsUUID();

private:
    LoggerStream info() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};


#endif //VTCPD_PAYMENTTRANSACTIONSHANDLER_H
