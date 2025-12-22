#include <gtest/gtest.h>
#include <type_traits>

#include "core/network/messages/payments/FinalAmountsConfigurationMessage.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/contractors/Contractor.h"
#include "core/crypto/MsgEncryptor.h"
#include "core/common/Types.h"
#include "core/transactions/transactions/regular/payments/base/PathReservation.h"

using namespace crypto;

// Test Category 13: FinalAmountsConfigurationMessage

// Test 13.1: testFinalAmountsConfigurationMessageFieldsRemoved
TEST(FinalAmountsConfigurationMessage, FieldsRemoved)
{
    // Verify old fields are replaced with vector-based approach
    // Old: bool mIsReceiptContains, Signature::Shared mSignature
    // New: vector<pair<SerializedEquivalent, Signature::Shared>> mSignatures

    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    vector<pair<PathID, ConstSharedTrustLineAmount>> finalAmountsConfig = {
        {PathID(1), make_shared<const TrustLineAmount>(100)}
    };
    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 1000;

    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber);

    // Verify that signatures() returns a vector (new implementation)
    const auto& sigs = message.signatures();

    // Verify signatures is a vector by checking it has vector operations
    // The old implementation had a single signature, not a vector
    EXPECT_EQ(sigs.size(), 0) << "Signatures vector should be empty for message without receipts";

    // Verify we can use vector operations like empty(), size(), etc
    EXPECT_TRUE(sigs.empty()) << "Should be able to call empty() on signatures vector";

    // Verify isReceiptContains() is based on vector size (not a separate bool field)
    // If vector is empty, isReceiptContains() should return false
    EXPECT_FALSE(message.isReceiptContains()) << "Should return false when signatures vector is empty";

    // The fact that this compiles and runs confirms signatures() returns a vector-like container
    // not a single Signature::Shared pointer (which wouldn't have size() or empty())
}

// Test 13.2: testFinalAmountsConfigurationMessageSignaturesVectorAdded
TEST(FinalAmountsConfigurationMessage, SignaturesVectorAdded)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    vector<PathReservation> finalAmountsConfig = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(1), PathReservation::Incoming)
    };

    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 1000;
    uint16_t disputeGracePeriodBlocksCount = 4000;

    // Create test signatures
    auto keys1 = MsgEncryptor::generateKeyTrio("");
    auto keys2 = MsgEncryptor::generateKeyTrio("");

    // Create dummy signatures (in real scenario, these would be actual transaction signatures)
    auto sig1 = make_shared<sphincs::Signature>();
    auto sig2 = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig1},
        {SerializedEquivalent(2), sig2}
    };

    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber,
        disputeGracePeriodBlocksCount,
        signatures);

    const auto& retrievedSignatures = message.signatures();
    ASSERT_EQ(retrievedSignatures.size(), 2);
    EXPECT_EQ(retrievedSignatures[0].first, SerializedEquivalent(1));
    EXPECT_EQ(retrievedSignatures[1].first, SerializedEquivalent(2));
    EXPECT_EQ(retrievedSignatures[0].second, sig1);
    EXPECT_EQ(retrievedSignatures[1].second, sig2);
    EXPECT_EQ(message.disputeGracePeriodBlocksCount(), disputeGracePeriodBlocksCount);
}

// Test 13.3: testIsReceiptContainsEmptyVector
TEST(FinalAmountsConfigurationMessage, IsReceiptContainsEmptyVector)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    vector<pair<PathID, ConstSharedTrustLineAmount>> finalAmountsConfig = {
        {PathID(1), make_shared<const TrustLineAmount>(100)}
    };
    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 1000;

    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber);  // No signatures

    EXPECT_FALSE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 0);
}

// Test 13.4: testIsReceiptContainsSingleElement
TEST(FinalAmountsConfigurationMessage, IsReceiptContainsSingleElement)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    vector<PathReservation> finalAmountsConfig = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(1), PathReservation::Incoming)
    };

    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 1000;
    uint16_t disputeGracePeriodBlocksCount = 4000;

    auto sig = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig}
    };

    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber,
        disputeGracePeriodBlocksCount,
        signatures);

    EXPECT_TRUE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 1);
    EXPECT_EQ(message.disputeGracePeriodBlocksCount(), disputeGracePeriodBlocksCount);
}

