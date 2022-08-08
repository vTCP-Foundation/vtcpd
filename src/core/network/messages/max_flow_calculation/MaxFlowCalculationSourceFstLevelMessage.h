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
        bool isSenderGateway,
        uint8_t hopsCount
	);

	MaxFlowCalculationSourceFstLevelMessage(BytesShared buffer);

	pair<BytesShared, size_t> serializeToBytes() const override;

    const MessageType typeID() const override;

	uint8_t getHopsCount() const;

private:

	bool mIsSenderGateway;
	uint8_t mHopsCnt;
	
};

#endif //GEO_NETWORK_CLIENT_MAXFLOWCALCULATIONSOURCEFSTLEVELMESSAGE_H
