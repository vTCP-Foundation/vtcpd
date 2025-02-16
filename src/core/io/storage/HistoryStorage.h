#ifndef VTCPD_HISTORYSTORAGE_H
#define VTCPD_HISTORYSTORAGE_H

#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../common/Types.h"
#include "../../logger/Logger.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/multiprecision/MultiprecisionUtils.h"

#include "record/payment/PaymentRecord.h"
#include "record/trust_line/TrustLineRecord.h"
#include "record/payment/PaymentAdditionalRecord.h"

#include "../../../libs/sqlite3/sqlite3.h"

#include <vector>
#include <memory>

class HistoryStorage
{

public:
    HistoryStorage(
        sqlite3 *dbConnection,
        const string &mainTableName,
        const string &additionalTableName,
        Logger &logger);

    void saveTrustLineRecord(
        TrustLineRecord::Shared record,
        const SerializedEquivalent equivalent);

    void savePaymentRecord(
        PaymentRecord::Shared record);

    void savePaymentAdditionalRecord(
        PaymentAdditionalRecord::Shared record,
        const SerializedEquivalent equivalent);

    vector<TrustLineRecord::Shared> allTrustLineRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

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

    vector<PaymentRecord::Shared> paymentRecordsByCommandUUID(
        const CommandUUID &commandUUID);

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

    vector<Record::Shared> recordsWithContractor(
        vector<BaseAddress::Shared> contractorAddresses,
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord);

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
    // main table used for storing history, needed for frontend
    // (trustlines, payments coordinator, payments receiver)
    string mMainTableName;
    // additional table used for storing history, needed for statistics
    // (cycles and payment intermediate nodes)
    string mAdditionalTableName;
    Logger &mLog;
};


#endif //VTCPD_HISTORYSTORAGE_H
