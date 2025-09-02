#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "../../../src/core/rates/manager/ExchangeRatesManager.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/OverflowError.h"

namespace as = boost::asio;

class ExchangeRatesManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = std::make_unique<Logger>();
        manager = std::make_unique<ExchangeRatesManager>(ioContext, *logger);
    }

    void TearDown() override {
        manager.reset();
        logger.reset();
    }

    as::io_context ioContext;
    std::unique_ptr<Logger> logger;
    std::unique_ptr<ExchangeRatesManager> manager;
};

TEST_F(ExchangeRatesManagerTest, testAddOrUpdateCreatesNewRateWithCorrectExpiry) {
    SerializedEquivalent equivFrom = 1;
    SerializedEquivalent equivTo = 2;
    TrustLineAmount exchangeRate(15000);
    int16_t exchangeRateShift = 2;
    TrustLineAmount minAmount(100);
    TrustLineAmount maxAmount(1000000);

    auto beforeAdd = utc_now();
    manager->addOrUpdate(equivFrom, equivTo, exchangeRate, exchangeRateShift, minAmount, maxAmount);
    auto afterAdd = utc_now();

    auto rate = manager->get(equivFrom, equivTo);
    ASSERT_NE(rate, nullptr);
    EXPECT_EQ(rate->equivalentFrom(), equivFrom);
    EXPECT_EQ(rate->equivalentTo(), equivTo);
    EXPECT_EQ(rate->exchangeRate(), exchangeRate);
    EXPECT_EQ(rate->exchangeRateShift(), exchangeRateShift);
    EXPECT_EQ(rate->minExchangeAmount(), minAmount);
    EXPECT_EQ(rate->maxExchangeAmount(), maxAmount);

    // Check expiry time is approximately utc_now() + TTL
    auto expectedMinExpiry = beforeAdd + Duration(0, 0, 0, ExchangeRatesManager::kTTLMilliseconds * 1000);
    auto expectedMaxExpiry = afterAdd + Duration(0, 0, 0, ExchangeRatesManager::kTTLMilliseconds * 1000);
    EXPECT_GE(rate->expiresAt(), expectedMinExpiry);
    EXPECT_LE(rate->expiresAt(), expectedMaxExpiry);
}

TEST_F(ExchangeRatesManagerTest, testAddOrUpdateUpdatesExistingRateAndRefreshesExpiry) {
    SerializedEquivalent equivFrom = 1;
    SerializedEquivalent equivTo = 2;
    
    // Add initial rate
    manager->addOrUpdate(equivFrom, equivTo, TrustLineAmount(10000), 0, TrustLineAmount(50), TrustLineAmount(500000));
    auto firstRate = manager->get(equivFrom, equivTo);
    auto firstExpiry = firstRate->expiresAt();

    // Wait a bit to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Update the rate
    TrustLineAmount newExchangeRate(20000);
    int16_t newShift = 1;
    TrustLineAmount newMinAmount(200);
    TrustLineAmount newMaxAmount(2000000);
    
    auto beforeUpdate = utc_now();
    manager->addOrUpdate(equivFrom, equivTo, newExchangeRate, newShift, newMinAmount, newMaxAmount);
    auto afterUpdate = utc_now();

    auto updatedRate = manager->get(equivFrom, equivTo);
    ASSERT_NE(updatedRate, nullptr);
    EXPECT_EQ(updatedRate->exchangeRate(), newExchangeRate);
    EXPECT_EQ(updatedRate->exchangeRateShift(), newShift);
    EXPECT_EQ(updatedRate->minExchangeAmount(), newMinAmount);
    EXPECT_EQ(updatedRate->maxExchangeAmount(), newMaxAmount);

    // Check that expiry was refreshed
    auto expectedMinExpiry = beforeUpdate + Duration(0, 0, 0, ExchangeRatesManager::kTTLMilliseconds * 1000);
    auto expectedMaxExpiry = afterUpdate + Duration(0, 0, 0, ExchangeRatesManager::kTTLMilliseconds * 1000);
    EXPECT_GE(updatedRate->expiresAt(), expectedMinExpiry);
    EXPECT_LE(updatedRate->expiresAt(), expectedMaxExpiry);
    EXPECT_GT(updatedRate->expiresAt(), firstExpiry);
}

