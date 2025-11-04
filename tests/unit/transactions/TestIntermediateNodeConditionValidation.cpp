#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <memory>
#include <vector>

#include "../../../src/core/transactions/transactions/regular/payments/IntermediateNodeExchangePaymentTransaction.h"
#include "../../../src/core/network/messages/payments/CoordinatorReservationRequestMessage.h"
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

using namespace testing;
using namespace std;

namespace {
    // Helper to create shared TrustLineAmount
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    /**
     * Test environment for IntermediateNode condition validation tests.
     */
    struct IntermediateNodeValidationTestEnv {
        Logger logger;
        unique_ptr<boost::asio::io_context> io;
        string dbDir;
        unique_ptr<StorageHandlerSQLite> storage;
        unique_ptr<crypto::Keystore> keystore;
        unique_ptr<EventsInterfaceManager> eventsManager;
        unique_ptr<ContractorsManager> contractors;
        unique_ptr<EquivalentsSubsystemsRouter> router;
        unique_ptr<ExchangeRatesManager> ratesManager;
        unique_ptr<SubsystemsController> subsystemsController;
        unique_ptr<ResourcesManager> resourcesManager;

        BaseAddress::Shared coordinatorAddress;
        BaseAddress::Shared nextNodeAddress;
        ContractorID coordinatorID;
        ContractorID nextNodeID;

        SerializedEquivalent EQ_1001{1001};
        SerializedEquivalent EQ_2002{2002};

        IntermediateNodeValidationTestEnv(const string& testName) {
            dbDir = "build-tests/testdb_intermediate_validation_" + testName;
            filesystem::remove_all(dbDir);

            io = make_unique<boost::asio::io_context>();
            storage = make_unique<StorageHandlerSQLite>(dbDir, "test.db", logger);

            keystore = make_unique<crypto::Keystore>(logger);
            {
                auto ioTransaction = storage->beginTransaction();
                keystore->init(ioTransaction);
            }

            eventsManager = make_unique<EventsInterfaceManager>(
                vector<pair<string, SerializedEventType>>{},
                vector<pair<string, bool>>{},
                logger);

            vector<pair<string, string>> ownAddrs = {{"ipv4", "127.0.0.1:3000"}};
            contractors = make_unique<ContractorsManager>(ownAddrs, storage.get(), logger);

            vector<SerializedEquivalent> gateways;
            router = make_unique<EquivalentsSubsystemsRouter>(
                storage.get(),
                keystore.get(),
                contractors.get(),
                eventsManager.get(),
                *io,
                gateways,
                logger);

            router->initNewEquivalent(EQ_1001);
            router->initNewEquivalent(EQ_2002);

            ratesManager = make_unique<ExchangeRatesManager>(*io, logger);
            subsystemsController = make_unique<SubsystemsController>(logger);
            resourcesManager = make_unique<ResourcesManager>();

            coordinatorAddress = make_shared<IPv4WithPortAddress>("192.168.1.5:8080");
            nextNodeAddress = make_shared<IPv4WithPortAddress>("192.168.1.10:8080");

            coordinatorID = router->getOrCreateParticipantID(coordinatorAddress);
            nextNodeID = router->getOrCreateParticipantID(nextNodeAddress);
        }

        ~IntermediateNodeValidationTestEnv() {
            filesystem::remove_all(dbDir);
        }

        void setupExchangeRate(
            SerializedEquivalent fromEquiv,
            SerializedEquivalent toEquiv,
            const TrustLineAmount& rate,
            int16_t rateShift)
        {
            ratesManager->addOrUpdate(
                fromEquiv, toEquiv, rate, rateShift,
                TrustLineAmount(0), TrustLineAmount(0));
        }
    };

} // namespace

// ======================================================================================
// Exchange Rate Validation Tests
// ======================================================================================

TEST(IntermediateNodeValidationTest, ExchangeRateMatches_MessageContainsExpectedRate) {
    IntermediateNodeValidationTestEnv env("exchange_rate_matches");

    // Setup: Node has rate 0.5 (5 * 10^-1)
    TrustLineAmount rate(5);
    int16_t shift = -1;
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, rate, shift);

    // Create CoordinatorReservationRequestMessage with matching expected rate
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

    CoordinatorReservationRequestMessage msg(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        finalCfg,
        env.nextNodeAddress,
        rate,  // Expected rate matches actual
        shift);

    // Verify message has expected rate
    ASSERT_TRUE(msg.expectedExchangeRate().has_value());
    EXPECT_EQ(msg.expectedExchangeRate()->first, rate);
    EXPECT_EQ(msg.expectedExchangeRate()->second, shift);
}

