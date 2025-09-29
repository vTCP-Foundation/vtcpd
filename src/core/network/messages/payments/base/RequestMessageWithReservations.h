#ifndef VTCPD_REQUESTMESSAGEWITHRESERVATIONS_H
#define VTCPD_REQUESTMESSAGEWITHRESERVATIONS_H

#include "../../base/transaction/TransactionMessage.h"
#include "../../../../common/multiprecision/MultiprecisionUtils.h"

#include <vector>

class RequestMessageWithReservations : public TransactionMessage
{

public:
    typedef shared_ptr<RequestMessageWithReservations> Shared;

public:
    RequestMessageWithReservations(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig);

    RequestMessageWithReservations(
        const SerializedEquivalent equivalent,
        ContractorID contractorID,
        const TransactionUUID &transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig);

    RequestMessageWithReservations(
        BytesShared buffer);

    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfiguration() const;

protected:
    virtual pair<BytesShared, size_t> serializeToBytes() const override;

    const size_t kOffsetToInheritedBytes() const override;

private:
    vector<pair<PathID, ConstSharedTrustLineAmount>> mFinalAmountsConfiguration;
};


#endif //VTCPD_REQUESTMESSAGEWITHRESERVATIONS_H
