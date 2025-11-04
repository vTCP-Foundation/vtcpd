#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "../../../../src/core/network/messages/payments/CoordinatorReservationRequestMessage.h"
#include "../../../../src/core/network/messages/payments/CoordinatorReservationResponseMessage.h"
#include "../../../../src/core/network/messages/payments/base/ResponseMessage.h"
#include "../../../../src/core/transactions/transactions/regular/payments/base/PathReservation.h"
#include "../../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../../src/core/common/Types.h"

using namespace std;

namespace {
    // Helper to create shared TrustLineAmount
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }
}

// ======================================================================================
// CoordinatorReservationRequestMessage Tests
// ======================================================================================

// Test 1: Serialize/deserialize with exchange rate
TEST(CoordinatorReservationRequestMessageTest, SerializeWithExchangeRate) {
    // Arrange
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    TrustLineAmount expectedRate(5);
    int16_t expectedShift = -1;

    // Act: Create message with exchange rate
    CoordinatorReservationRequestMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        finalCfg,
        nextNode,
        expectedRate,
        expectedShift);

    // Serialize
    auto [buffer, size] = original.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, 0u);

    // Deserialize
    CoordinatorReservationRequestMessage deserialized(buffer);

    // Assert
    EXPECT_EQ(deserialized.equivalent(), equivalent);
    EXPECT_EQ(deserialized.transactionUUID(), txnUUID);

    EXPECT_TRUE(deserialized.expectedExchangeRate().has_value());
    EXPECT_EQ(deserialized.expectedExchangeRate()->first, expectedRate);
    EXPECT_EQ(deserialized.expectedExchangeRate()->second, expectedShift);

    EXPECT_FALSE(deserialized.expectedCommission().has_value());
}

// Test 2: Serialize/deserialize with commission
TEST(CoordinatorReservationRequestMessageTest, SerializeWithCommission) {
    // Arrange
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("bbbbbbbb-cccc-4ddd-8eee-ffffffffffff");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(2), A(200), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.3:8080");

    TrustLineAmount expectedCommission(10);

    // Act: Create message with commission
    CoordinatorReservationRequestMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        finalCfg,
        nextNode,
        expectedCommission);

    // Serialize
    auto [buffer, size] = original.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, 0u);

    // Deserialize
    CoordinatorReservationRequestMessage deserialized(buffer);

    // Assert
    EXPECT_EQ(deserialized.equivalent(), equivalent);
    EXPECT_EQ(deserialized.transactionUUID(), txnUUID);

    EXPECT_FALSE(deserialized.expectedExchangeRate().has_value());

    EXPECT_TRUE(deserialized.expectedCommission().has_value());
    EXPECT_EQ(deserialized.expectedCommission().value(), expectedCommission);
}

// Test 3: Serialize/deserialize with both fields empty
TEST(CoordinatorReservationRequestMessageTest, SerializeWithBothEmpty) {
    // Arrange
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("cccccccc-dddd-4eee-8fff-000000000000");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(3), A(300), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.4:8080");

    // Act: Create message without exchange rate or commission
    CoordinatorReservationRequestMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        finalCfg,
        nextNode);

    // Serialize
    auto [buffer, size] = original.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, 0u);

    // Deserialize
    CoordinatorReservationRequestMessage deserialized(buffer);

    // Assert
    EXPECT_EQ(deserialized.equivalent(), equivalent);
    EXPECT_EQ(deserialized.transactionUUID(), txnUUID);

    EXPECT_FALSE(deserialized.expectedExchangeRate().has_value());
    EXPECT_FALSE(deserialized.expectedCommission().has_value());
}

// Test 4: Multiple exchange rates with different shifts
TEST(CoordinatorReservationRequestMessageTest, ExchangeRatesWithDifferentShifts) {
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("dddddddd-eeee-4fff-8000-111111111111");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    // Test different exchange rate configurations
    struct TestCase {
        TrustLineAmount rate;
        int16_t shift;
    };

    vector<TestCase> testCases = {
        {TrustLineAmount(2), 0},      // rate = 2.0
        {TrustLineAmount(5), -1},     // rate = 0.5
        {TrustLineAmount(20), 1},     // rate = 200
        {TrustLineAmount(1), -2},     // rate = 0.01
    };

    for (const auto& tc : testCases) {
        CoordinatorReservationRequestMessage original(
            equivalent, senderAddresses, txnUUID, finalCfg, nextNode, tc.rate, tc.shift);

        auto [buffer, size] = original.serializeToBytes();
        CoordinatorReservationRequestMessage deserialized(buffer);

        ASSERT_TRUE(deserialized.expectedExchangeRate().has_value());
        EXPECT_EQ(deserialized.expectedExchangeRate()->first, tc.rate);
        EXPECT_EQ(deserialized.expectedExchangeRate()->second, tc.shift);
    }
}

// ======================================================================================
// CoordinatorReservationResponseMessage Tests
// ======================================================================================