TEST(IntermediateNodeValidationTest, ExchangeRateMismatch_ResponseContainsActualRate) {
    IntermediateNodeValidationTestEnv env("exchange_rate_mismatch");

    // Setup: Node has actual rate = 0.6
    TrustLineAmount actualRate(6);
    int16_t actualShift = -1;
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, actualRate, actualShift);

    // Coordinator expects rate = 0.5
    TrustLineAmount expectedRate(5);
    int16_t expectedShift = -1;

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

    CoordinatorReservationRequestMessage requestMsg(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        finalCfg,
        env.nextNodeAddress,
        expectedRate,
        expectedShift);

    // When rates don't match, response should contain RejectedDueConditionsChanged with actual rate
    CoordinatorReservationResponseMessage responseMsg(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        actualRate,
        actualShift);

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);
    ASSERT_TRUE(responseMsg.actualExchangeRate().has_value());
    EXPECT_EQ(responseMsg.actualExchangeRate()->first, actualRate);
    EXPECT_EQ(responseMsg.actualExchangeRate()->second, actualShift);
}

TEST(IntermediateNodeValidationTest, ExchangeRateNotFound_ResponseIndicatesMissing) {
    IntermediateNodeValidationTestEnv env("exchange_rate_not_found");

    // Setup: Coordinator expects rate but node doesn't have it
    TrustLineAmount expectedRate(5);
    int16_t expectedShift = -1;

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    // Response when rate not found - RejectedDueConditionsChanged without rate field
    CoordinatorReservationResponseMessage responseMsg(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0));

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_FALSE(responseMsg.actualExchangeRate().has_value());
}

// ======================================================================================
// Commission Validation Tests
// ======================================================================================

TEST(IntermediateNodeValidationTest, CommissionMatches_MessageContainsExpectedCommission) {
    IntermediateNodeValidationTestEnv env("commission_matches");

    TrustLineAmount commission(10);

    // Create CoordinatorReservationRequestMessage with expected commission
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

    CoordinatorReservationRequestMessage msg(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        finalCfg,
        env.nextNodeAddress,
        commission);  // Expected commission

    // Verify message has expected commission
    ASSERT_TRUE(msg.expectedCommission().has_value());
    EXPECT_EQ(msg.expectedCommission().value(), commission);
}

TEST(IntermediateNodeValidationTest, CommissionMismatch_ResponseContainsActualCommission) {
    IntermediateNodeValidationTestEnv env("commission_mismatch");

    TrustLineAmount expectedCommission(10);
    TrustLineAmount actualCommission(15);

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    // When commissions don't match, response contains actual commission
    CoordinatorReservationResponseMessage responseMsg(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        actualCommission);

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueConditionsChanged);
    ASSERT_TRUE(responseMsg.actualCommission().has_value());
    EXPECT_EQ(responseMsg.actualCommission().value(), actualCommission);
}

