#ifndef VTCPD_MAXFLOWCALCULATIONMESSAGE_H
#define VTCPD_MAXFLOWCALCULATIONMESSAGE_H

#include "../../SenderMessage.h"

#include "../../../../common/Types.h"
#include "../../../../common/memory/MemoryUtils.h"

#include "../../../../transactions/transactions/base/TransactionUUID.h"

#include <memory>
#include <utility>
#include <stdint.h>

using namespace std;

class MaxFlowCalculationMessage : public SenderMessage
{
public:
    typedef shared_ptr<MaxFlowCalculationMessage> Shared;

public:
    MaxFlowCalculationMessage(
        const SerializedEquivalent equivalent,
        ContractorID idOnReceiverSide,
        vector<BaseAddress::Shared> targetAddresses);

    MaxFlowCalculationMessage(
        const SerializedEquivalent equivalent,
        ContractorID idOnReceiverSide,
        vector<BaseAddress::Shared> targetAddresses,
        vector<SerializedEquivalent> exchangeEquivalents);

    MaxFlowCalculationMessage(
        BytesShared buffer);

    vector<BaseAddress::Shared> targetAddresses() const;

    vector<SerializedEquivalent> exchangeEquivalents() const;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

    const size_t kOffsetToInheritedBytes() const override;

protected:
    vector<BaseAddress::Shared> mTargetAddresses;
    vector<SerializedEquivalent> mExchangeEquivalents;
};

#endif //VTCPD_MAXFLOWCALCULATIONMESSAGE_H