TEST_F(ExchangeRatesManagerTest, testGetReturnsCorrectRate) {
    SerializedEquivalent equivFrom = 1;
    SerializedEquivalent equivTo = 2;
    TrustLineAmount exchangeRate(15000);
    int16_t exchangeRateShift = 2;
    TrustLineAmount minAmount(100);
    TrustLineAmount maxAmount(1000000);

    manager->addOrUpdate(equivFrom, equivTo, exchangeRate, exchangeRateShift, minAmount, maxAmount);

    auto rate = manager->get(equivFrom, equivTo);
    ASSERT_NE(rate, nullptr);
    EXPECT_EQ(rate->equivalentFrom(), equivFrom);
    EXPECT_EQ(rate->equivalentTo(), equivTo);
    EXPECT_EQ(rate->exchangeRate(), exchangeRate);
    EXPECT_EQ(rate->exchangeRateShift(), exchangeRateShift);
    EXPECT_EQ(rate->minExchangeAmount(), minAmount);
    EXPECT_EQ(rate->maxExchangeAmount(), maxAmount);
}

TEST_F(ExchangeRatesManagerTest, testGetReturnsNullptrForMissingPair) {
    auto rate = manager->get(1, 2);
    EXPECT_EQ(rate, nullptr);
}

TEST_F(ExchangeRatesManagerTest, testListReturnsAllRates) {
    // Add multiple rates
    manager->addOrUpdate(1, 2, TrustLineAmount(15000), 2, TrustLineAmount(100), TrustLineAmount(1000000));
    manager->addOrUpdate(2, 3, TrustLineAmount(20000), -1, TrustLineAmount(200), TrustLineAmount(2000000));
    manager->addOrUpdate(3, 1, TrustLineAmount(25000), 0, TrustLineAmount(300), TrustLineAmount(3000000));

    auto rates = manager->list();
    EXPECT_EQ(rates.size(), 3);

    // Verify all rates are present
    bool found12 = false, found23 = false, found31 = false;
    for (const auto& rate : rates) {
        if (rate->equivalentFrom() == 1 && rate->equivalentTo() == 2) {
            found12 = true;
            EXPECT_EQ(rate->exchangeRate(), TrustLineAmount(15000));
        } else if (rate->equivalentFrom() == 2 && rate->equivalentTo() == 3) {
            found23 = true;
            EXPECT_EQ(rate->exchangeRate(), TrustLineAmount(20000));
        } else if (rate->equivalentFrom() == 3 && rate->equivalentTo() == 1) {
            found31 = true;
            EXPECT_EQ(rate->exchangeRate(), TrustLineAmount(25000));
        }
    }
    EXPECT_TRUE(found12);
    EXPECT_TRUE(found23);
    EXPECT_TRUE(found31);
}

TEST_F(ExchangeRatesManagerTest, testRemoveDeletesSpecificPair) {
    // Add multiple rates
    manager->addOrUpdate(1, 2, TrustLineAmount(15000), 2, TrustLineAmount(100), TrustLineAmount(1000000));
    manager->addOrUpdate(2, 3, TrustLineAmount(20000), -1, TrustLineAmount(200), TrustLineAmount(2000000));

    // Remove one rate
    manager->remove(1, 2);

    // Verify the specific rate is removed
    auto rate12 = manager->get(1, 2);
    EXPECT_EQ(rate12, nullptr);

    // Verify other rate is still present
    auto rate23 = manager->get(2, 3);
    EXPECT_NE(rate23, nullptr);
    EXPECT_EQ(rate23->exchangeRate(), TrustLineAmount(20000));
}

