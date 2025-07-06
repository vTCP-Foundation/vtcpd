#ifndef VTCPD_HISTORYSTORAGESQLITE_H
#define VTCPD_HISTORYSTORAGESQLITE_H

#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../interfaces/HistoryStorage.h"
#include "../../../common/Types.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
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
 * Creates two tables:
 * - Main table: stores trust line and payment records for frontend
 * - Additional table: stores additional payment records for statistics
 */
class HistoryStorageSQLite : public HistoryStorage
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
    HistoryStorageSQLite(
        sqlite3 *dbConnection,
        const string &mainTableName,
        const string &additionalTableName,
        Logger &logger);

    /**
     * Saves a trust line record to the main table.
     * @param record Trust line record to save (must not be null)
     * @param equivalent Equivalent identifier
     * @throws ValueError if record is null
     * @throws IOError if database operation fails
     */
    void saveTrustLineRecord(
        TrustLineRecord::Shared record,
        const SerializedEquivalent equivalent) override;

    /**
     * Saves a payment record to the main table.
     * @param record Payment record to save (must not be null)
     * @throws ValueError if record is null or has invalid type
     */
    void savePaymentRecord(
        PaymentRecord::Shared record) override;

    /**
     * Saves a payment additional record to the additional table.
     * @param record Payment additional record to save (must not be null)
     */
    void savePaymentAdditionalRecord(
        PaymentAdditionalRecord::Shared record,
        const SerializedEquivalent equivalent) override;

    /**
     * Retrieves trust line records with optional time filtering.
     * @param recordsCount Maximum number of records to retrieve
     * @param fromRecord Offset for pagination
     * @param timeFrom Start time filter (if isTimeFromPresent is true)
     * @param isTimeFromPresent Whether to apply timeFrom filter
     * @param timeTo End time filter (if isTimeToPresent is true)
     * @param isTimeToPresent Whether to apply timeTo filter
     * @return Vector of trust line records ordered by timestamp (newest first)
     */
    vector<TrustLineRecord::Shared> allTrustLineRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent) override;

    /**
     * Retrieves payment records with optional time and amount filtering.
     * @param lowBoundaryAmount Minimum amount filter (if isLowBoundaryAmountPresent is true)
     * @param isLowBoundaryAmountPresent Whether to apply lowBoundaryAmount filter
     * @param highBoundaryAmount Maximum amount filter (if isHighBoundaryAmountPresent is true)
     * @param isHighBoundaryAmountPresent Whether to apply highBoundaryAmount filter
     * @return Vector of payment records ordered by timestamp (newest first)
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
        bool isHighBoundaryAmountPresent) override;

    /**
     * Retrieves payment records across all equivalents with optional filtering.
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
        bool isHighBoundaryAmountPresent) override;

    /**
     * Retrieves payment records by command UUID.
     * @param commandUUID Command UUID to search for
     * @return Vector of matching payment records
     */
    vector<PaymentRecord::Shared> paymentRecordsByCommandUUID(
        const CommandUUID &commandUUID) override;

    /**
     * Retrieves payment records by transaction UUID.
     * @param transactionUUID Transaction UUID to search for
     */
    vector<PaymentRecord::Shared> paymentRecordsByTransactionUUID(
        const TransactionUUID &transactionUUID) override;

    /**
     * Retrieves payment additional records with optional filtering.
     * @return Vector of payment additional records ordered by timestamp (newest first)
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
        bool isHighBoundaryAmountPresent) override;

    /**
     * Retrieves records filtered by contractor addresses.
     * @param contractorAddresses Vector of contractor addresses to match
     * @return Vector of records matching the contractor addresses
     */
    vector<Record::Shared> recordsWithContractor(
        vector<BaseAddress::Shared> contractorAddresses,
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord) override;

    /**
     * Checks if an operation was previously conducted.
     * @param transactionUUID Transaction UUID to check
     * @return true if operation exists, false otherwise
     */
    bool whetherOperationWasConducted(
        const TransactionUUID &transactionUUID) override;

private:
    void savePaymentMainOutgoingRecord(
        PaymentRecord::Shared record,
        const SerializedEquivalent equivalent);

    void savePaymentMainIncomingRecord(
        PaymentRecord::Shared record,
        const SerializedEquivalent equivalent);

    size_t countRecordsByType(
        Record::RecordType recordType,
        const SerializedEquivalent equivalent);

    size_t countRecordsByTypeAllEquivalents(
        Record::RecordType recordType);

    vector<Record::Shared> recordsPortionWithContractor(
        vector<BaseAddress::Shared> contractorAddresses,
        size_t recordsCount,
        size_t fromRecord);

    // Base methods for internal use
    vector<PaymentRecord::Shared> allPaymentRecordsBase(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    vector<PaymentRecord::Shared> paymentRecordsAllEquivalentsBase(
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    vector<PaymentAdditionalRecord::Shared> allPaymentAdditionalRecordsBase(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    TrustLineRecord::Shared deserializeTrustLineRecord(
        sqlite3_stmt *stmt);

    PaymentRecord::Shared deserializePaymentRecord(
        sqlite3_stmt *stmt);

    PaymentAdditionalRecord::Shared deserializePaymentAdditionalRecord(
        sqlite3_stmt *stmt);

    LoggerStream info() const;
    LoggerStream debug() const;
    LoggerStream warning() const;
    const string logHeader() const;

    const size_t kPortionRequestSize = 1000;
    sqlite3 *mDataBase = nullptr;
    string mMainTableName;
    string mAdditionalTableName;
    Logger &mLog;
};

#endif //VTCPD_HISTORYSTORAGESQLITE_H
