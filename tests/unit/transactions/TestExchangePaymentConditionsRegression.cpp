#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>

#include "../../../src/core/network/messages/payments/CoordinatorReservationRequestMessage.h"
#include "../../../src/core/network/messages/payments/CoordinatorReservationResponseMessage.h"
#include "../../../src/core/network/messages/payments/base/ResponseMessage.h"
#include "../../../src/core/transactions/transactions/regular/payments/base/PathReservation.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/rates/manager/ExchangeRatesManager.h"
#include "../../../src/core/rates/manager/CommissionsManager.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/paths/lib/OptimalPathResult.h"

using namespace testing;
using namespace std;

namespace {
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }
}

// ======================================================================================
// Exchange Payment Conditions Regression Tests (PRD 09)
// These tests verify that condition change handling doesn't break existing functionality
// ======================================================================================

// Regression Test Suite 1: Message Backward Compatibility
TEST(ExchangePaymentRegressionTest, RequestMessageWithoutConditions_StillWorks) {
    // Old-style request message without any condition fields should still work
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };
    BaseAddress::Shared nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(
        PathID(1), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));

    // Create message without any expected conditions (backward compatible)
    CoordinatorReservationRequestMessage msg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        finalCfg,
        nextNode);

    // Verify message doesn't have condition fields
    EXPECT_FALSE(msg.expectedExchangeRate().has_value());
    EXPECT_FALSE(msg.expectedCommission().has_value());

    // Message should be valid and serializable
    auto [buffer, size] = msg.serializeToBytes();
    EXPECT_GT(size, 0);
}

TEST(ExchangePaymentRegressionTest, ResponseAccepted_WorksWithoutConditionFields) {
    // Response with Accepted state should work without condition fields
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::Accepted,
        TrustLineAmount(100));

    EXPECT_EQ(responseMsg.state(), ResponseMessage::Accepted);
    EXPECT_EQ(responseMsg.amountReserved(), TrustLineAmount(100));
    EXPECT_FALSE(responseMsg.actualExchangeRate().has_value());
    EXPECT_FALSE(responseMsg.actualCommission().has_value());
}

TEST(ExchangePaymentRegressionTest, ResponseRejected_WorksWithoutConditionFields) {
    // Response with Rejected state should work without condition fields
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::Rejected,
        TrustLineAmount(0));

    EXPECT_EQ(responseMsg.state(), ResponseMessage::Rejected);
    EXPECT_FALSE(responseMsg.actualExchangeRate().has_value());
    EXPECT_FALSE(responseMsg.actualCommission().has_value());
}

TEST(ExchangePaymentRegressionTest, AllExistingResponseStates_StillSupported) {
    // Verify all existing response states still work
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    vector<ResponseMessage::OperationState> states = {
        ResponseMessage::Accepted,
        ResponseMessage::Rejected,
        ResponseMessage::RejectedDueOwnKeysAbsence,
        ResponseMessage::RejectedDueContractorKeysAbsence,
        ResponseMessage::RejectedDueAuditPending,
        ResponseMessage::Closed,
        ResponseMessage::NextNodeInaccessible
    };

    for (auto state : states) {
        CoordinatorReservationResponseMessage responseMsg(
            SerializedEquivalent(1001),
            senderAddrs,
            txnUUID,
            PathID(1),
            state,
            TrustLineAmount(0));

        EXPECT_EQ(responseMsg.state(), state) << "State " << static_cast<int>(state) << " failed";
    }
}

// Regression Test Suite 2: Single-Equivalent Payments
TEST(ExchangePaymentRegressionTest, SingleEquivalentMessage_NoExchangeRateNeeded) {
    // Single-equivalent payments don't need exchange rates
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };
    BaseAddress::Shared nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(
        PathID(1), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));

    // Single-equivalent payment - no exchange rate
    CoordinatorReservationRequestMessage msg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        finalCfg,
        nextNode);

    EXPECT_FALSE(msg.expectedExchangeRate().has_value());
}

TEST(ExchangePaymentRegressionTest, SingleEquivalentWithCommission_StillWorks) {
    // Single-equivalent payment with commission (no exchange)
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };
    BaseAddress::Shared nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(
        PathID(1), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));

    TrustLineAmount commission(10);

    CoordinatorReservationRequestMessage msg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        finalCfg,
        nextNode,
        commission);

    EXPECT_FALSE(msg.expectedExchangeRate().has_value());
    EXPECT_TRUE(msg.expectedCommission().has_value());
    EXPECT_EQ(msg.expectedCommission().value(), commission);
}

// Regression Test Suite 3: ExchangeRatesManager Compatibility
TEST(ExchangePaymentRegressionTest, ExchangeRatesManager_BasicOperations) {
    Logger logger;
    boost::asio::io_context io;
    ExchangeRatesManager ratesManager(io, logger);

    // Basic add/get/remove operations should work
    SerializedEquivalent fromEquiv(1001);
    SerializedEquivalent toEquiv(2002);
    TrustLineAmount rate(5);
    int16_t shift = -1;

    // Add
    ratesManager.addOrUpdate(fromEquiv, toEquiv, rate, shift,
                            TrustLineAmount(0), TrustLineAmount(0));
    auto retrieved = ratesManager.get(fromEquiv, toEquiv);
    ASSERT_NE(retrieved, nullptr);

    // Remove
    ratesManager.remove(fromEquiv, toEquiv);
    auto afterRemove = ratesManager.get(fromEquiv, toEquiv);
    EXPECT_EQ(afterRemove, nullptr);
}