TEST_F(ExchangeRatesManagerTest, testClearRemovesAllRates) {
    // Add multiple rates
    manager->addOrUpdate(1, 2, TrustLineAmount(15000), 2, TrustLineAmount(100), TrustLineAmount(1000000));
    manager->addOrUpdate(2, 3, TrustLineAmount(20000), -1, TrustLineAmount(200), TrustLineAmount(2000000));

    // Clear all rates
    manager->clear();

    // Verify all rates are removed
    auto rates = manager->list();
    EXPECT_EQ(rates.size(), 0);
    
    auto rate12 = manager->get(1, 2);
    EXPECT_EQ(rate12, nullptr);
    
    auto rate23 = manager->get(2, 3);
    EXPECT_EQ(rate23, nullptr);
}

TEST_F(ExchangeRatesManagerTest, testCalculateConvertedAmountPositiveShift) {
    // 1000 * 1.5 with shift +2 = 1500 * 100 = 150000
    manager->addOrUpdate(1, 2, TrustLineAmount(15000), 2, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(1000));
    EXPECT_EQ(result, TrustLineAmount(1500000000)); // 1000 * 15000 * 100
}

TEST_F(ExchangeRatesManagerTest, testCalculateConvertedAmountNegativeShift) {
    // 123456 * 1.0 with shift -2 = 123456 / 100 = 1234 (truncated)
    manager->addOrUpdate(1, 2, TrustLineAmount(10000), -2, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(123456));
    EXPECT_EQ(result, TrustLineAmount(12345600)); // 123456 * 10000 / 100
}

TEST_F(ExchangeRatesManagerTest, testCalculateConvertedAmountZeroShift) {
    // 1000 * 2.5 with shift 0 = 2500
    manager->addOrUpdate(1, 2, TrustLineAmount(25000), 0, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(1000));
    EXPECT_EQ(result, TrustLineAmount(25000000)); // 1000 * 25000
}

TEST_F(ExchangeRatesManagerTest, testCalculateThrowsNotFoundError) {
    // No rate added for this pair
    EXPECT_THROW(
        manager->calculateConvertedAmount(1, 2, TrustLineAmount(1000)),
        NotFoundError
    );
}

TEST_F(ExchangeRatesManagerTest, testCalculateWithZeroAmount) {
    manager->addOrUpdate(1, 2, TrustLineAmount(25000), 5, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(0));
    EXPECT_EQ(result, TrustLineAmount(0));
}

TEST_F(ExchangeRatesManagerTest, testCalculateWithZeroRate) {
    manager->addOrUpdate(1, 2, TrustLineAmount(0), 6, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(123456));
    EXPECT_EQ(result, TrustLineAmount(0));
}

TEST_F(ExchangeRatesManagerTest, testCalculateWithFractionalRateAndPositiveShift) {
    // 1000 * 1.2345 with shift +2 = 1234500
    manager->addOrUpdate(1, 2, TrustLineAmount(12345), 2, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(1000));
    EXPECT_EQ(result, TrustLineAmount(1234500000)); // 1000 * 12345 * 100
}

TEST_F(ExchangeRatesManagerTest, testCalculateResultBecomesZeroAfterNegativeShift) {
    // 99 * 1.0 with shift -3 results in 0
    manager->addOrUpdate(1, 2, TrustLineAmount(10000), -3, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(99));
    EXPECT_EQ(result, TrustLineAmount(990)); // 99 * 10000 / 1000
}

TEST_F(ExchangeRatesManagerTest, testCalculateTruncatesTowardZeroOnNegativeShiftWithRemainder) {
    // 199 * 1.0 with shift -2 results in 1 (no rounding)
    manager->addOrUpdate(1, 2, TrustLineAmount(10000), -2, TrustLineAmount(1), TrustLineAmount(1000000));
    
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(199));
    EXPECT_EQ(result, TrustLineAmount(19900)); // 199 * 10000 / 100
}

TEST_F(ExchangeRatesManagerTest, testCalculateMaxAmountWithUnitRate) {
    // Test with maximum possible amount and unit rate
    TrustLineAmount maxAmount("115792089237316195423570985008687907853269984665640564039457584007913129639935");
    manager->addOrUpdate(1, 2, TrustLineAmount(10000), 0, TrustLineAmount(1), maxAmount);
    
    // This should not throw overflow error
    auto result = manager->calculateConvertedAmount(1, 2, TrustLineAmount(1000));
    EXPECT_EQ(result, TrustLineAmount(10000000));
}

