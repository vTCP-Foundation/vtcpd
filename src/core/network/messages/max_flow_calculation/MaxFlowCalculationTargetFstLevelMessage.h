#ifndef VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELMESSAGE_H
#define VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELMESSAGE_H

#include "../base/max_flow_calculation/MaxFlowCalculationMessage.h"


class MaxFlowCalculationTargetFstLevelMessage :
    public MaxFlowCalculationMessage
{

public:
    typedef shared_ptr<MaxFlowCalculationTargetFstLevelMessage> Shared;

public:
    MaxFlowCalculationTargetFstLevelMessage(
        const SerializedEquivalent equivalent,
        ContractorID idOnReceiverSide,
        vector<BaseAddress::Shared> targetAddresses,
        bool isTargetGateway,
        HopsCount_t hopsCount);

    MaxFlowCalculationTargetFstLevelMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    bool isTargetGateway() const;

    const uint8_t getHopsCount() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    bool mIsTargetGateway;
    uint8_t mHopsCnt;
};

#endif //VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELMESSAGE_H
