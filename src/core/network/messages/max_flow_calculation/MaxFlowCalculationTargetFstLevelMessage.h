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

    bool isTargetGateway() const;

    const MessageType typeID() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

	uint8_t getHopsCount() const;

private:
    bool mIsTargetGateway;
	HopsCount_t mHopsCnt;
};

#endif //VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELMESSAGE_H
