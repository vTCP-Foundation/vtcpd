#ifndef VTCPD_OUTGOINGPAYMENTRECEIPTHANDLER_H
#define VTCPD_OUTGOINGPAYMENTRECEIPTHANDLER_H

#include "../../logger/Logger.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../common/multiprecision/MultiprecisionUtils.h"
#include "../../crypto/lamportscheme.h"
#include "record/audit/ReceiptRecord.h"
#include "SQLiteStatementRAII.h"

#include <sqlite3.h>

using namespace crypto::lamport;

/**
 * Handles storage and retrieval of outgoing payment receipts.
 * Manages audit data for payments sent from this node.
 */
class OutgoingPaymentReceiptHandler
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
    OutgoingPaymentReceiptHandler(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves an outgoing payment receipt record.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Audit sequence number
     * @param transactionUUID Transaction identifier
     * @param ownPublicKeyHash Public key hash used for signing
     * @param amount Payment amount
     * @throws ValueError if ownPublicKeyHash is null
     * @throws IOError if database operation fails
     */
    void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared ownPublicKeyHash,
        const TrustLineAmount &amount);

    /**
     * Retrieves transaction UUIDs and amounts for specific audit number.
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
     * Retrieves receipt records for specific audit number.
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
     * Retrieves receipt records for audit numbers up to specified value.
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
     * Counts receipts for specific audit number.
     *
     * @param trustLineID Trust line identifier
     * @param auditNumber Audit sequence number
     * @return Number of receipts
     * @throws IOError if database operation fails
     */
    uint32_t countReceiptsByNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber);

    /**
     * Deletes all records for specific transaction.
     *
     * @param transactionUUID Transaction identifier
     * @throws IOError if database operation fails
     */
    void deleteRecords(
        const TransactionUUID &transactionUUID);

    /**
     * Deletes all records for specific trust line.
     *
     * @param trustLineID Trust line identifier
     * @throws IOError if database operation fails
     */
    void deleteRecords(
        const TrustLineID trustLineID);

    /**
     * Deletes all records with specific key hash.
     *
     * @param keyHash Public key hash to match
     * @throws ValueError if keyHash is null
     * @throws IOError if database operation fails
     */
    void deleteRecords(
        KeyHash::Shared keyHash);

    /**
     * Checks if any records exist with specified key hash.
     *
     * @param keyHash Public key hash to search for
     * @return True if records exist, false otherwise
     * @throws ValueError if keyHash is null
     * @throws IOError if database operation fails
     */
    bool isContainsKeyHash(
        KeyHash::Shared keyHash) const;

    /**
     * Checks if record exists for specific transaction.
     *
     * @param transactionUUID Transaction identifier to search for
     * @return True if record exists, false otherwise
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

#endif //VTCPD_OUTGOINGPAYMENTRECEIPTHANDLER_H
