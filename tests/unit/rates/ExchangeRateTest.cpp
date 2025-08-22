#include <gtest/gtest.h>
#include "../../../src/core/rates/ExchangeRate.h"
#include "../../../src/core/common/time/TimeUtils.h"
#include "../../../src/core/common/Types.h"

class ExchangeRateTest : public ::testing::Test {
protected:
    void SetUp() override {
        equivFrom = 1;
        equivTo = 2;
        exchangeRate = TrustLineAmount(15000); // 1.5000 with 4 decimal places
        exchangeRateShift = 2;
        expiresAt = utc_now() + Duration(0, 5, 0); // 5 minutes from now
        minExchangeAmount = TrustLineAmount(100);
        maxExchangeAmount = TrustLineAmount(1000000);
    }

    SerializedEquivalent equivFrom;
    SerializedEquivalent equivTo;
    TrustLineAmount exchangeRate;
    int16_t exchangeRateShift;
    DateTime expiresAt;
    TrustLineAmount minExchangeAmount;
    TrustLineAmount maxExchangeAmount;
};

TEST_F(ExchangeRateTest, testConstructorSetsAllFieldsCorrectly) {
    ExchangeRate rate(
        equivFrom,
        equivTo,
        exchangeRate,
        exchangeRateShift,
        expiresAt,
        minExchangeAmount,
        maxExchangeAmount);

    EXPECT_EQ(rate.equivalentFrom(), equivFrom);
    EXPECT_EQ(rate.equivalentTo(), equivTo);
    EXPECT_EQ(rate.exchangeRate(), exchangeRate);
    EXPECT_EQ(rate.exchangeRateShift(), exchangeRateShift);
    EXPECT_EQ(rate.expiresAt(), expiresAt);
    EXPECT_EQ(rate.minExchangeAmount(), minExchangeAmount);
    EXPECT_EQ(rate.maxExchangeAmount(), maxExchangeAmount);
}

TEST_F(ExchangeRateTest, testUpdateSetsAllFieldsCorrectly) {
    ExchangeRate rate(
        equivFrom,
        equivTo,
        TrustLineAmount(10000),
        0,
        utc_now(),
        TrustLineAmount(50),
        TrustLineAmount(500000));

    TrustLineAmount newExchangeRate = TrustLineAmount(20000);
    int16_t newExchangeRateShift = -1;
    TrustLineAmount newMinExchangeAmount = TrustLineAmount(200);
    TrustLineAmount newMaxExchangeAmount = TrustLineAmount(2000000);
    DateTime newExpiresAt = utc_now() + Duration(0, 10, 0);

    rate.update(
        newExchangeRate,
        newExchangeRateShift,
        newMinExchangeAmount,
        newMaxExchangeAmount,
        newExpiresAt);

    EXPECT_EQ(rate.exchangeRate(), newExchangeRate);
    EXPECT_EQ(rate.exchangeRateShift(), newExchangeRateShift);
    EXPECT_EQ(rate.minExchangeAmount(), newMinExchangeAmount);
    EXPECT_EQ(rate.maxExchangeAmount(), newMaxExchangeAmount);
    EXPECT_EQ(rate.expiresAt(), newExpiresAt);
    
    // Verify that equivalents are not changed by update
    EXPECT_EQ(rate.equivalentFrom(), equivFrom);
    EXPECT_EQ(rate.equivalentTo(), equivTo);
}

TEST_F(ExchangeRateTest, testAccessorsReturnCorrectValues) {
    ExchangeRate rate(
        equivFrom,
        equivTo,
        exchangeRate,
        exchangeRateShift,
        expiresAt,
        minExchangeAmount,
        maxExchangeAmount);

    // Test all accessor methods
    EXPECT_EQ(rate.equivalentFrom(), equivFrom);
    EXPECT_EQ(rate.equivalentTo(), equivTo);
    EXPECT_EQ(rate.exchangeRate(), exchangeRate);
    EXPECT_EQ(rate.exchangeRateShift(), exchangeRateShift);
    EXPECT_EQ(rate.expiresAt(), expiresAt);
    EXPECT_EQ(rate.minExchangeAmount(), minExchangeAmount);
    EXPECT_EQ(rate.maxExchangeAmount(), maxExchangeAmount);
}

TEST_F(ExchangeRateTest, testSharedTypedef) {
    auto rate = std::make_shared<ExchangeRate>(
        equivFrom,
        equivTo,
        exchangeRate,
        exchangeRateShift,
        expiresAt,
        minExchangeAmount,
        maxExchangeAmount);

    ExchangeRate::Shared sharedRate = rate;
    EXPECT_EQ(sharedRate->equivalentFrom(), equivFrom);
    EXPECT_EQ(sharedRate->equivalentTo(), equivTo);
}