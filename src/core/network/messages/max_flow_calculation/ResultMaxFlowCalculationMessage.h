#ifndef VTCPD_RESULTMAXFLOWCALCULATIONMESSAGE_H
#define VTCPD_RESULTMAXFLOWCALCULATIONMESSAGE_H

#include "../base/max_flow_calculation/MaxFlowCalculationConfirmationMessage.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../rates/Commission.h"

#include <vector>
#include <optional>

class ResultMaxFlowCalculationMessage:
    public MaxFlowCalculationConfirmationMessage
{

public:
    typedef shared_ptr<ResultMaxFlowCalculationMessage> Shared;

public:
    ResultMaxFlowCalculationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> &outgoingFlows,
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> &incomingFlows,
        Commission::Shared commission = nullptr);

    ResultMaxFlowCalculationMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const bool isAddToConfirmationNotStronglyRequiredMessagesHandler() const override;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

    const vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows() const;

    const vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows() const;

    Commission::Shared commission() const;

private:
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> mOutgoingFlows;
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> mIncomingFlows;
    Commission::Shared mCommission;
};


#endif //VTCPD_RESULTMAXFLOWCALCULATIONMESSAGE_H