TEST(ExchangePaymentRegressionTest, ExchangeRatesManager_ListOperations) {
    Logger logger;
    boost::asio::io_context io;
    ExchangeRatesManager ratesManager(io, logger);

    // Add multiple rates
    ratesManager.addOrUpdate(SerializedEquivalent(1001), SerializedEquivalent(2002),
                            TrustLineAmount(5), -1, TrustLineAmount(0), TrustLineAmount(0));
    ratesManager.addOrUpdate(SerializedEquivalent(2002), SerializedEquivalent(3003),
                            TrustLineAmount(3), -1, TrustLineAmount(0), TrustLineAmount(0));

    // List should return both
    auto rates = ratesManager.list();
    EXPECT_GE(rates.size(), 2);
}

TEST(ExchangePaymentRegressionTest, ExchangeRatesManager_ClearOperation) {
    Logger logger;
    boost::asio::io_context io;
    ExchangeRatesManager ratesManager(io, logger);

    // Add rates
    ratesManager.addOrUpdate(SerializedEquivalent(1001), SerializedEquivalent(2002),
                            TrustLineAmount(5), -1, TrustLineAmount(0), TrustLineAmount(0));

    // Clear all
    ratesManager.clear();

    // Should be empty
    auto rates = ratesManager.list();
    EXPECT_EQ(rates.size(), 0);
}

// Regression Test Suite 4: Path Processing
TEST(ExchangePaymentRegressionTest, OptimalPathResult_BasicFields) {
    // OptimalPathResult basic fields should still work
    OptimalPathResult pathResult;

    pathResult.mMaxPathFlow = TrustLineAmount(100);
    pathResult.optimal_flow = TrustLineAmount(80);
    pathResult.received_amount = TrustLineAmount(75);

    EXPECT_EQ(pathResult.mMaxPathFlow, TrustLineAmount(100));
    EXPECT_EQ(pathResult.optimal_flow, TrustLineAmount(80));
    EXPECT_EQ(pathResult.received_amount, TrustLineAmount(75));
}

TEST(ExchangePaymentRegressionTest, PathReservation_Construction) {
    // PathReservation construction should work with all parameters
    PathID pathID(1);
    ConstSharedTrustLineAmount amount = A(100);
    SerializedEquivalent equivalent(1001);
    PathReservation::Direction direction = PathReservation::Outgoing;

    PathReservation reservation(pathID, amount, equivalent, direction);

    // Basic construction should not throw
    SUCCEED();
}

TEST(ExchangePaymentRegressionTest, PathReservation_IncomingDirection) {
    // Incoming direction should also work
    PathReservation reservation(
        PathID(1), A(100), SerializedEquivalent(1001), PathReservation::Incoming);

    SUCCEED();
}

// Regression Test Suite 5: Message Serialization
TEST(ExchangePaymentRegressionTest, RequestMessage_Serialization_WithoutConditions) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };
    BaseAddress::Shared nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(
        PathID(1), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));

    CoordinatorReservationRequestMessage msg(
        SerializedEquivalent(1001), senderAddrs, txnUUID, finalCfg, nextNode);

    // Serialize
    auto [buffer, size] = msg.serializeToBytes();
    EXPECT_GT(size, 0);

    // Deserialize
    CoordinatorReservationRequestMessage deserialized(buffer);

    // Should match
    EXPECT_FALSE(deserialized.expectedExchangeRate().has_value());
    EXPECT_FALSE(deserialized.expectedCommission().has_value());
}

TEST(ExchangePaymentRegressionTest, ResponseMessage_Serialization_Accepted) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    CoordinatorReservationResponseMessage msg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::Accepted,
        TrustLineAmount(100));

    // Serialize
    auto [buffer, size] = msg.serializeToBytes();
    EXPECT_GT(size, 0);

    // Deserialize
    CoordinatorReservationResponseMessage deserialized(buffer);

    EXPECT_EQ(deserialized.state(), ResponseMessage::Accepted);
    EXPECT_EQ(deserialized.amountReserved(), TrustLineAmount(100));
}

// Regression Test Suite 6: Edge Cases
TEST(ExchangePaymentRegressionTest, ZeroAmountReservation) {
    // Zero amount should be handled
    PathReservation reservation(
        PathID(1), A(0), SerializedEquivalent(1001), PathReservation::Outgoing);

    SUCCEED();
}