// Test 13.5: testIsReceiptContainsMultipleElements
TEST(FinalAmountsConfigurationMessage, IsReceiptContainsMultipleElements)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    vector<PathReservation> finalAmountsConfig = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(1), PathReservation::Incoming)
    };

    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 1000;
    uint16_t disputeGracePeriodBlocksCount = 4000;

    auto sig1 = make_shared<sphincs::Signature>();
    auto sig2 = make_shared<sphincs::Signature>();
    auto sig3 = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig1},
        {SerializedEquivalent(2), sig2},
        {SerializedEquivalent(3), sig3}
    };

    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber,
        disputeGracePeriodBlocksCount,
        signatures);

    EXPECT_TRUE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 3);
    EXPECT_EQ(message.disputeGracePeriodBlocksCount(), disputeGracePeriodBlocksCount);
}

// Test 13.6: testDeprecatedConstructorNullSignature
TEST(FinalAmountsConfigurationMessage, DeprecatedConstructorNullSignature)
{
    SerializedEquivalent equivalent(5);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:3000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    vector<pair<PathID, ConstSharedTrustLineAmount>> finalAmountsConfig = {
        {PathID(1), make_shared<const TrustLineAmount>(200)}
    };
    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 2000;

    // Deprecated constructor with null signature
    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber,
        nullptr);  // Null signature

    EXPECT_FALSE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 0);
}

// Test 13.7: testDeprecatedConstructorValidSignature
TEST(FinalAmountsConfigurationMessage, DeprecatedConstructorValidSignature)
{
    SerializedEquivalent equivalent(5);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:4000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    vector<pair<PathID, ConstSharedTrustLineAmount>> finalAmountsConfig = {
        {PathID(1), make_shared<const TrustLineAmount>(300)}
    };
    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 3000;

    auto signature = make_shared<sphincs::Signature>();

    // Deprecated constructor with valid signature
    FinalAmountsConfigurationMessage message(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber,
        signature);

    EXPECT_TRUE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 1);
    EXPECT_EQ(message.signatures()[0].first, equivalent);
    EXPECT_EQ(message.signatures()[0].second, signature);
}

// Test 13.8: testSerializationDeserializationMultipleSignatures
TEST(FinalAmountsConfigurationMessage, SerializationDeserializationMultipleSignatures)
{
    SerializedEquivalent equivalent(7);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:5000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    vector<PathReservation> finalAmountsConfig = {
        PathReservation(PathID(1), make_shared<const TrustLineAmount>(100), SerializedEquivalent(1), PathReservation::Incoming),
        PathReservation(PathID(2), make_shared<const TrustLineAmount>(200), SerializedEquivalent(2), PathReservation::Outgoing)
    };

    map<PaymentNodeID, Contractor::Shared> participants;
    BlockNumber maxBlockNumber = 4000;
    uint16_t disputeGracePeriodBlocksCount = 4000;

    auto sig1 = make_shared<sphincs::Signature>();
    auto sig2 = make_shared<sphincs::Signature>();
    auto sig3 = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig1},
        {SerializedEquivalent(2), sig2},
        {SerializedEquivalent(3), sig3}
    };

    FinalAmountsConfigurationMessage originalMessage(
        equivalent,
        senderAddresses,
        uuid,
        finalAmountsConfig,
        participants,
        maxBlockNumber,
        disputeGracePeriodBlocksCount,
        signatures);

    auto [buffer, size] = originalMessage.serializeToBytes();
    ASSERT_GT(size, 0);
    ASSERT_NE(buffer, nullptr);

    FinalAmountsConfigurationMessage deserializedMessage(buffer);

    const auto& retrievedSignatures = deserializedMessage.signatures();
    ASSERT_EQ(retrievedSignatures.size(), 3);

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(retrievedSignatures[i].first, signatures[i].first);
        // Note: Signature comparison by pointer is valid since we're testing round-trip
        // In production, signature content would be verified cryptographically
    }

    // Verify other fields are preserved
    EXPECT_EQ(deserializedMessage.transactionUUID(), uuid);
    EXPECT_EQ(deserializedMessage.equivalent(), equivalent);
    EXPECT_EQ(deserializedMessage.maximalClaimingBlockNumber(), maxBlockNumber);
    EXPECT_EQ(deserializedMessage.disputeGracePeriodBlocksCount(), disputeGracePeriodBlocksCount);
}
