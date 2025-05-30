#ifndef VTCPD_HISTORYSTORAGE_H
#define VTCPD_HISTORYSTORAGE_H

#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../common/Types.h"
#include "../../logger/Logger.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../common/multiprecision/MultiprecisionUtils.h"
#include "SQLiteStatementRAII.h"

#include "record/payment/PaymentRecord.h"
#include "record/trust_line/TrustLineRecord.h"
#include "record/payment/PaymentAdditionalRecord.h"

#include <sqlite3.h>
#include <vector>
#include <memory>

/**
 * Manages history storage operations for trust line and payment records in SQLite database.
 *
 * Provides functionality to:
 * - Store and retrieve trust line history records
 * - Store and retrieve payment records (main and additional)
 * - Query records with filtering by time, amount, and contractor
 * - Check operation existence and prevent duplicates
 *
 * Creates two tables:
 * - Main table: stores trust line and payment records for frontend
 * - Additional table: stores additional payment records for statistics
 */
class HistoryStorage
{

public:
    /**
     * Constructs HistoryStorage and initializes database tables with indexes.
     *
     * @param dbConnection SQLite database connection (must not be null)
     * @param mainTableName Name for the main history table
     * @param additionalTableName Name for the additional history table
     * @param logger Logger instance for debugging and error reporting
     * @throws ValueError if dbConnection is null or table names are empty
     * @throws IOError if database operations fail
     */
    HistoryStorage(
        sqlite3 *dbConnection,
        const string &mainTableName,
        const string &additionalTableName,
        Logger &logger);

    /**
     * Saves a trust line record to the main table.
     *
     * @param record Trust line record to save (must not be null)
     * @param equivalent Equivalent identifier
     * @throws ValueError if record is null
     * @throws IOError if database operation fails
     */
    void saveTrustLineRecord(
        TrustLineRecord::Shared record,
        const SerializedEquivalent equivalent);

    /**
     * Saves a payment record to the main table.
     *
     * @param record Payment record to save (must not be null)
     * @throws ValueError if record is null or has invalid type
     * @throws IOError if database operation fails
     */
    void savePaymentRecord(
        PaymentRecord::Shared record);

    /**
     * Saves a payment additional record to the additional table.
     *
     * @param record Payment additional record to save (must not be null)
     * @param equivalent Equivalent identifier
     * @throws ValueError if record is null
     * @throws IOError if database operation fails
     */
    void savePaymentAdditionalRecord(
        PaymentAdditionalRecord::Shared record,
        const SerializedEquivalent equivalent);