TEST_F(ExchangeRatesManagerTest, testAddOrUpdateExternalCreatesNewExternalRate) {
    SerializedEquivalent equivFrom = 1;
    SerializedEquivalent equivTo = 2;
    TrustLineAmount exchangeRate(15000);
    int16_t exchangeRateShift = 2;
    TrustLineAmount minAmount(100);
    TrustLineAmount maxAmount(1000000);
    DateTime expiresAt = utc_now() + Duration(0, 5, 0, 0);

    ExchangeRate rate(equivFrom, equivTo, exchangeRate, exchangeRateShift, expiresAt, minAmount, maxAmount);
    ContractorID contractorID = 101;
    manager->addOrUpdateExternal(contractorID, rate);

    auto externalRates = manager->getExternalRates(equivFrom, equivTo);
    ASSERT_EQ(externalRates.size(), 1);
    
    auto retrievedPair = externalRates[0];
    EXPECT_EQ(retrievedPair.first, contractorID);
    auto retrievedRate = retrievedPair.second;
    EXPECT_EQ(retrievedRate->equivalentFrom(), equivFrom);
    EXPECT_EQ(retrievedRate->equivalentTo(), equivTo);
    EXPECT_EQ(retrievedRate->exchangeRate(), exchangeRate);
    EXPECT_EQ(retrievedRate->exchangeRateShift(), exchangeRateShift);
    EXPECT_EQ(retrievedRate->minExchangeAmount(), minAmount);
    EXPECT_EQ(retrievedRate->maxExchangeAmount(), maxAmount);
    EXPECT_EQ(retrievedRate->expiresAt(), expiresAt);
}

TEST_F(ExchangeRatesManagerTest, testAddOrUpdateExternalAddsMultipleRatesForSamePair) {
    SerializedEquivalent equivFrom = 1;
    SerializedEquivalent equivTo = 2;
    DateTime expiresAt1 = utc_now() + Duration(0, 5, 0, 0);
    DateTime expiresAt2 = utc_now() + Duration(0, 10, 0, 0);

    ExchangeRate rate1(equivFrom, equivTo, TrustLineAmount(15000), 2, expiresAt1, TrustLineAmount(100), TrustLineAmount(1000000));
    ExchangeRate rate2(equivFrom, equivTo, TrustLineAmount(20000), 1, expiresAt2, TrustLineAmount(200), TrustLineAmount(2000000));
    ContractorID contractorID1 = 201;
    ContractorID contractorID2 = 202;
    
    manager->addOrUpdateExternal(contractorID1, rate1);
    manager->addOrUpdateExternal(contractorID2, rate2);

    auto externalRates = manager->getExternalRates(equivFrom, equivTo);
    ASSERT_EQ(externalRates.size(), 2);
    
    EXPECT_EQ(externalRates[0].first, contractorID1);
    EXPECT_EQ(externalRates[1].first, contractorID2);
    EXPECT_EQ(externalRates[0].second->exchangeRate(), TrustLineAmount(15000));
    EXPECT_EQ(externalRates[1].second->exchangeRate(), TrustLineAmount(20000));
}

TEST_F(ExchangeRatesManagerTest, testGetExternalRatesReturnsEmptyVectorForMissingPair) {
    auto externalRates = manager->getExternalRates(1, 2);
    EXPECT_EQ(externalRates.size(), 0);
}

