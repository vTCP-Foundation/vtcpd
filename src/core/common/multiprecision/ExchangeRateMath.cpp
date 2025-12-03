#include "ExchangeRateMath.h"

#include <boost/multiprecision/cpp_int.hpp>

#include <string>

namespace ExchangeRateMath {
namespace {

using boost::multiprecision::cpp_int;

cpp_int pow10(size_t exponent)
{
    cpp_int result = 1;
    for (size_t idx = 0; idx < exponent; ++idx) {
        result *= 10;
    }
    return result;
}

TrustLineAmount ceilDivideToAmount(
    const cpp_int &numerator,
    const cpp_int &denominator)
{
    if (denominator <= 0) {
        throw ValueError(
            "ExchangeRateMath::ceilDivideToAmount: denominator must be positive");
    }

    cpp_int quotient = numerator / denominator;
    if (numerator % denominator != 0) {
        ++quotient;
    }

    if (quotient < 0) {
        throw ValueError(
            "ExchangeRateMath::ceilDivideToAmount: negative quotient computed");
    }

    return quotient.convert_to<TrustLineAmount>();
}

} // namespace

TrustLineAmount applyExchangeRate(
    const TrustLineAmount &inputAmount,
    const TrustLineAmount &exchangeRate,
    int16_t exchangeRateShift)
{
    cpp_int result = cpp_int(inputAmount) * cpp_int(exchangeRate);

    if (exchangeRateShift > 0) {
        result *= pow10(static_cast<size_t>(exchangeRateShift));
    } else if (exchangeRateShift < 0) {
        const cpp_int divisor = pow10(static_cast<size_t>(-exchangeRateShift));
        if (divisor == 0) {
            throw ValueError(
                "ExchangeRateMath::applyExchangeRate: invalid divisor for negative shift");
        }
        result /= divisor;
    }

    if (result < 0) {
        throw ValueError(
            "ExchangeRateMath::applyExchangeRate: negative converted amount");
    }

    try {
        return result.convert_to<TrustLineAmount>();
    } catch (const std::exception &e) {
        throw ValueError(
            std::string("ExchangeRateMath::applyExchangeRate: conversion overflow: ") +
            e.what());
    }
}

TrustLineAmount invertExchangeRate(
    const TrustLineAmount &outputAmount,
    const TrustLineAmount &exchangeRate,
    int16_t exchangeRateShift)
{
    if (exchangeRate == TrustLineAmount(0)) {
        throw ValueError(
            "ExchangeRateMath::invertExchangeRate: zero exchange rate");
    }

    cpp_int numerator = cpp_int(outputAmount);
    cpp_int denominator = cpp_int(exchangeRate);

    if (exchangeRateShift >= 0) {
        denominator *= pow10(static_cast<size_t>(exchangeRateShift));
    } else {
        numerator *= pow10(static_cast<size_t>(-exchangeRateShift));
    }

    return ceilDivideToAmount(numerator, denominator);
}

} // namespace ExchangeRateMath
