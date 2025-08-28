#ifndef VTCPD_INITIATEMAXFLOWFOREXCHANGECALCULATIONMESSAGE_H
#define VTCPD_INITIATEMAXFLOWFOREXCHANGECALCULATIONMESSAGE_H

#include "../SenderMessage.h"
#include "../../../common/Types.h"

#include <vector>

using namespace std;

class InitiateMaxFlowForExchangeCalculationMessage : public SenderMessage
{
public:
    typedef shared_ptr<InitiateMaxFlowForExchangeCalculationMessage> Shared;

public:
    InitiateMaxFlowForExchangeCalculationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared>& senderAddresses,
        bool isSenderGateway,
        uint8_t hopsCount,
        vector<SerializedEquivalent> exchangeEquivalents);

    InitiateMaxFlowForExchangeCalculationMessage(
        BytesShared buffer);

    bool isSenderGateway() const;

    uint8_t getHopsCount() const;

    vector<SerializedEquivalent> exchangeEquivalents() const;

    virtual const MessageType typeID() const override;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

    const size_t kOffsetToInheritedBytes() const override;

private:
    uint8_t mHopsCount;
    bool mIsSenderGateway;
    vector<SerializedEquivalent> mExchangeEquivalents;
};

#endif //VTCPD_INITIATEMAXFLOWFOREXCHANGECALCULATIONMESSAGE_H