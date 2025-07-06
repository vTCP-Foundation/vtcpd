#ifndef VTCPD_TRANSACTIONSHANDLERSQLITE_H
#define VTCPD_TRANSACTIONSHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/TransactionsHandler.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/Types.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <vector>

/**
 * Handles persistent storage of serialized transactions in SQLite database.
 * Provides CRUD operations with proper validation and error handling.
 */
class TransactionsHandlerSQLite : public TransactionsHandler
{
public:
    /**
     * Initializes handler and creates table with unique index.
     * @param dbConnection Valid SQLite database connection
     * @param tableName Non-empty table name
     * @param logger Logger instance for debug output
     * @throws ValueError if parameters are invalid
     * @throws IOError if table/index creation fails
     */
    TransactionsHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves transaction with INSERT OR REPLACE semantics.
     * @param transactionUUID Unique transaction identifier
     * @param transaction Non-null transaction data
     * @param transactionBytesCount Non-zero byte count
     * @throws IOError if database operation fails
     */
    void saveRecord(
        const TransactionUUID &transactionUUID,
        BytesShared transaction,
        size_t transactionBytesCount);

    /**
     * Retrieves transaction data by UUID.
     * @param transactionUUID Transaction identifier
     * @return Shared pointer to transaction bytes
     * @throws NotFoundError if transaction not found
     */
    BytesShared getTransaction(
        const TransactionUUID &transactionUUID);

    /**
     * Checks if transaction exists in database.
     * @return true if transaction exists, false otherwise
     */
    bool isTransactionSerialized(
        const TransactionUUID &transactionUUID);

    /**
     * Removes transaction from database if it exists. 
     * Coordinator does not save transactions, but it deletes them from storage 
     * because this logic realized in BasePaymentTransaction.
     */
    void deleteRecordIfExists(
        const TransactionUUID &transactionUUID);

    /**
     * Retrieves all stored transactions.
     * @return Vector of transaction data
     */
    vector<BytesShared> allTransactions();

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_TRANSACTIONSHANDLERSQLITE_H
