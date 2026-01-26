#ifndef VTCPD_OUTGOINGPAYMENTRECEIPTHANDLERSQLITE_H
#define VTCPD_OUTGOINGPAYMENTRECEIPTHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/OutgoingPaymentReceiptHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>

using namespace crypto::sphincs;

/**
 * Handles storage and retrieval of outgoing payment receipts.
 * Manages audit data for payments sent from this node.
 */
class OutgoingPaymentReceiptHandlerSQLite : public OutgoingPaymentReceiptHandler
{
public:
    /**
     * Initializes handler and creates necessary database tables and indexes.
     *
     * @param dbConnection Valid SQLite database connection
     * @param tableName Name for the outgoing receipts table
     * @param logger Logger instance for debug output
     * @throws ValueError if dbConnection is null or tableName is empty
     * @throws IOError if database operations fail
     */
    OutgoingPaymentReceiptHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves an outgoing payment receipt record.
     */
    void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared ownPublicKeyHash,
        const TrustLineAmount &amount) override;

    /**
     * Retrieves transaction UUIDs and amounts for specific audit number.
     */
    map<TransactionUUID, TrustLineAmount> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    /**
     * Retrieves receipt records for specific audit number.
     */
    vector<ReceiptRecord::Shared> receiptsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    /**
     * Retrieves receipt records for audit numbers less than or equal to the specified number.
     */
    vector<ReceiptRecord::Shared> receiptsLessEqualThanAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    /**
     * Retrieves receipts with zero audit number that belong to finalized transactions.
     */
    vector<ReceiptRecord::Shared> getFinalizedReceiptsWithZeroAuditNumber(
        const TrustLineID trustLineID) override;

    /**
     * Updates audit number for receipts identified by transaction UUIDs.
     */
    void updateAuditNumberByTransactionUUIDs(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const vector<TransactionUUID> &transactionUUIDs) override;

    /**
     * Deletes all records for specific transaction.
     */
    void deleteRecords(
        const TransactionUUID &transactionUUID) override;

    /**
     * Deletes all records for specific trust line.
     */
    void deleteRecords(
        const TrustLineID trustLineID) override;

    /**
     * Task 20-01: Deletes all receipt records for a trust line and audit number.
     */
    void deleteRecordsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    /**
     * Checks if any records exist with specified key hash.
     */
    bool isContainsKeyHash(
        KeyHash::Shared keyHash) override;

    /**
     * Checks if record exists for specific transaction.
     */
    bool isContainsTransaction(
        const TransactionUUID &transactionUUID) override;

    /**
     * Counts receipts by audit number.
     */
    size_t countReceiptsByNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_OUTGOINGPAYMENTRECEIPTHANDLERSQLITE_H
