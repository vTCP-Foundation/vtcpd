#ifndef VTCPD_INCOMINGPAYMENTRECEIPTHANDLERSQLITE_H
#define VTCPD_INCOMINGPAYMENTRECEIPTHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/IncomingPaymentReceiptHandler.h"
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
 * Manages incoming payment receipt records in SQLite database.
 *
 * Provides functionality to:
 * - Store and retrieve payment receipt records with audit information
 * - Query receipts by audit number and trust line
 * - Delete receipts by various criteria (transaction, trust line, key hash)
 * - Check receipt existence by key hash or transaction UUID
 * Creates a table with foreign key constraints to trust_lines and contractor_keys tables.
 */
class IncomingPaymentReceiptHandlerSQLite : public IncomingPaymentReceiptHandler
{
public:
    IncomingPaymentReceiptHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves a payment receipt record to the database.
     */
    void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared contractorPublicKeyHash,
        const TrustLineAmount &amount,
        const Signature::Shared contractorSignature) override;

    /**
     * Retrieves audit amounts for a specific trust line and audit number.
     */
    map<TransactionUUID, TrustLineAmount> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    /**
     * Retrieves all receipt records for a specific audit number.
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
     * Deletes all receipt records for a transaction.
     */
    void deleteRecords(
        const TransactionUUID &transactionUUID) override;

    /**
     * Deletes all receipt records for a trust line.
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
     * Checks if any receipt contains the specified key hash.
     */
    bool isContainsKeyHash(
        KeyHash::Shared keyHash) override;

    /**
     * Checks if any receipt contains the specified transaction UUID.
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

#endif //VTCPD_INCOMINGPAYMENTRECEIPTHANDLERSQLITE_H
