#include "ExchangeRate.h"

ExchangeRate::ExchangeRate(
    const SerializedEquivalent equivalentFrom,
    const SerializedEquivalent equivalentTo,
    const TrustLineAmount &exchangeRate,
    const int16_t exchangeRateShift,
    const DateTime &expiresAt,
    const TrustLineAmount &minExchangeAmount,
    const TrustLineAmount &maxExchangeAmount):

    mEquivalentFrom(equivalentFrom),
    mEquivalentTo(equivalentTo),
    mExchangeRate(exchangeRate),
    mExchangeRateShift(exchangeRateShift),
    mExpiresAt(expiresAt),
    mMinExchangeAmount(minExchangeAmount),
    mMaxExchangeAmount(maxExchangeAmount)
{
}

const SerializedEquivalent ExchangeRate::equivalentFrom() const
{
    return mEquivalentFrom;
}

const SerializedEquivalent ExchangeRate::equivalentTo() const
{
    return mEquivalentTo;
}

const TrustLineAmount& ExchangeRate::exchangeRate() const
{
    return mExchangeRate;
}

const int16_t ExchangeRate::exchangeRateShift() const
{
    return mExchangeRateShift;
}

const DateTime& ExchangeRate::expiresAt() const
{
    return mExpiresAt;
}

const TrustLineAmount& ExchangeRate::minExchangeAmount() const
{
    return mMinExchangeAmount;
}

const TrustLineAmount& ExchangeRate::maxExchangeAmount() const
{
    return mMaxExchangeAmount;
}

void ExchangeRate::update(
    const TrustLineAmount &exchangeRate,
    const int16_t exchangeRateShift,
    const TrustLineAmount &minExchangeAmount,
    const TrustLineAmount &maxExchangeAmount,
    const DateTime &expiresAt)
{
    mExchangeRate = exchangeRate;
    mExchangeRateShift = exchangeRateShift;
    mMinExchangeAmount = minExchangeAmount;
    mMaxExchangeAmount = maxExchangeAmount;
    mExpiresAt = expiresAt;
}