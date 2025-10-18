#include <gtest/gtest.h>
#include <cstring>

#include "core/network/messages/payments/base/RequestMessageWithReservations.h"
#include "core/network/messages/payments/IntermediateNodeReservationRequestMessage.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/common/Types.h"
#include "core/transactions/transactions/regular/payments/base/PathReservation.h"

// Test Category 11: RequestMessageWithReservations

// Test 11.1: testRequestMessageWithReservationsNewConstructor
TEST(RequestMessageWithReservations, NewConstructor)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    vector<PathReservation> reservations = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(1), PathReservation::Incoming),
        PathReservation(PathID(2), make_shared<const TrustLineAmount>(200), SerializedEquivalent(2), PathReservation::Outgoing)
    };

    IntermediateNodeReservationRequestMessage message(equivalent, senderAddresses, uuid, reservations);

    const auto& config = message.finalAmountsConfiguration();
    ASSERT_EQ(config.size(), 2);
    EXPECT_EQ(config[0].pathID, PathID(1));
    EXPECT_EQ(config[0].equivalent, SerializedEquivalent(1));
    EXPECT_EQ(*config[0].amount, TrustLineAmount(100));
    EXPECT_EQ(config[0].direction, PathReservation::Incoming);

    EXPECT_EQ(config[1].pathID, PathID(2));
    EXPECT_EQ(config[1].equivalent, SerializedEquivalent(2));
    EXPECT_EQ(*config[1].amount, TrustLineAmount(200));
    EXPECT_EQ(config[1].direction, PathReservation::Outgoing);
}

// Test 11.2: testRequestMessageWithReservationsDeprecatedConstructor
TEST(RequestMessageWithReservations, DeprecatedConstructor)
{
    SerializedEquivalent equivalent(3);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("bbbbbbbb-cccc-4ddd-8eee-ffffffffffff"));

    vector<pair<PathID, ConstSharedTrustLineAmount>> reservations = {
        {PathID(10), make_shared<const TrustLineAmount>(500)},
        {PathID(20), make_shared<const TrustLineAmount>(750)}
    };

    IntermediateNodeReservationRequestMessage message(equivalent, senderAddresses, uuid, reservations);

    const auto& config = message.finalAmountsConfiguration();
    ASSERT_EQ(config.size(), 2);

    // Should use mEquivalent (3) for all reservations
    EXPECT_EQ(config[0].equivalent, SerializedEquivalent(3));
    EXPECT_EQ(config[1].equivalent, SerializedEquivalent(3));

    EXPECT_EQ(config[0].pathID, PathID(10));
    EXPECT_EQ(*config[0].amount, TrustLineAmount(500));

    EXPECT_EQ(config[1].pathID, PathID(20));
    EXPECT_EQ(*config[1].amount, TrustLineAmount(750));
}

// Test 11.3: testRequestMessageWithReservationsSerializationWithEquivalents
TEST(RequestMessageWithReservations, SerializationWithEquivalents)
{
    SerializedEquivalent equivalent(5);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:3000")
    };
    const TransactionUUID uuid(string("cccccccc-dddd-4eee-8fff-000000000000"));

    vector<PathReservation> reservations = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(10), PathReservation::Incoming),
        PathReservation(PathID(2), make_shared<const TrustLineAmount>(200), SerializedEquivalent(20), PathReservation::Outgoing)
    };

    IntermediateNodeReservationRequestMessage message(equivalent, senderAddresses, uuid, reservations);

    // Serialize via base Message class
    const Message &baseMessage = message;
    auto [buffer, size] = baseMessage.serializeToBytes();

    ASSERT_GT(size, 0);
    ASSERT_NE(buffer, nullptr);

    // Verify that serialized buffer contains both equivalents (10 and 20)
    // We scan the buffer for these values to ensure they were serialized
    bool foundEquivalent10 = false;
    bool foundEquivalent20 = false;

    // Scan buffer for our equivalent values
    // Note: This is a simplified check that looks for the 4-byte equivalent values
    for (size_t i = 0; i + sizeof(SerializedEquivalent) <= size; i++) {
        SerializedEquivalent value;
        memcpy(&value, buffer.get() + i, sizeof(SerializedEquivalent));

        if (value == 10) {
            foundEquivalent10 = true;
        }
        if (value == 20) {
            foundEquivalent20 = true;
        }
    }

    EXPECT_TRUE(foundEquivalent10) << "Equivalent 10 should be present in serialized data";
    EXPECT_TRUE(foundEquivalent20) << "Equivalent 20 should be present in serialized data";
}

