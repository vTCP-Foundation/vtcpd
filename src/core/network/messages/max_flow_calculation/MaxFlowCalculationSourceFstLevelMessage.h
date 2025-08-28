#ifndef VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H
#define VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H

#include "../SenderMessage.h"

class MaxFlowCalculationSourceFstLevelMessage:
    public SenderMessage
{

public:
    typedef shared_ptr<MaxFlowCalculationSourceFstLevelMessage> Shared;

public:
    using SenderMessage::SenderMessage;

	MaxFlowCalculationSourceFstLevelMessage(
	 const SerializedEquivalent equivalent,
		ContractorID idOnReceiverSide,
        HopsCount_t hopsCount
	);

	MaxFlowCalculationSourceFstLevelMessage(
	 const SerializedEquivalent equivalent,
		ContractorID idOnReceiverSide,
        HopsCount_t hopsCount,
        vector<SerializedEquivalent> exchangeEquivalents
	);

	MaxFlowCalculationSourceFstLevelMessage(BytesShared buffer);

	pair<BytesShared, size_t> serializeToBytes() const override;

    const MessageType typeID() const override;

	HopsCount_t getHopsCount() const;

	vector<SerializedEquivalent> exchangeEquivalents() const;

private:

	HopsCount_t mHopsCnt;
	vector<SerializedEquivalent> mExchangeEquivalents;
	
};

#endif //VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H
