#ifndef VTCPD_INCOMINGPAYMENTRECEIPTHANDLER_H
#define VTCPD_INCOMINGPAYMENTRECEIPTHANDLER_H

#include "../../logger/Logger.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../common/multiprecision/MultiprecisionUtils.h"
#include "../../crypto/lamportscheme.h"
#include "SQLiteStatementRAII.h"
#include "record/audit/ReceiptRecord.h"

#include <sqlite3.h>

using namespace crypto::lamport;

/**
 * Manages incoming payment receipt records in SQLite database.
 *
 * Provides functionality to:
 * - Store and retrieve payment receipt records with audit information
 * - Query receipts by audit number and trust line
 * - Delete receipts by various criteria (transaction, trust line, key hash)
 * - Check receipt existence by key hash or transaction UUID
 *
 * Creates a table with foreign key constraints to trust_lines and contractor_keys tables.
 */
class IncomingPaymentReceiptHandler
{

public:
    /**
     * Constructs IncomingPaymentReceiptHandler and initializes the database table.
     *
     * Creates the receipt table with schema:
     * - trust_line_id: INTEGER NOT NULL (foreign key to trust_lines)
     * - audit_number: INTEGER NOT NULL
     * - transaction_uuid: BLOB NOT NULL
     * - contractor_public_key_hash: BLOB NOT NULL (foreign key to contractor_keys)
     * - amount: BLOB NOT NULL
     * - contractor_signature: BLOB NOT NULL
     *
     * Also creates necessary indexes for efficient querying.
     *
     * @param dbConnection SQLite database connection (must not be null)
     * @param tableName Name of the table to create/use (must not be empty)
     * @param logger Logger instance for debugging and error reporting
     * @throws ValueError if dbConnection is null or tableName is empty
     * @throws IOError if database operations fail
     */
    IncomingPaymentReceiptHandler(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves a payment receipt record to the database.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Audit sequence number
     * @param transactionUUID Transaction UUID
     * @param contractorPublicKeyHash Hash of contractor's public key (must not be null)
     * @param amount Payment amount
     * @param contractorSignature Contractor's signature (must not be null)
     * @throws ValueError if contractorPublicKeyHash or contractorSignature is null
     * @throws IOError if database operation fails
     */
    void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared contractorPublicKeyHash,
        const TrustLineAmount &amount,
        const Signature::Shared contractorSignature);

    /**
     * Retrieves audit amounts for a specific trust line and audit number.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Audit sequence number
     * @return Vector of transaction UUID and amount pairs
     * @throws IOError if database operation fails
     */
    vector<pair<TransactionUUID, TrustLineAmount>> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber);

    /**
     * Retrieves all receipt records for a specific audit number.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Audit sequence number
     * @return Vector of receipt records
     * @throws IOError if database operation fails
     */
    vector<ReceiptRecord::Shared> receiptsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber);

    /**
     * Retrieves receipt records with audit numbers less than or equal to specified number.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Maximum audit sequence number (inclusive)
     * @return Vector of receipt records
     * @throws IOError if database operation fails
     */
    vector<ReceiptRecord::Shared> receiptsLessEqualThanAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber);

    /**
     * Counts receipt records for a specific audit number.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Audit sequence number
     * @return Number of receipt records
     * @throws IOError if database operation fails
     */
    uint32_t countReceiptsByNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber);

    /**
     * Deletes receipt records by transaction UUID.
     *
     * @param transactionUUID Transaction UUID to delete
     * @throws IOError if database operation fails
     */
    void deleteRecords(
        const TransactionUUID &transactionUUID);

    /**
     * Deletes all receipt records for a trust line.
     *
     * @param trustLineID Trust line identifier
     * @throws IOError if database operation fails
     */
    void deleteRecords(
        const TrustLineID trustLineID);

    /**
     * Deletes receipt records by contractor key hash.
     *
     * @param keyHash Key hash to delete records for (must not be null)
     * @throws ValueError if keyHash is null
     * @throws IOError if database operation fails
     */
    void deleteRecords(
        KeyHash::Shared keyHash);

    /**
     * Checks if any receipt contains the specified key hash.
     *
     * @param keyHash Key hash to search for (must not be null)
     * @return true if key hash exists in receipts, false otherwise
     * @throws ValueError if keyHash is null
     * @throws IOError if database operation fails
     */
    bool isContainsKeyHash(
        KeyHash::Shared keyHash) const;

    /**
     * Checks if any receipt contains the specified transaction UUID.
     *
     * @param transactionUUID Transaction UUID to search for
     * @return true if transaction exists in receipts, false otherwise
     * @throws IOError if database operation fails
     */
    bool isContainsTransaction(
        const TransactionUUID &transactionUUID) const;

private:
    LoggerStream info() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};


#endif //VTCPD_INCOMINGPAYMENTRECEIPTHANDLER_H
