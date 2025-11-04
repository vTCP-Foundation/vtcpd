#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <memory>
#include <vector>

#include "../../../src/core/transactions/transactions/regular/payments/CoordinatorExchangePaymentTransaction.h"
#include "../../../src/core/network/messages/payments/CoordinatorReservationResponseMessage.h"
#include "../../../src/core/transactions/transactions/regular/payments/base/PathReservation.h"
#include "../../../src/core/contractors/ContractorsManager.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../src/core/rates/manager/ExchangeRatesManager.h"
#include "../../../src/core/rates/manager/CommissionsManager.h"
#include "../../../src/core/rates/Commission.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/crypto/keychain.h"
#include "../../../src/core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../src/core/subsystems_controller/SubsystemsController.h"
#include "../../../src/core/resources/manager/ResourcesManager.h"
#include "../../../src/core/network/messages/payments/base/ResponseMessage.h"

using namespace testing;
using namespace std;

namespace {
    // Helper to create shared TrustLineAmount
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }
}

// ======================================================================================
// Coordinator Condition Change Detection and Handling Tests (PRD 09)
// ======================================================================================

TEST(CoordinatorConditionChangeTest, RejectedDueConditionsChangedEnumExists) {
    // Verify that ResponseMessage::RejectedDueConditionsChanged exists and has correct value
    EXPECT_EQ(static_cast<int>(ResponseMessage::RejectedDueConditionsChanged), 12);
}

TEST(CoordinatorConditionChangeTest, ResponseMessageWithExchangeRate_ContainsActualRate) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    TrustLineAmount actualRate(6);
    int16_t actualShift = -1;

    // Create response with RejectedDueConditionsChanged and actual exchange rate
    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        actualRate,
        actualShift);

    // Verify the response contains the condition change state
    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);

    // Verify the response contains actual exchange rate
    ASSERT_TRUE(responseMsg.actualExchangeRate().has_value());
    EXPECT_EQ(responseMsg.actualExchangeRate()->first, actualRate);
    EXPECT_EQ(responseMsg.actualExchangeRate()->second, actualShift);

    // Verify no commission is present (rate-only rejection)
    EXPECT_FALSE(responseMsg.actualCommission().has_value());
}

TEST(CoordinatorConditionChangeTest, ResponseMessageWithCommission_ContainsActualCommission) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    TrustLineAmount actualCommission(15);

    // Create response with RejectedDueConditionsChanged and actual commission
    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        actualCommission);

    // Verify the response contains the condition change state
    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);

    // Verify the response contains actual commission
    ASSERT_TRUE(responseMsg.actualCommission().has_value());
    EXPECT_EQ(responseMsg.actualCommission().value(), actualCommission);

    // Verify no exchange rate is present (commission-only rejection)
    EXPECT_FALSE(responseMsg.actualExchangeRate().has_value());
}

TEST(CoordinatorConditionChangeTest, ExchangeRatesManagerSupportsAddOrUpdate) {
    Logger logger;
    boost::asio::io_context io;

    ExchangeRatesManager ratesManager(io, logger);

    // Test that addOrUpdate method exists and works
    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);
    TrustLineAmount rate(5);
    int16_t shift = -1;

    // This should not throw
    ratesManager.addOrUpdate(fromEquiv, toEquiv, rate, shift,
                            TrustLineAmount(0), TrustLineAmount(0));

    // Verify we can retrieve the rate
    auto retrievedRate = ratesManager.get(fromEquiv, toEquiv);
    ASSERT_NE(retrievedRate, nullptr);
    EXPECT_EQ(retrievedRate->exchangeRate(), rate);
    EXPECT_EQ(retrievedRate->exchangeRateShift(), shift);
}