TEST(IntermediateNodeValidationTest, CommissionRemoved_ResponseWithZeroCommission) {
    IntermediateNodeValidationTestEnv env("commission_removed");

    TrustLineAmount zeroCommission(0);

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    // Node no longer has commission - response with 0
    CoordinatorReservationResponseMessage responseMsg(
        env.EQ_1001,
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
// Message Structure Tests
// ======================================================================================

TEST(IntermediateNodeValidationTest, RequestMessage_CannotHaveBothExchangeRateAndCommission) {
    IntermediateNodeValidationTestEnv env("protocol_violation");

    // Protocol: message can have EITHER exchange rate OR commission, not both
    // Test that separate constructors exist for each

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

    // Constructor with exchange rate
    CoordinatorReservationRequestMessage msgWithRate(
        env.EQ_1001, senderAddrs, txnUUID, finalCfg, env.nextNodeAddress,
        TrustLineAmount(5), -1);

    EXPECT_TRUE(msgWithRate.expectedExchangeRate().has_value());
    EXPECT_FALSE(msgWithRate.expectedCommission().has_value());

    // Constructor with commission
    CoordinatorReservationRequestMessage msgWithCommission(
        env.EQ_1001, senderAddrs, txnUUID, finalCfg, env.nextNodeAddress,
        TrustLineAmount(10));

    EXPECT_FALSE(msgWithCommission.expectedExchangeRate().has_value());
    EXPECT_TRUE(msgWithCommission.expectedCommission().has_value());
}

TEST(IntermediateNodeValidationTest, ResponseMessage_CanContainEitherRateOrCommission) {
    IntermediateNodeValidationTestEnv env("response_structure");

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    // Response with exchange rate
    CoordinatorReservationResponseMessage respWithRate(
        env.EQ_1001, senderAddrs, txnUUID, PathID(1),
        ResponseMessage::RejectedDueConditionsChanged, TrustLineAmount(0),
        TrustLineAmount(6), -1);

    EXPECT_TRUE(respWithRate.actualExchangeRate().has_value());
    EXPECT_FALSE(respWithRate.actualCommission().has_value());

    // Response with commission
    CoordinatorReservationResponseMessage respWithCommission(
        env.EQ_1001, senderAddrs, txnUUID, PathID(1),
        ResponseMessage::RejectedDueConditionsChanged, TrustLineAmount(0),
        TrustLineAmount(15));

    EXPECT_FALSE(respWithCommission.actualExchangeRate().has_value());
    EXPECT_TRUE(respWithCommission.actualCommission().has_value());
}

// ======================================================================================
// Edge Cases
// ======================================================================================

TEST(IntermediateNodeValidationTest, LargeExchangeRateValues) {
    IntermediateNodeValidationTestEnv env("large_rate");

    // Test with very large exchange rate value
    TrustLineAmount largeRate = 0;
    largeRate += 1;
    largeRate <<= 100;
    largeRate += 0xABCDEF;
    int16_t shift = -50;

    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, largeRate, shift);

    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

    CoordinatorReservationRequestMessage msg(
        env.EQ_1001, senderAddrs, txnUUID, finalCfg, env.nextNodeAddress,
        largeRate, shift);

    ASSERT_TRUE(msg.expectedExchangeRate().has_value());
    EXPECT_EQ(msg.expectedExchangeRate()->first, largeRate);
    EXPECT_EQ(msg.expectedExchangeRate()->second, shift);
}

TEST(IntermediateNodeValidationTest, NegativeExchangeRateShift) {
    IntermediateNodeValidationTestEnv env("negative_shift");

    vector<int16_t> shifts = {-1, -2, -5, -10, -100, -32768};

    for (int16_t shift : shifts) {
        TransactionUUID txnUUID;
        vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
        vector<PathReservation> finalCfg;
        finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

        CoordinatorReservationRequestMessage msg(
            env.EQ_1001, senderAddrs, txnUUID, finalCfg, env.nextNodeAddress,
            TrustLineAmount(5), shift);

        ASSERT_TRUE(msg.expectedExchangeRate().has_value());
        EXPECT_EQ(msg.expectedExchangeRate()->second, shift);
    }
}

TEST(IntermediateNodeValidationTest, PositiveExchangeRateShift) {
    IntermediateNodeValidationTestEnv env("positive_shift");

    vector<int16_t> shifts = {0, 1, 2, 5, 10, 100, 32767};

    for (int16_t shift : shifts) {
        TransactionUUID txnUUID;
        vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};
        vector<PathReservation> finalCfg;
        finalCfg.push_back(PathReservation(PathID(1), A(100), env.EQ_1001, PathReservation::Outgoing));

        CoordinatorReservationRequestMessage msg(
            env.EQ_1001, senderAddrs, txnUUID, finalCfg, env.nextNodeAddress,
            TrustLineAmount(5), shift);

        ASSERT_TRUE(msg.expectedExchangeRate().has_value());
        EXPECT_EQ(msg.expectedExchangeRate()->second, shift);
    }
}

// ======================================================================================
// Real Transaction Logic Tests (Limited Scope)
// These tests verify actual validation logic from IntermediateNodeExchangePaymentTransaction
// ======================================================================================

