#ifndef VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELMESSAGE_H
#define VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELMESSAGE_H

#include "../base/max_flow_calculation/MaxFlowCalculationMessage.h"

class MaxFlowCalculationTargetSndLevelMessage : public MaxFlowCalculationMessage
{

public:
    typedef shared_ptr<MaxFlowCalculationTargetSndLevelMessage> Shared;

public:
    MaxFlowCalculationTargetSndLevelMessage(
        const SerializedEquivalent equivalent,
        ContractorID idOnReceiverSide,
        vector<BaseAddress::Shared> targetAddresses,
        bool isTargetGateway);

    MaxFlowCalculationTargetSndLevelMessage(
        const SerializedEquivalent equivalent,
        ContractorID idOnReceiverSide,
        vector<BaseAddress::Shared> targetAddresses,
        bool isTargetGateway,
        vector<SerializedEquivalent> exchangeEquivalents);

    MaxFlowCalculationTargetSndLevelMessage(
        BytesShared buffer);

    bool isTargetGateway() const;

    const MessageType typeID() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    bool mIsTargetGateway;
};


#endif //VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELMESSAGE_H