// Test 11.4: testRequestMessageWithReservationsDeserializationWithEquivalents
TEST(RequestMessageWithReservations, DeserializationWithEquivalents)
{
    SerializedEquivalent equivalent(7);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:4000")
    };
    const TransactionUUID uuid(string("dddddddd-eeee-4fff-8000-111111111111"));

    // Create message, serialize, then deserialize
    vector<PathReservation> reservations = {
        PathReservation(PathID(5), make_shared<const TrustLineAmount>(300), SerializedEquivalent(10), PathReservation::Incoming),
        PathReservation(PathID(6), make_shared<const TrustLineAmount>(400), SerializedEquivalent(20), PathReservation::Outgoing)
    };

    IntermediateNodeReservationRequestMessage originalMessage(equivalent, senderAddresses, uuid, reservations);

    // Serialize via base Message class
    const Message &baseMessage = originalMessage;
    auto [buffer, size] = baseMessage.serializeToBytes();

    IntermediateNodeReservationRequestMessage deserializedMessage(buffer);

    const auto& config = deserializedMessage.finalAmountsConfiguration();
    ASSERT_EQ(config.size(), 2);
    EXPECT_EQ(config[0].pathID, PathID(5));
    EXPECT_EQ(config[0].equivalent, SerializedEquivalent(10));
    EXPECT_EQ(*config[0].amount, TrustLineAmount(300));
    EXPECT_EQ(config[0].direction, PathReservation::Incoming);

    EXPECT_EQ(config[1].pathID, PathID(6));
    EXPECT_EQ(config[1].equivalent, SerializedEquivalent(20));
    EXPECT_EQ(*config[1].amount, TrustLineAmount(400));
    EXPECT_EQ(config[1].direction, PathReservation::Outgoing);
}

// Test 11.5: testRequestMessageWithReservationsRoundTrip
TEST(RequestMessageWithReservations, RoundTrip)
{
    SerializedEquivalent equivalent(9);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:5000")
    };
    const TransactionUUID uuid(string("eeeeeeee-ffff-4000-8111-222222222222"));

    vector<PathReservation> reservations = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(1), PathReservation::Incoming),
        PathReservation(PathID(2), make_shared<const TrustLineAmount>(200), SerializedEquivalent(2), PathReservation::Outgoing),
        PathReservation(PathID(3), make_shared<const TrustLineAmount>(300), SerializedEquivalent(3), PathReservation::Incoming)
    };

    IntermediateNodeReservationRequestMessage originalMessage(equivalent, senderAddresses, uuid, reservations);

    // Serialize via base Message class
    const Message &baseMessage = originalMessage;
    auto [buffer, size] = baseMessage.serializeToBytes();

    IntermediateNodeReservationRequestMessage deserializedMessage(buffer);

    const auto& config = deserializedMessage.finalAmountsConfiguration();
    ASSERT_EQ(config.size(), 3);

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(config[i].pathID, reservations[i].pathID);
        EXPECT_EQ(*config[i].amount, *reservations[i].amount);
        EXPECT_EQ(config[i].equivalent, reservations[i].equivalent);
        EXPECT_EQ(config[i].direction, reservations[i].direction);
    }

    // Verify transaction UUID and equivalent are preserved
    EXPECT_EQ(deserializedMessage.transactionUUID(), uuid);
    EXPECT_EQ(deserializedMessage.equivalent(), equivalent);
}

// Test 11.6: testRequestMessageWithReservationsBackwardCompatibility
TEST(RequestMessageWithReservations, BackwardCompatibility)
{
    // Deprecated constructor should produce same format as new constructor
    SerializedEquivalent equivalent(5);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:6000")
    };
    const TransactionUUID uuid(string("ffffffff-0000-4111-8222-333333333333"));

    vector<pair<PathID, ConstSharedTrustLineAmount>> deprecatedReservations = {
        {PathID(1), make_shared<const TrustLineAmount>(100)}
    };

    vector<PathReservation> newReservations = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(5), PathReservation::Incoming)
    };

    IntermediateNodeReservationRequestMessage deprecatedMessage(equivalent, senderAddresses, uuid, deprecatedReservations);

    // Create new message with same UUID for comparison
    vector<BaseAddress::Shared> senderAddresses2 = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:6000")
    };
    IntermediateNodeReservationRequestMessage newMessage(equivalent, senderAddresses2, uuid, newReservations);

    // Serialize via base Message class
    const Message &baseDeprecatedMessage = deprecatedMessage;
    auto [deprecatedBuffer, deprecatedSize] = baseDeprecatedMessage.serializeToBytes();

    const Message &baseNewMessage = newMessage;
    auto [newBuffer, newSize] = baseNewMessage.serializeToBytes();

    // Deserialize both and verify they produce the same final configuration
    IntermediateNodeReservationRequestMessage deprecatedDeserialized(deprecatedBuffer);
    IntermediateNodeReservationRequestMessage newDeserialized(newBuffer);

    const auto& deprecatedConfig = deprecatedDeserialized.finalAmountsConfiguration();
    const auto& newConfig = newDeserialized.finalAmountsConfiguration();

    ASSERT_EQ(deprecatedConfig.size(), newConfig.size());
    EXPECT_EQ(deprecatedConfig[0].pathID, newConfig[0].pathID);
    EXPECT_EQ(*deprecatedConfig[0].amount, *newConfig[0].amount);
    EXPECT_EQ(deprecatedConfig[0].equivalent, newConfig[0].equivalent);
}
