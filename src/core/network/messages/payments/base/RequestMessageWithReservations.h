#ifndef VTCPD_REQUESTMESSAGEWITHRESERVATIONS_H
#define VTCPD_REQUESTMESSAGEWITHRESERVATIONS_H

#include "../../base/transaction/TransactionMessage.h"
#include "../../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../../transactions/transactions/regular/payments/base/PathReservation.h"

#include <vector>

class RequestMessageWithReservations : public TransactionMessage
{

public:
    typedef shared_ptr<RequestMessageWithReservations> Shared;

public:
    // NEW: Constructor with equivalents
    RequestMessageWithReservations(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const vector<PathReservation> &finalAmountsConfig);

    RequestMessageWithReservations(
        const SerializedEquivalent equivalent,
        ContractorID contractorID,
        const TransactionUUID &transactionUUID,
        const vector<PathReservation> &finalAmountsConfig);

    // DEPRECATED: Constructor without equivalents (uses mEquivalent for all)
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

    const vector<PathReservation> &finalAmountsConfiguration() const;

    // DEPRECATED: For backward compatibility with old payment transactions
    vector<pair<PathID, ConstSharedTrustLineAmount>> finalAmountsConfigurationDeprecated() const;

protected:
    virtual pair<BytesShared, size_t> serializeToBytes() const override;

    const size_t kOffsetToInheritedBytes() const override;

private:
    vector<PathReservation> mFinalAmountsConfiguration;
};


#endif //VTCPD_REQUESTMESSAGEWITHRESERVATIONS_H
