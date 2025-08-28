#ifndef VTCPD_EXCHANGERATESMESSAGE_H
#define VTCPD_EXCHANGERATESMESSAGE_H

#include "../SenderMessage.h"
#include "../../../rates/ExchangeRate.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../common/time/TimeUtils.h"

#include <vector>
#include <memory>

using namespace std;

class ExchangeRatesMessage : public SenderMessage
{
public:
    typedef shared_ptr<ExchangeRatesMessage> Shared;

public:
    ExchangeRatesMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        vector<ExchangeRate::Shared> exchangeRates);

    ExchangeRatesMessage(
        BytesShared buffer);

    vector<ExchangeRate::Shared> exchangeRates() const;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

    virtual const MessageType typeID() const override;

    const size_t kOffsetToInheritedBytes() const override;

private:
    vector<ExchangeRate::Shared> mExchangeRates;
};

#endif //VTCPD_EXCHANGERATESMESSAGE_H