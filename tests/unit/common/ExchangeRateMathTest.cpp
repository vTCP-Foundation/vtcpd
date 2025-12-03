#include <gtest/gtest.h>

#include "core/common/multiprecision/ExchangeRateMath.h"
#include "core/common/exceptions/ValueError.h"

using namespace ExchangeRateMath;

TEST(ExchangeRateMathTest, ApplyExchangeRatePositiveShift)
{
    // 150 * 2 * 10^1 = 3000
    TrustLineAmount amount = 150;
    TrustLineAmount rate = 2;
    int16_t shift = 1;

    auto result = applyExchangeRate(amount, rate, shift);

    EXPECT_EQ(result, TrustLineAmount(3000));
}

TEST(ExchangeRateMathTest, ApplyExchangeRateNegativeShift)
{
    // 500 * 25 * 10^-2 = 500 * 25 / 100 = 125
    TrustLineAmount amount = 500;
    TrustLineAmount rate = 25;
    int16_t shift = -2;

    auto result = applyExchangeRate(amount, rate, shift);

    EXPECT_EQ(result, TrustLineAmount(125));
}

TEST(ExchangeRateMathTest, InvertExchangeRateRoundsUp)
{
    // output=101, rate=2, shift=0 => input=ceil(101/2)=51
    TrustLineAmount output = 101;
    TrustLineAmount rate = 2;
    int16_t shift = 0;

    auto result = invertExchangeRate(output, rate, shift);

    EXPECT_EQ(result, TrustLineAmount(51));
}

TEST(ExchangeRateMathTest, InvertExchangeRateWithNegativeShift)
{
    // output=200, rate=5, shift=-1 => numerator scaled by 10, ceil(2000/5)=400
    TrustLineAmount output = 200;
    TrustLineAmount rate = 5;
    int16_t shift = -1;

    auto result = invertExchangeRate(output, rate, shift);

    EXPECT_EQ(result, TrustLineAmount(400));
}

TEST(ExchangeRateMathTest, InvertExchangeRateZeroRateThrows)
{
    TrustLineAmount output = 10;
    TrustLineAmount rate = 0;

    EXPECT_THROW({
        (void)invertExchangeRate(output, rate, 0);
    }, ValueError);
}