// Test 5: Serialize/deserialize with actual exchange rate
TEST(CoordinatorReservationResponseMessageTest, SerializeWithActualExchangeRate) {
    // Arrange
    const SerializedEquivalent equivalent = 2002;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.2.1:8080"));

    const TransactionUUID txnUUID("eeeeeeee-ffff-4000-8111-222222222222");
    const PathID pathID(1);

    TrustLineAmount reservedAmount(100);
    TrustLineAmount actualRate(6);
    int16_t actualShift = -1;

    // Act: Create response with actual exchange rate
    CoordinatorReservationResponseMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        pathID,
        ResponseMessage::RejectedDueConditionsChanged,
        reservedAmount,
        actualRate,
        actualShift);

    // Serialize
    auto [buffer, size] = original.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, 0u);

    // Deserialize
    CoordinatorReservationResponseMessage deserialized(buffer);

    // Assert
    EXPECT_EQ(deserialized.equivalent(), equivalent);
    EXPECT_EQ(deserialized.transactionUUID(), txnUUID);
    EXPECT_EQ(deserialized.pathID(), pathID);
    EXPECT_EQ(deserialized.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_EQ(deserialized.amountReserved(), reservedAmount);

    EXPECT_TRUE(deserialized.actualExchangeRate().has_value());
    EXPECT_EQ(deserialized.actualExchangeRate()->first, actualRate);
    EXPECT_EQ(deserialized.actualExchangeRate()->second, actualShift);

    EXPECT_FALSE(deserialized.actualCommission().has_value());
}

// Test 6: Serialize/deserialize with actual commission
TEST(CoordinatorReservationResponseMessageTest, SerializeWithActualCommission) {
    // Arrange
    const SerializedEquivalent equivalent = 2002;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.2.1:8080"));

    const TransactionUUID txnUUID("ffffffff-0000-4111-8222-333333333333");
    const PathID pathID(2);

    TrustLineAmount reservedAmount(200);
    TrustLineAmount actualCommission(15);

    // Act: Create response with actual commission
    CoordinatorReservationResponseMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        pathID,
        ResponseMessage::RejectedDueConditionsChanged,
        reservedAmount,
        actualCommission);

    // Serialize
    auto [buffer, size] = original.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, 0u);

    // Deserialize
    CoordinatorReservationResponseMessage deserialized(buffer);

    // Assert
    EXPECT_EQ(deserialized.equivalent(), equivalent);
    EXPECT_EQ(deserialized.transactionUUID(), txnUUID);
    EXPECT_EQ(deserialized.pathID(), pathID);
    EXPECT_EQ(deserialized.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_EQ(deserialized.amountReserved(), reservedAmount);

    EXPECT_FALSE(deserialized.actualExchangeRate().has_value());

    EXPECT_TRUE(deserialized.actualCommission().has_value());
    EXPECT_EQ(deserialized.actualCommission().value(), actualCommission);
}

// Test 7: Serialize/deserialize with both fields empty
TEST(CoordinatorReservationResponseMessageTest, SerializeWithBothEmpty) {
    // Arrange
    const SerializedEquivalent equivalent = 2002;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.2.1:8080"));

    const TransactionUUID txnUUID("00000000-1111-4222-8333-444444444444");
    const PathID pathID(3);

    TrustLineAmount reservedAmount(0);

    // Act: Create response without actual exchange rate or commission
    CoordinatorReservationResponseMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        pathID,
        ResponseMessage::Accepted,
        reservedAmount);

    // Serialize
    auto [buffer, size] = original.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, 0u);

    // Deserialize
    CoordinatorReservationResponseMessage deserialized(buffer);

    // Assert
    EXPECT_EQ(deserialized.equivalent(), equivalent);
    EXPECT_EQ(deserialized.state(), ResponseMessage::Accepted);

    EXPECT_FALSE(deserialized.actualExchangeRate().has_value());
    EXPECT_FALSE(deserialized.actualCommission().has_value());
}

// ======================================================================================
// ResponseMessage::OperationState Tests
// ======================================================================================

// Test 8: RejectedDueConditionsChanged enum value recognized
TEST(ResponseMessageTest, RejectedDueConditionsChangedRecognized) {
    // Arrange
    const SerializedEquivalent equivalent = 3003;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.3.1:8080"));

    const TransactionUUID txnUUID("11111111-2222-4333-8444-555555555555");
    const PathID pathID(5);

    // Act: Create response with RejectedDueConditionsChanged state
    CoordinatorReservationResponseMessage response(
        equivalent,
        senderAddresses,
        txnUUID,
        pathID,
        ResponseMessage::RejectedDueConditionsChanged,
        TrustLineAmount(0));

    // Assert
    EXPECT_EQ(response.state(), ResponseMessage::RejectedDueConditionsChanged);
    EXPECT_EQ(static_cast<int>(response.state()), 12);
}

