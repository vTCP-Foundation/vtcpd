#ifndef VTCPD_EXCHANGERATEMATH_H
#define VTCPD_EXCHANGERATEMATH_H

#include "../Types.h"
#include "../exceptions/ValueError.h"

namespace ExchangeRateMath {

TrustLineAmount applyExchangeRate(
    const TrustLineAmount &inputAmount,
    const TrustLineAmount &exchangeRate,
    int16_t exchangeRateShift);

TrustLineAmount invertExchangeRate(
    const TrustLineAmount &outputAmount,
    const TrustLineAmount &exchangeRate,
    int16_t exchangeRateShift);

} // namespace ExchangeRateMath

#endif // VTCPD_EXCHANGERATEMATH_H
