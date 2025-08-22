#ifndef VTCPD_EXCHANGERATE_H
#define VTCPD_EXCHANGERATE_H

#include "../common/Types.h"
#include "../common/time/TimeUtils.h"

#include <memory>

using namespace std;

class ExchangeRate
{
public:
    typedef shared_ptr<ExchangeRate> Shared;

public:
    ExchangeRate(
        const SerializedEquivalent equivalentFrom,
        const SerializedEquivalent equivalentTo,
        const TrustLineAmount &exchangeRate,
        const int16_t exchangeRateShift,
        const DateTime &expiresAt,
        const TrustLineAmount &minExchangeAmount,
        const TrustLineAmount &maxExchangeAmount);

    const SerializedEquivalent equivalentFrom() const;

    const SerializedEquivalent equivalentTo() const;

    const TrustLineAmount& exchangeRate() const;

    const int16_t exchangeRateShift() const;

    const DateTime& expiresAt() const;

    const TrustLineAmount& minExchangeAmount() const;

    const TrustLineAmount& maxExchangeAmount() const;

    void update(
        const TrustLineAmount &exchangeRate,
        const int16_t exchangeRateShift,
        const TrustLineAmount &minExchangeAmount,
        const TrustLineAmount &maxExchangeAmount,
        const DateTime &expiresAt);

private:
    SerializedEquivalent mEquivalentFrom;
    SerializedEquivalent mEquivalentTo;
    TrustLineAmount mExchangeRate;
    int16_t mExchangeRateShift;
    DateTime mExpiresAt;
    TrustLineAmount mMinExchangeAmount;
    TrustLineAmount mMaxExchangeAmount;
};

#endif //VTCPD_EXCHANGERATE_H