// Test 9: All operation states serialization
TEST(ResponseMessageTest, AllOperationStatesSerialization) {
    const SerializedEquivalent equivalent = 3003;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.3.1:8080"));

    const TransactionUUID txnUUID("22222222-3333-4444-8555-666666666666");
    const PathID pathID(1);

    vector<ResponseMessage::OperationState> states = {
        ResponseMessage::Accepted,
        ResponseMessage::Rejected,
        ResponseMessage::RejectedDueOwnKeysAbsence,
        ResponseMessage::RejectedDueContractorKeysAbsence,
        ResponseMessage::RejectedDueAuditPending,
        ResponseMessage::Closed,
        ResponseMessage::NextNodeInaccessible,
        ResponseMessage::RejectedDueConditionsChanged
    };

    for (auto state : states) {
        CoordinatorReservationResponseMessage original(
            equivalent, senderAddresses, txnUUID, pathID, state, TrustLineAmount(0));

        auto [buffer, size] = original.serializeToBytes();
        CoordinatorReservationResponseMessage deserialized(buffer);

        EXPECT_EQ(deserialized.state(), state)
            << "State mismatch for state value " << static_cast<int>(state);
    }
}

// Test 10: Large exchange rate values
TEST(CoordinatorReservationRequestMessageTest, LargeExchangeRateValues) {
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("33333333-4444-4555-8666-777777777777");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    // Test large value exchange rate
    TrustLineAmount largeRate = 0;
    largeRate += 1;
    largeRate <<= 100;  // Very large number
    largeRate += 0xDEADBEEF;

    int16_t shift = -50;

    CoordinatorReservationRequestMessage original(
        equivalent, senderAddresses, txnUUID, finalCfg, nextNode, largeRate, shift);

    auto [buffer, size] = original.serializeToBytes();
    CoordinatorReservationRequestMessage deserialized(buffer);

    ASSERT_TRUE(deserialized.expectedExchangeRate().has_value());
    EXPECT_EQ(deserialized.expectedExchangeRate()->first, largeRate);
    EXPECT_EQ(deserialized.expectedExchangeRate()->second, shift);
}

// Test 11: Zero commission value
TEST(CoordinatorReservationResponseMessageTest, ZeroCommissionValue) {
    const SerializedEquivalent equivalent = 2002;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.2.1:8080"));

    const TransactionUUID txnUUID("44444444-5555-4666-8777-888888888888");
    const PathID pathID(4);

    TrustLineAmount reservedAmount(100);
    TrustLineAmount zeroCommission(0);

    // Act: Create response with zero commission (commission removed)
    CoordinatorReservationResponseMessage original(
        equivalent,
        senderAddresses,
        txnUUID,
        pathID,
        ResponseMessage::RejectedDueConditionsChanged,
        reservedAmount,
        zeroCommission);

    auto [buffer, size] = original.serializeToBytes();
    CoordinatorReservationResponseMessage deserialized(buffer);

    ASSERT_TRUE(deserialized.actualCommission().has_value());
    EXPECT_EQ(deserialized.actualCommission().value(), TrustLineAmount(0));
}

// Test 12: Multiple path reservations in request
TEST(CoordinatorReservationRequestMessageTest, MultiplePathReservations) {
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("55555555-6666-4777-8888-999999999999");

    // Multiple path reservations
    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), equivalent, PathReservation::Outgoing));
    finalCfg.push_back(PathReservation(PathID(2), A(200), equivalent, PathReservation::Outgoing));
    finalCfg.push_back(PathReservation(PathID(3), A(300), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    TrustLineAmount expectedRate(5);
    int16_t expectedShift = -1;

    CoordinatorReservationRequestMessage original(
        equivalent, senderAddresses, txnUUID, finalCfg, nextNode, expectedRate, expectedShift);

    auto [buffer, size] = original.serializeToBytes();
    CoordinatorReservationRequestMessage deserialized(buffer);

    EXPECT_EQ(deserialized.finalAmountsConfiguration().size(), 3);
    EXPECT_TRUE(deserialized.expectedExchangeRate().has_value());
}

// Test 13: Negative shift values
TEST(CoordinatorReservationRequestMessageTest, NegativeShiftValues) {
    const SerializedEquivalent equivalent = 1001;
    vector<BaseAddress::Shared> senderAddresses;
    senderAddresses.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));

    const TransactionUUID txnUUID("66666666-7777-4888-8999-aaaaaaaaaaaa");

    vector<PathReservation> finalCfg;
    finalCfg.push_back(PathReservation(PathID(1), A(100), equivalent, PathReservation::Outgoing));

    auto nextNode = make_shared<IPv4WithPortAddress>("192.168.1.2:8080");

    // Test various negative shift values
    vector<int16_t> shifts = {-1, -2, -5, -10, -100, -32768}; // -32768 is min int16_t

    for (int16_t shift : shifts) {
        CoordinatorReservationRequestMessage original(
            equivalent, senderAddresses, txnUUID, finalCfg, nextNode, TrustLineAmount(5), shift);

        auto [buffer, size] = original.serializeToBytes();
        CoordinatorReservationRequestMessage deserialized(buffer);

        ASSERT_TRUE(deserialized.expectedExchangeRate().has_value());
        EXPECT_EQ(deserialized.expectedExchangeRate()->second, shift)
            << "Shift value mismatch for shift = " << shift;
    }
}