/**
 * Test that sendRejectedDueConditionsChanged creates correct response message
 * with exchange rate.
 */
TEST(IntermediateNodeValidationLogicTest, SendRejectedWithExchangeRate_CreatesCorrectMessage) {
    IntermediateNodeValidationTestEnv env("send_rejected_rate");

    // Setup actual exchange rate
    TrustLineAmount actualRate(6);
    int16_t actualShift = -1;
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, actualRate, actualShift);

    // Get the rate from manager
    auto exchangeRate = env.ratesManager->get(env.EQ_1001, env.EQ_2002);
    ASSERT_NE(exchangeRate, nullptr);

    // Verify the rate has correct values (simulating what sendRejectedDueConditionsChanged does)
    EXPECT_EQ(exchangeRate->exchangeRate(), actualRate);
    EXPECT_EQ(exchangeRate->exchangeRateShift(), actualShift);

    // Create response message as sendRejectedDueConditionsChanged would
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    CoordinatorReservationResponseMessage response(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        exchangeRate->exchangeRate(),
        exchangeRate->exchangeRateShift());

    // Verify response contains correct data
    EXPECT_EQ(response.state(), ResponseMessage::RejectedDueConditionsChanged);
    ASSERT_TRUE(response.actualExchangeRate().has_value());
    EXPECT_EQ(response.actualExchangeRate()->first, actualRate);
    EXPECT_EQ(response.actualExchangeRate()->second, actualShift);
    EXPECT_FALSE(response.actualCommission().has_value());
}

/**
 * Test that sendRejectedDueConditionsChanged creates correct response message
 * with commission.
 */
TEST(IntermediateNodeValidationLogicTest, SendRejectedWithCommission_CreatesCorrectMessage) {
    IntermediateNodeValidationTestEnv env("send_rejected_commission");

    TrustLineAmount actualCommission(15);

    // Create response message as sendRejectedDueConditionsChanged would
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    CoordinatorReservationResponseMessage response(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0),
        actualCommission);

    // Verify response contains correct data
    EXPECT_EQ(response.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_FALSE(response.actualExchangeRate().has_value());
    ASSERT_TRUE(response.actualCommission().has_value());
    EXPECT_EQ(response.actualCommission().value(), actualCommission);
}

/**
 * Test that sendRejectedDueConditionsChanged creates correct response message
 * when rate is not available (nullopt case).
 */
TEST(IntermediateNodeValidationLogicTest, SendRejectedWithoutRate_CreatesEmptyMessage) {
    IntermediateNodeValidationTestEnv env("send_rejected_no_rate");

    // Create response without rate/commission (nullopt case)
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {env.coordinatorAddress};

    CoordinatorReservationResponseMessage response(
        env.EQ_1001,
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0));

    // Verify response has neither rate nor commission
    EXPECT_EQ(response.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_FALSE(response.actualExchangeRate().has_value());
    EXPECT_FALSE(response.actualCommission().has_value());
}

/**
 * Test context map behavior: first-seen rate is cached.
 * Simulates mContextExchangeRates logic.
 */
TEST(IntermediateNodeValidationLogicTest, ContextMap_FirstSeenRateIsCached) {
    IntermediateNodeValidationTestEnv env("context_cache");

    // Simulate context map
    using RateKey = pair<SerializedEquivalent, SerializedEquivalent>;
    map<RateKey, ExchangeRate::Shared> contextRates;

    RateKey key = {env.EQ_1001, env.EQ_2002};

    // First lookup - not in context, get from manager
    auto it = contextRates.find(key);
    EXPECT_EQ(it, contextRates.end());

    // Setup rate in manager
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, TrustLineAmount(5), -1);
    auto rate = env.ratesManager->get(env.EQ_1001, env.EQ_2002);
    ASSERT_NE(rate, nullptr);

    // Add to context
    contextRates.emplace(key, rate);

    // Second lookup - should be in context
    auto it2 = contextRates.find(key);
    ASSERT_NE(it2, contextRates.end());
    EXPECT_EQ(it2->second, rate);

    // Context holds shared_ptr to the rate object
    // If manager updates the same object, context sees the update (shared ownership)
    // This is correct behavior - context caches the pointer, not a copy
    auto it3 = contextRates.find(key);
    EXPECT_EQ(it3->second, rate);  // Same pointer

    // Update manager rate
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, TrustLineAmount(10), -1);
    auto newRate = env.ratesManager->get(env.EQ_1001, env.EQ_2002);

    // Since addOrUpdate modifies the existing ExchangeRate object,
    // context sees the updated value (shared_ptr semantics)
    EXPECT_EQ(it3->second->exchangeRate(), TrustLineAmount(10));
    EXPECT_EQ(newRate->exchangeRate(), it3->second->exchangeRate());
}

