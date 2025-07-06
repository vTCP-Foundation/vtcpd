#ifndef VTCPD_INTERFACES_HISTORYSTORAGE_H
#define VTCPD_INTERFACES_HISTORYSTORAGE_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/time/TimeUtils.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../interface/commands_interface/CommandUUID.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../record/trust_line/TrustLineRecord.h"
#include "../record/payment/PaymentRecord.h"
#include "../record/payment/PaymentAdditionalRecord.h"
#include "../record/base/Record.h"
#include "../../../contractors/addresses/BaseAddress.h"

#include <memory>
#include <vector>

using namespace std;

class HistoryStorage
{
public:
    virtual ~HistoryStorage() = default;

    virtual void saveTrustLineRecord(
        TrustLineRecord::Shared record,
        const SerializedEquivalent equivalent) = 0;

    virtual void savePaymentRecord(
        PaymentRecord::Shared record) = 0;

    virtual void savePaymentAdditionalRecord(
        PaymentAdditionalRecord::Shared record,
        const SerializedEquivalent equivalent) = 0;

    virtual vector<TrustLineRecord::Shared> allTrustLineRecords(
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent) = 0;

    virtual vector<PaymentRecord::Shared> allPaymentRecords(
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
        bool isHighBoundaryAmountPresent) = 0;

    virtual vector<PaymentRecord::Shared> paymentRecordsAllEquivalents(
        size_t recordsCount,
        size_t fromRecord,
        DateTime timeFrom,
        bool isTimeFromPresent,
        DateTime timeTo,
        bool isTimeToPresent,
        const TrustLineAmount& lowBoundaryAmount,
        bool isLowBoundaryAmountPresent,
        const TrustLineAmount& highBoundaryAmount,
        bool isHighBoundaryAmountPresent) = 0;

    virtual vector<PaymentRecord::Shared> paymentRecordsByCommandUUID(
        const CommandUUID &commandUUID) = 0;

    virtual vector<PaymentRecord::Shared> paymentRecordsByTransactionUUID(
        const TransactionUUID &transactionUUID) = 0;

    virtual vector<PaymentAdditionalRecord::Shared> allPaymentAdditionalRecords(
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
        bool isHighBoundaryAmountPresent) = 0;

    virtual vector<Record::Shared> recordsWithContractor(
        vector<BaseAddress::Shared> contractorAddresses,
        const SerializedEquivalent equivalent,
        size_t recordsCount,
        size_t fromRecord) = 0;

    virtual bool whetherOperationWasConducted(
        const TransactionUUID &transactionUUID) = 0;
};

#endif //VTCPD_INTERFACES_HISTORYSTORAGE_H 