TEST(ExchangePaymentRegressionTest, LargeAmountReservation) {
    // Very large amounts should work
    TrustLineAmount largeAmount(0);
    largeAmount += 1;
    largeAmount <<= 100;

    PathReservation reservation(
        PathID(1), make_shared<TrustLineAmount>(largeAmount),
        SerializedEquivalent(1001), PathReservation::Outgoing);

    SUCCEED();
}

TEST(ExchangePaymentRegressionTest, MultiplePathReservations) {
    // Multiple path reservations should work
    vector<PathReservation> finalCfg;
    for (int i = 1; i <= 5; i++) {
        finalCfg.push_back(PathReservation(
            PathID(i), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));
    }

    EXPECT_EQ(finalCfg.size(), 5);
}

// Regression Test Suite 7: Commission Manager
TEST(ExchangePaymentRegressionTest, CommissionsManager_Construction) {
    Logger logger;

    // CommissionsManager should construct with initial commissions
    vector<pair<SerializedEquivalent, uint64_t>> initialCommissions = {
        {SerializedEquivalent(1001), 10},
        {SerializedEquivalent(2002), 20}
    };

    CommissionsManager commissionsManager(logger, initialCommissions);

    // Basic construction should work
    SUCCEED();
}

TEST(ExchangePaymentRegressionTest, CommissionsManager_GetCommission) {
    Logger logger;

    vector<pair<SerializedEquivalent, uint64_t>> initialCommissions = {
        {SerializedEquivalent(1001), 10}
    };

    CommissionsManager commissionsManager(logger, initialCommissions);

    auto commission = commissionsManager.getCommission(SerializedEquivalent(1001));
    ASSERT_NE(commission, nullptr);
}

// Regression Test Suite 8: Error States
TEST(ExchangePaymentRegressionTest, ResponseMessage_AuditPending) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueAuditPending,
        TrustLineAmount(0));

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueAuditPending);
}

TEST(ExchangePaymentRegressionTest, ResponseMessage_KeysAbsence) {
    TransactionUUID txnUUID;
    vector<BaseAddress::Shared> senderAddrs = {
        make_shared<IPv4WithPortAddress>("192.168.1.1:8080")
    };

    CoordinatorReservationResponseMessage responseMsg(
        SerializedEquivalent(1001),
        senderAddrs,
        txnUUID,
        PathID(1),
        ResponseMessage::RejectedDueOwnKeysAbsence,
        TrustLineAmount(0));

    EXPECT_EQ(responseMsg.state(), ResponseMessage::RejectedDueOwnKeysAbsence);
}

// Regression Test Suite 9: Multiple Equivalents
TEST(ExchangePaymentRegressionTest, DifferentEquivalents_InSameMessage) {
    vector<PathReservation> finalCfg;

    // Path 1: equivalent 1001
    finalCfg.push_back(PathReservation(
        PathID(1), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));

    // Path 2: equivalent 2002
    finalCfg.push_back(PathReservation(
        PathID(2), A(200), SerializedEquivalent(2002), PathReservation::Incoming));

    EXPECT_EQ(finalCfg.size(), 2);
}

TEST(ExchangePaymentRegressionTest, ExchangeRatesManager_BidirectionalRates) {
    Logger logger;
    boost::asio::io_context io;
    ExchangeRatesManager ratesManager(io, logger);

    // Add rates in both directions
    ratesManager.addOrUpdate(SerializedEquivalent(1001), SerializedEquivalent(2002),
                            TrustLineAmount(5), -1, TrustLineAmount(0), TrustLineAmount(0));
    ratesManager.addOrUpdate(SerializedEquivalent(2002), SerializedEquivalent(1001),
                            TrustLineAmount(2), -1, TrustLineAmount(0), TrustLineAmount(0));

    // Both should exist
    auto rate1 = ratesManager.get(SerializedEquivalent(1001), SerializedEquivalent(2002));
    auto rate2 = ratesManager.get(SerializedEquivalent(2002), SerializedEquivalent(1001));

    EXPECT_NE(rate1, nullptr);
    EXPECT_NE(rate2, nullptr);
}

// Regression Test Suite 10: Performance and Memory
TEST(ExchangePaymentRegressionTest, ManyPaths_NoMemoryLeak) {
    // Create and destroy many path reservations
    for (int i = 0; i < 100; i++) {
        vector<PathReservation> finalCfg;
        for (int j = 0; j < 10; j++) {
            finalCfg.push_back(PathReservation(
                PathID(j), A(100), SerializedEquivalent(1001), PathReservation::Outgoing));
        }
    }
    // If we get here without crash, no obvious memory issues
    SUCCEED();
}

TEST(ExchangePaymentRegressionTest, ManyExchangeRates_Performance) {
    Logger logger;
    boost::asio::io_context io;
    ExchangeRatesManager ratesManager(io, logger);

    // Add many rates
    for (int i = 1000; i < 1100; i++) {
        ratesManager.addOrUpdate(
            SerializedEquivalent(i),
            SerializedEquivalent(i + 1000),
            TrustLineAmount(5), -1,
            TrustLineAmount(0), TrustLineAmount(0));
    }

    auto rates = ratesManager.list();
    EXPECT_EQ(rates.size(), 100);
}
