#ifndef VTCPD_HISTORYSTORAGEPOSTGRESQL_H
#define VTCPD_HISTORYSTORAGEPOSTGRESQL_H

#include "../interfaces/HistoryStorage.h"
#include "../../../common/Types.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../record/payment/PaymentRecord.h"
#include "../record/trust_line/TrustLineRecord.h"
#include "../record/payment/PaymentAdditionalRecord.h"
#include "../../../common/time/TimeUtils.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <vector>
#include <memory>
#include <string>

class HistoryStoragePostgreSQL : public HistoryStorage
{
public:
    HistoryStoragePostgreSQL(
        PGconn *dbConnection,
        const std::string &mainTableName,
        const std::string &additionalTableName,
        Logger &logger);

    void saveTrustLineRecord(
        TrustLineRecord::Shared record,
        const SerializedEquivalent equivalent) override;

    void savePaymentRecord(
        PaymentRecord::Shared record) override;

    void savePaymentAdditionalRecord(
        PaymentAdditionalRecord::Shared record,
        const SerializedEquivalent equivalent) override;

    std::vector<TrustLineRecord::Shared> allTrustLineRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent) override;

    std::vector<PaymentRecord::Shared> allPaymentRecords(
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

    std::vector<PaymentRecord::Shared> paymentRecordsAllEquivalents(
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

    std::vector<PaymentRecord::Shared> paymentRecordsByCommandUUID(
        const CommandUUID &commandUUID) override;

    std::vector<PaymentRecord::Shared> paymentRecordsByTransactionUUID(
        const TransactionUUID &transactionUUID) override;

    std::vector<PaymentAdditionalRecord::Shared> allPaymentAdditionalRecords(
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

    std::vector<Record::Shared> recordsWithContractor(
        std::vector<BaseAddress::Shared> contractorAddresses,
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord) override;

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

    std::vector<Record::Shared> recordsPortionWithContractor(
        std::vector<BaseAddress::Shared> contractorAddresses,
        size_t recordsCount,
        size_t fromRecord);

    // Base helpers
    std::vector<PaymentRecord::Shared> allPaymentRecordsBase(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    std::vector<PaymentRecord::Shared> paymentRecordsAllEquivalentsBase(
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    std::vector<PaymentAdditionalRecord::Shared> allPaymentAdditionalRecordsBase(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent);

    TrustLineRecord::Shared deserializeTrustLineRecord(
        PGresult *res,
        int rowIdx);

    PaymentRecord::Shared deserializePaymentRecord(
        PGresult *res,
        int rowIdx);

    PaymentAdditionalRecord::Shared deserializePaymentAdditionalRecord(
        PGresult *res,
        int rowIdx);

    LoggerStream info() const;
    LoggerStream debug() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

private:
    const size_t kPortionRequestSize = 1000;
    PGconn *mDataBase = nullptr;
    std::string mMainTableName;
    std::string mAdditionalTableName;
    Logger &mLog;
};

#endif // VTCPD_HISTORYSTORAGEPOSTGRESQL_H 