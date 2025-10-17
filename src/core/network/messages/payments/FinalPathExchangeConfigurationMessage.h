#ifndef VTCPD_FINALPATHEXCHANGECONFIGURATIONMESSAGE_H
#define VTCPD_FINALPATHEXCHANGECONFIGURATIONMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"

class FinalPathExchangeConfigurationMessage : public TransactionMessage
{

public:
    typedef shared_ptr<FinalPathExchangeConfigurationMessage> Shared;
    typedef shared_ptr<const FinalPathExchangeConfigurationMessage> ConstShared;

public:
    FinalPathExchangeConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PathID &pathID,
        const TrustLineAmount &incomingAmount,
        const SerializedEquivalent &incomingEquivalent,
        const TrustLineAmount &outgoingAmount,
        const SerializedEquivalent &outgoingEquivalent);

    FinalPathExchangeConfigurationMessage(
        BytesShared buffer);

    const PathID& pathID() const;
    const TrustLineAmount& incomingAmount() const;
    const SerializedEquivalent& incomingEquivalent() const;
    const TrustLineAmount& outgoingAmount() const;
    const SerializedEquivalent& outgoingEquivalent() const;

protected:
    const MessageType typeID() const override;
    pair<BytesShared, size_t> serializeToBytes() const override;
    const size_t kOffsetToInheritedBytes() const override;

protected:
    PathID mPathID;
    TrustLineAmount mIncomingAmount;
    SerializedEquivalent mIncomingEquivalent;
    TrustLineAmount mOutgoingAmount;
    SerializedEquivalent mOutgoingEquivalent;
};


#endif //VTCPD_FINALPATHEXCHANGECONFIGURATIONMESSAGE_H