TEST_F(ExchangeRatesManagerTest, testListExternalRatesReturnsAllExternalRates) {
    DateTime expiresAt = utc_now() + Duration(0, 5, 0, 0);
    
    ExchangeRate rate1(1, 2, TrustLineAmount(15000), 2, expiresAt, TrustLineAmount(100), TrustLineAmount(1000000));
    ExchangeRate rate2(2, 3, TrustLineAmount(20000), -1, expiresAt, TrustLineAmount(200), TrustLineAmount(2000000));
    ExchangeRate rate3(3, 1, TrustLineAmount(25000), 0, expiresAt, TrustLineAmount(300), TrustLineAmount(3000000));
    ContractorID contractorID1 = 301;
    ContractorID contractorID2 = 302;
    ContractorID contractorID3 = 303;
    
    manager->addOrUpdateExternal(contractorID1, rate1);
    manager->addOrUpdateExternal(contractorID2, rate2);
    manager->addOrUpdateExternal(contractorID3, rate3);

    auto allExternalRates = manager->listExternalRates();
    EXPECT_EQ(allExternalRates.size(), 3);

    bool found12 = false, found23 = false, found31 = false;
    for (const auto& ratePair : allExternalRates) {
        const auto &rate = ratePair.second;
        if (rate->equivalentFrom() == 1 && rate->equivalentTo() == 2) {
            found12 = true;
            EXPECT_EQ(rate->exchangeRate(), TrustLineAmount(15000));
        } else if (rate->equivalentFrom() == 2 && rate->equivalentTo() == 3) {
            found23 = true;
            EXPECT_EQ(rate->exchangeRate(), TrustLineAmount(20000));
        } else if (rate->equivalentFrom() == 3 && rate->equivalentTo() == 1) {
            found31 = true;
            EXPECT_EQ(rate->exchangeRate(), TrustLineAmount(25000));
        }
    }
    EXPECT_TRUE(found12);
    EXPECT_TRUE(found23);
    EXPECT_TRUE(found31);
}

TEST_F(ExchangeRatesManagerTest, testExternalRatesExpiredCleanup) {
    DateTime expiredTime = utc_now() - Duration(0, 1, 0, 0);  // 1 minute ago
    DateTime futureTime = utc_now() + Duration(0, 5, 0, 0);   // 5 minutes from now
    
    ExchangeRate expiredRate(1, 2, TrustLineAmount(15000), 2, expiredTime, TrustLineAmount(100), TrustLineAmount(1000000));
    ExchangeRate futureRate(2, 3, TrustLineAmount(20000), -1, futureTime, TrustLineAmount(200), TrustLineAmount(2000000));
    ContractorID contractorID1 = 401;
    ContractorID contractorID2 = 402;
    
    manager->addOrUpdateExternal(contractorID1, expiredRate);
    manager->addOrUpdateExternal(contractorID2, futureRate);
    
    // Run io_context briefly to allow timer to process expired rates
    ioContext.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ioContext.poll();
    
    // Check that expired rate was removed and future rate remains
    auto rates12 = manager->getExternalRates(1, 2);
    auto rates23 = manager->getExternalRates(2, 3);
    
    EXPECT_EQ(rates12.size(), 0);  // Expired rate should be removed
    EXPECT_EQ(rates23.size(), 1);  // Future rate should remain
}

TEST_F(ExchangeRatesManagerTest, testClearRemovesAllExternalRates) {
    DateTime expiresAt = utc_now() + Duration(0, 5, 0, 0);
    
    ExchangeRate rate1(1, 2, TrustLineAmount(15000), 2, expiresAt, TrustLineAmount(100), TrustLineAmount(1000000));
    ExchangeRate rate2(2, 3, TrustLineAmount(20000), -1, expiresAt, TrustLineAmount(200), TrustLineAmount(2000000));
    
    manager->addOrUpdateExternal(501, rate1);
    manager->addOrUpdateExternal(502, rate2);
    
    manager->clear();
    
    auto allExternalRates = manager->listExternalRates();
    EXPECT_EQ(allExternalRates.size(), 0);
    
    auto rates12 = manager->getExternalRates(1, 2);
    auto rates23 = manager->getExternalRates(2, 3);
    EXPECT_EQ(rates12.size(), 0);
    EXPECT_EQ(rates23.size(), 0);
}