    /**
     * Retrieves trust line records with optional time filtering.
     *
     * @param equivalent Equivalent identifier
     * @param recordsCount Maximum number of records to retrieve
     * @param fromRecord Offset for pagination
     * @param timeFrom Start time filter (if isTimeFromPresent is true)
     * @param isTimeFromPresent Whether to apply timeFrom filter
     * @param timeTo End time filter (if isTimeToPresent is true)
     * @param isTimeToPresent Whether to apply timeTo filter
     * @return Vector of trust line records ordered by timestamp (newest first)
     * @throws IOError if database operation fails
     */
    vector<TrustLineRecord::Shared> allTrustLineRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    /**
     * Retrieves payment records with optional time and amount filtering.
     *
     * @param equivalent Equivalent identifier
     * @param recordsCount Maximum number of records to retrieve
     * @param fromRecord Offset for pagination
     * @param timeFrom Start time filter (if isTimeFromPresent is true)
     * @param isTimeFromPresent Whether to apply timeFrom filter
     * @param timeTo End time filter (if isTimeToPresent is true)
     * @param isTimeToPresent Whether to apply timeTo filter
     * @param lowBoundaryAmount Minimum amount filter (if isLowBoundaryAmountPresent is true)
     * @param isLowBoundaryAmountPresent Whether to apply lowBoundaryAmount filter
     * @param highBoundaryAmount Maximum amount filter (if isHighBoundaryAmountPresent is true)
     * @param isHighBoundaryAmountPresent Whether to apply highBoundaryAmount filter
     * @return Vector of payment records ordered by timestamp (newest first)
     * @throws IOError if database operation fails
     */
    vector<PaymentRecord::Shared> allPaymentRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent,
        const TrustLineAmount& lowBoundaryAmount,
        bool isLowBoundaryAmountPresent,
        const TrustLineAmount& highBoundaryAmount,
        bool isHighBoundaryAmountPresent);

    /**
     * Retrieves payment records across all equivalents with optional filtering.
     *
     * @param recordsCount Maximum number of records to retrieve
     * @param fromRecord Offset for pagination
     * @param timeFrom Start time filter (if isTimeFromPresent is true)
     * @param isTimeFromPresent Whether to apply timeFrom filter
     * @param timeTo End time filter (if isTimeToPresent is true)
     * @param isTimeToPresent Whether to apply timeTo filter
     * @param lowBoundaryAmount Minimum amount filter (if isLowBoundaryAmountPresent is true)
     * @param isLowBoundaryAmountPresent Whether to apply lowBoundaryAmount filter
     * @param highBoundaryAmount Maximum amount filter (if isHighBoundaryAmountPresent is true)
     * @param isHighBoundaryAmountPresent Whether to apply highBoundaryAmount filter
     * @return Vector of payment records ordered by timestamp (newest first)
     * @throws IOError if database operation fails
     */
    vector<PaymentRecord::Shared> paymentRecordsAllEquivalents(
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent,
        const TrustLineAmount& lowBoundaryAmount,
        bool isLowBoundaryAmountPresent,
        const TrustLineAmount& highBoundaryAmount,
        bool isHighBoundaryAmountPresent);

    /**
     * Retrieves payment records by command UUID.
     *
     * @param commandUUID Command UUID to search for
     * @return Vector of matching payment records
     * @throws IOError if database operation fails
     */
    vector<PaymentRecord::Shared> paymentRecordsByCommandUUID(
        const CommandUUID &commandUUID);

    /**
     * Retrieves payment records by transaction UUID.
     *
     * @param transactionUUID Transaction UUID to search for
     * @return Vector of matching payment records
     * @throws IOError if database operation fails
     */
    vector<PaymentRecord::Shared> paymentRecordsByTransactionUUID(
        const TransactionUUID &transactionUUID);

    /**
     * Retrieves payment additional records with optional filtering.
     *
     * @param equivalent Equivalent identifier
     * @param recordsCount Maximum number of records to retrieve
     * @param fromRecord Offset for pagination
     * @param timeFrom Start time filter (if isTimeFromPresent is true)
     * @param isTimeFromPresent Whether to apply timeFrom filter
     * @param timeTo End time filter (if isTimeToPresent is true)
     * @param isTimeToPresent Whether to apply timeTo filter
     * @param lowBoundaryAmount Minimum amount filter (if isLowBoundaryAmountPresent is true)
     * @param isLowBoundaryAmountPresent Whether to apply lowBoundaryAmount filter
     * @param highBoundaryAmount Maximum amount filter (if isHighBoundaryAmountPresent is true)
     * @param isHighBoundaryAmountPresent Whether to apply highBoundaryAmount filter
     * @return Vector of payment additional records ordered by timestamp (newest first)
     * @throws IOError if database operation fails
     */
    vector<PaymentAdditionalRecord::Shared> allPaymentAdditionalRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent,
        const TrustLineAmount& lowBoundaryAmount,
        bool isLowBoundaryAmountPresent,
        const TrustLineAmount& highBoundaryAmount,
        bool isHighBoundaryAmountPresent);

    /**
     * Retrieves records filtered by contractor addresses.
     *
     * @param contractorAddresses Vector of contractor addresses to match
     * @param equivalent Equivalent identifier
     * @param recordsCount Maximum number of records to retrieve
     * @param fromRecord Offset for pagination
     * @return Vector of records matching the contractor addresses
     * @throws IOError if database operation fails
     */
    vector<Record::Shared> recordsWithContractor(
        vector<BaseAddress::Shared> contractorAddresses,
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord);

    /**
     * Checks if an operation was previously conducted.
     *
     * @param transactionUUID Transaction UUID to check
     * @return true if operation exists, false otherwise
     * @throws IOError if database operation fails
     */
    bool whetherOperationWasConducted(
        const TransactionUUID &transactionUUID);

private:
    void savePaymentMainOutgoingRecord(
        PaymentRecord::Shared record);

    void savePaymentMainIncomingRecord(
        PaymentRecord::Shared record);

    vector<PaymentRecord::Shared> allPaymentRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    vector<PaymentRecord::Shared> paymentRecordsAllEquivalents(
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    vector<PaymentAdditionalRecord::Shared> allPaymentAdditionalRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    size_t countRecordsByType(
        Record::RecordType recordType,
        const SerializedEquivalent equivalent);

    size_t countRecordsByTypeAllEquivalents(
        Record::RecordType recordType);

    vector<Record::Shared> recordsPortionWithContractor(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord);

    TrustLineRecord::Shared deserializeTrustLineRecord(
        sqlite3_stmt *stmt);

    PaymentRecord::Shared deserializePaymentRecord(
        const SerializedEquivalent equivalent,
        sqlite3_stmt *stmt);

    PaymentAdditionalRecord::Shared deserializePaymentAdditionalRecord(
        sqlite3_stmt *stmt);

    LoggerStream info() const;

    LoggerStream debug() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    const size_t kPortionRequestSize = 1000;

private:
    sqlite3 *mDataBase = nullptr;
    string mMainTableName;
    string mAdditionalTableName;
    Logger &mLog;
};


#endif //VTCPD_HISTORYSTORAGE_H