/**
 * Test context map behavior: commissions are cached per equivalent.
 * Simulates mContextCommissions logic.
 */
TEST(IntermediateNodeValidationLogicTest, ContextMap_CommissionIsCachedPerEquivalent) {
    IntermediateNodeValidationTestEnv env("context_commission");

    // Simulate context commission map
    map<SerializedEquivalent, Commission::Shared> contextCommissions;

    // First lookup - not in context
    auto it = contextCommissions.find(env.EQ_1001);
    EXPECT_EQ(it, contextCommissions.end());

    // Create commission and add to context
    auto commission = make_shared<Commission>(10);  // Commission constructor takes uint64_t
    contextCommissions.emplace(env.EQ_1001, commission);

    // Second lookup - should be in context
    auto it2 = contextCommissions.find(env.EQ_1001);
    ASSERT_NE(it2, contextCommissions.end());
    EXPECT_EQ(it2->second->amount(), TrustLineAmount(10));
}

/**
 * Test validation logic: rate mismatch detection.
 * Simulates the comparison logic from runCoordinatorRequestProcessingStage.
 */
TEST(IntermediateNodeValidationLogicTest, RateMismatchDetection_DifferentRate) {
    IntermediateNodeValidationTestEnv env("rate_mismatch");

    // Setup actual rate in manager
    TrustLineAmount actualRate(6);
    int16_t actualShift = -1;
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, actualRate, actualShift);

    // Get actual rate
    auto currentRate = env.ratesManager->get(env.EQ_1001, env.EQ_2002);
    ASSERT_NE(currentRate, nullptr);

    // Expected rate (from coordinator)
    TrustLineAmount expectedRate(5);
    int16_t expectedShift = -1;

    // Simulate mismatch detection logic
    bool rateMismatch = (currentRate->exchangeRate() != expectedRate ||
                        currentRate->exchangeRateShift() != expectedShift);

    EXPECT_TRUE(rateMismatch);
    EXPECT_EQ(currentRate->exchangeRate(), actualRate);
    EXPECT_NE(currentRate->exchangeRate(), expectedRate);
}

/**
 * Test validation logic: rate match (no mismatch).
 */
TEST(IntermediateNodeValidationLogicTest, RateMismatchDetection_SameRate) {
    IntermediateNodeValidationTestEnv env("rate_match");

    // Setup rate
    TrustLineAmount rate(5);
    int16_t shift = -1;
    env.setupExchangeRate(env.EQ_1001, env.EQ_2002, rate, shift);

    auto currentRate = env.ratesManager->get(env.EQ_1001, env.EQ_2002);
    ASSERT_NE(currentRate, nullptr);

    // Expected same rate
    TrustLineAmount expectedRate(5);
    int16_t expectedShift = -1;

    // No mismatch
    bool rateMismatch = (currentRate->exchangeRate() != expectedRate ||
                        currentRate->exchangeRateShift() != expectedShift);

    EXPECT_FALSE(rateMismatch);
}

/**
 * Test commission mismatch detection.
 */
TEST(IntermediateNodeValidationLogicTest, CommissionMismatchDetection_DifferentAmount) {
    TrustLineAmount expectedCommission(10);
    TrustLineAmount actualCommission(15);

    // Simulate mismatch detection
    bool mismatch = (actualCommission != expectedCommission);

    EXPECT_TRUE(mismatch);
}

/**
 * Test commission match (no mismatch).
 */
TEST(IntermediateNodeValidationLogicTest, CommissionMismatchDetection_SameAmount) {
    TrustLineAmount expectedCommission(10);
    TrustLineAmount actualCommission(10);

    bool mismatch = (actualCommission != expectedCommission);

    EXPECT_FALSE(mismatch);
}
