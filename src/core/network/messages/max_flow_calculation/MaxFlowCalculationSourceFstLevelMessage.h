#ifndef GEO_NETWORK_CLIENT_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H
#define GEO_NETWORK_CLIENT_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H

#include "../SenderMessage.h"

class MaxFlowCalculationSourceFstLevelMessage:
    public SenderMessage {

public:
    typedef shared_ptr<MaxFlowCalculationSourceFstLevelMessage> Shared;

public:
    using SenderMessage::SenderMessage;

	MaxFlowCalculationSourceFstLevelMessage(
	 const SerializedEquivalent equivalent,
		ContractorID idOnReceiverSide,
        HopsCount_t hopsCount
	);

	MaxFlowCalculationSourceFstLevelMessage(BytesShared buffer);

	pair<BytesShared, size_t> serializeToBytes() const override;

    const MessageType typeID() const override;

	HopsCount_t getHopsCount() const;

private:

	HopsCount_t mHopsCnt;
	
};

#endif //GEO_NETWORK_CLIENT_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H