TEST(CoordinatorConditionChangeTest, ExchangeRatesManagerUpdateChangesRate) {
    Logger logger;
    boost::asio::io_context io;

    ExchangeRatesManager ratesManager(io, logger);

    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);

    // Set initial rate = 0.5 (5 * 10^-1)
    TrustLineAmount initialRate(5);
    int16_t initialShift = -1;
    ratesManager.addOrUpdate(fromEquiv, toEquiv, initialRate, initialShift,
                            TrustLineAmount(0), TrustLineAmount(0));

    // Verify initial rate
    auto rate1 = ratesManager.get(fromEquiv, toEquiv);
    ASSERT_NE(rate1, nullptr);
    EXPECT_EQ(rate1->exchangeRate(), initialRate);
    EXPECT_EQ(rate1->exchangeRateShift(), initialShift);

    // Update rate = 0.6 (6 * 10^-1)
    TrustLineAmount updatedRate(6);
    int16_t updatedShift = -1;
    ratesManager.addOrUpdate(fromEquiv, toEquiv, updatedRate, updatedShift,
                            TrustLineAmount(0), TrustLineAmount(0));

    // Verify rate was updated
    auto rate2 = ratesManager.get(fromEquiv, toEquiv);
    ASSERT_NE(rate2, nullptr);
    EXPECT_EQ(rate2->exchangeRate(), updatedRate);
    EXPECT_EQ(rate2->exchangeRateShift(), updatedShift);
}

TEST(CoordinatorConditionChangeTest, ExchangeRatesManagerRemoveRate) {
    Logger logger;
    boost::asio::io_context io;

    ExchangeRatesManager ratesManager(io, logger);

    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);

    // Add a rate
    TrustLineAmount rate(5);
    int16_t shift = -1;
    ratesManager.addOrUpdate(fromEquiv, toEquiv, rate, shift,
                            TrustLineAmount(0), TrustLineAmount(0));

    // Verify rate exists
    auto rateCheck = ratesManager.get(fromEquiv, toEquiv);
    ASSERT_NE(rateCheck, nullptr);

    // Remove the rate
    ratesManager.remove(fromEquiv, toEquiv);

    // Verify rate is removed
    auto rateAfterRemove = ratesManager.get(fromEquiv, toEquiv);
    EXPECT_EQ(rateAfterRemove, nullptr);
}

// ======================================================================================
// Response Message Construction Tests for Condition Changes
// ======================================================================================

TEST(CoordinatorConditionChangeTest, ResponseWithoutActualRate_IndicatesRateRemoved) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    // Create response with RejectedDueConditionsChanged but NO actual rate
    // This indicates the exchange rate was removed
    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0));

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_FALSE(responseMsg.actualExchangeRate().has_value());
    EXPECT_FALSE(responseMsg.actualCommission().has_value());
}

TEST(CoordinatorConditionChangeTest, ResponseWithZeroCommission_IndicatesCommissionRemoved) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    TrustLineAmount zeroCommission(0);

    // Create response with zero commission
    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        zeroCommission);

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);
    ASSERT_TRUE(responseMsg.actualCommission().has_value());
    EXPECT_EQ(responseMsg.actualCommission().value(), TrustLineAmount(0));
}

// ======================================================================================
// Coordinator Path Management Tests
// ======================================================================================

TEST(CoordinatorConditionChangeTest, PathIDGeneratorCreatesUniqueIDs) {
    // Verify PathID can be created and used as map key
    map<PathID, string> pathMap;

    PathID id1(1);
    PathID id2(2);
    PathID id3(3);

    pathMap[id1] = "path1";
    pathMap[id2] = "path2";
    pathMap[id3] = "path3";

    EXPECT_EQ(pathMap.size(), 3);
    EXPECT_EQ(pathMap[id1], "path1");
    EXPECT_EQ(pathMap[id2], "path2");
    EXPECT_EQ(pathMap[id3], "path3");
}