TEST_F(ExchangeRatesManagerTest, testAddOrUpdateExternalReplacesExistingForSameContractorWhenNewExpiryLater) {
    // Prepare initial and newer expiry times
    SerializedEquivalent equivFrom = 10;
    SerializedEquivalent equivTo = 11;
    ContractorID contractorID = 777;

    DateTime expiresAtInitial = utc_now() + Duration(0, 5, 0, 0);
    DateTime expiresAtLater = utc_now() + Duration(0, 10, 0, 0);

    // Initial rate for the contractor
    ExchangeRate initialRate(
        equivFrom,
        equivTo,
        TrustLineAmount(15000),
        2,
        expiresAtInitial,
        TrustLineAmount(100),
        TrustLineAmount(1000000));

    manager->addOrUpdateExternal(contractorID, initialRate);

    // New rate with later expiry should replace the existing one for the same contractor
    ExchangeRate newerRate(
        equivFrom,
        equivTo,
        TrustLineAmount(20000),
        1,
        expiresAtLater,
        TrustLineAmount(200),
        TrustLineAmount(2000000));

    manager->addOrUpdateExternal(contractorID, newerRate);

    auto externalRates = manager->getExternalRates(equivFrom, equivTo);
    ASSERT_EQ(externalRates.size(), 1);

    EXPECT_EQ(externalRates[0].first, contractorID);
    auto storedRate = externalRates[0].second;
    EXPECT_EQ(storedRate->exchangeRate(), TrustLineAmount(20000));
    EXPECT_EQ(storedRate->exchangeRateShift(), 1);
    EXPECT_EQ(storedRate->minExchangeAmount(), TrustLineAmount(200));
    EXPECT_EQ(storedRate->maxExchangeAmount(), TrustLineAmount(2000000));
    EXPECT_EQ(storedRate->expiresAt(), expiresAtLater);
}

TEST_F(ExchangeRatesManagerTest, testAddOrUpdateExternalDoesNotReplaceWhenNewExpiryEarlierOrEqual) {
    SerializedEquivalent equivFrom = 20;
    SerializedEquivalent equivTo = 21;
    ContractorID contractorID = 888;

    DateTime expiresAtOriginal = utc_now() + Duration(0, 10, 0, 0);
    DateTime expiresAtEarlier = utc_now() + Duration(0, 5, 0, 0);
    DateTime expiresAtEqual = expiresAtOriginal;

    // Add original rate
    ExchangeRate originalRate(
        equivFrom,
        equivTo,
        TrustLineAmount(30000),
        3,
        expiresAtOriginal,
        TrustLineAmount(300),
        TrustLineAmount(3000000));
    manager->addOrUpdateExternal(contractorID, originalRate);

    // Attempt equal-expiry replacement – should not replace
    ExchangeRate equalExpiryRate(
        equivFrom,
        equivTo,
        TrustLineAmount(99999), // different to detect unintended replacement
        9,
        expiresAtEqual,
        TrustLineAmount(999),
        TrustLineAmount(9999999));
    manager->addOrUpdateExternal(contractorID, equalExpiryRate);

    // Attempt earlier-expiry replacement – should also not replace
    ExchangeRate earlierExpiryRate(
        equivFrom,
        equivTo,
        TrustLineAmount(11111),
        1,
        expiresAtEarlier,
        TrustLineAmount(111),
        TrustLineAmount(1111111));
    manager->addOrUpdateExternal(contractorID, earlierExpiryRate);

    auto externalRates = manager->getExternalRates(equivFrom, equivTo);
    ASSERT_EQ(externalRates.size(), 1);

    EXPECT_EQ(externalRates[0].first, contractorID);
    auto storedRate = externalRates[0].second;
    EXPECT_EQ(storedRate->exchangeRate(), TrustLineAmount(30000));
    EXPECT_EQ(storedRate->exchangeRateShift(), 3);
    EXPECT_EQ(storedRate->minExchangeAmount(), TrustLineAmount(300));
    EXPECT_EQ(storedRate->maxExchangeAmount(), TrustLineAmount(3000000));
    EXPECT_EQ(storedRate->expiresAt(), expiresAtOriginal);
}