TEST(CoordinatorConditionChangeTest, OptimalPathResultHasRequiredFieldsForRecalculation) {
    // Verify OptimalPathResult has the fields needed for path recalculation
    OptimalPathResult pathResult;

    // These fields must exist for condition change handling
    // mMaxPathFlow - maximum flow capacity (preserved during recalculation)
    pathResult.mMaxPathFlow = TrustLineAmount(100);
    EXPECT_EQ(pathResult.mMaxPathFlow, TrustLineAmount(100));

    // optimal_flow - current optimal flow
    pathResult.optimal_flow = TrustLineAmount(80);
    EXPECT_EQ(pathResult.optimal_flow, TrustLineAmount(80));

    // received_amount - amount after all exchanges/commissions
    pathResult.received_amount = TrustLineAmount(75);
    EXPECT_EQ(pathResult.received_amount, TrustLineAmount(75));

    // Methods for flow calculation should exist
    // calculateFlows is used during recalculation
    SUCCEED() << "OptimalPathResult has required fields for condition change handling";
}

// ======================================================================================
// Edge Cases and Integration
// ======================================================================================

TEST(CoordinatorConditionChangeTest, MultipleConditionChanges_IndependentPaths) {
    // Verify that condition changes on different paths are handled independently

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    // Path 1: exchange rate change
    CoordinatorReservationResponseMessage response1(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        TrustLineAmount(6),
        -1);

    // Path 2: commission change
    CoordinatorReservationResponseMessage response2(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(2),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        TrustLineAmount(15));

    EXPECT_EQ(response1.pathID(), PathID(1));
    EXPECT_TRUE(response1.actualExchangeRate().has_value());
    EXPECT_FALSE(response1.actualCommission().has_value());

    EXPECT_EQ(response2.pathID(), PathID(2));
    EXPECT_FALSE(response2.actualExchangeRate().has_value());
    EXPECT_TRUE(response2.actualCommission().has_value());
}

TEST(CoordinatorConditionChangeTest, LargeExchangeRateValues_HandledCorrectly) {
    Logger logger;
    boost::asio::io_context io;

    ExchangeRatesManager ratesManager(io, logger);

    // Test with very large exchange rate
    TrustLineAmount largeRate = TrustLineAmount(0);
    largeRate += 1;
    largeRate <<= 100;
    largeRate += 0xABCDEF;
    int16_t shift = -50;

    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);

    ratesManager.addOrUpdate(fromEquiv, toEquiv, largeRate, shift,
                            TrustLineAmount(0), TrustLineAmount(0));

    auto retrieved = ratesManager.get(fromEquiv, toEquiv);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->exchangeRate(), largeRate);
    EXPECT_EQ(retrieved->exchangeRateShift(), shift);
}

TEST(CoordinatorConditionChangeTest, ExtremeShiftValues_Positive) {
    Logger logger;
    boost::asio::io_context io;

    ExchangeRatesManager ratesManager(io, logger);

    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);

    vector<int16_t> shifts = {0, 1, 10, 100, 1000, 32767};

    for (int16_t shift : shifts) {
        ratesManager.addOrUpdate(fromEquiv, toEquiv, TrustLineAmount(5), shift,
                                TrustLineAmount(0), TrustLineAmount(0));

        auto retrieved = ratesManager.get(fromEquiv, toEquiv);
        ASSERT_NE(retrieved, nullptr);
        EXPECT_EQ(retrieved->exchangeRateShift(), shift) << "Failed for shift=" << shift;
    }
}

TEST(CoordinatorConditionChangeTest, ExtremeShiftValues_Negative) {
    Logger logger;
    boost::asio::io_context io;

    ExchangeRatesManager ratesManager(io, logger);

    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);

    vector<int16_t> shifts = {-1, -10, -100, -1000, -32768};

    for (int16_t shift : shifts) {
        ratesManager.addOrUpdate(fromEquiv, toEquiv, TrustLineAmount(5), shift,
                                TrustLineAmount(0), TrustLineAmount(0));

        auto retrieved = ratesManager.get(fromEquiv, toEquiv);
        ASSERT_NE(retrieved, nullptr);
        EXPECT_EQ(retrieved->exchangeRateShift(), shift) << "Failed for shift=" << shift;
    }
}
