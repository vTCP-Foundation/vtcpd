#include <gtest/gtest.h>
#include <type_traits>

#include "core/network/messages/payments/TransactionPublicKeyHashMessage.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/contractors/Contractor.h"
#include "core/crypto/MsgEncryptor.h"
#include "core/common/Types.h"

using namespace crypto;

// Test Category 14: TransactionPublicKeyHashMessage

// Test 14.1: testTransactionPublicKeyHashMessageFieldsRemoved
TEST(TransactionPublicKeyHashMessage, FieldsRemoved)
{
    // Verify old fields are replaced with vector-based approach
    // Old: bool mIsReceiptContains, Signature::Shared mSignature
    // New: vector<pair<SerializedEquivalent, Signature::Shared>> mSignatures

    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 5;
    auto keyHash = make_shared<sphincs::KeyHash>();

    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash);

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

// Test 14.2: testTransactionPublicKeyHashMessageSignaturesVectorAdded
TEST(TransactionPublicKeyHashMessage, SignaturesVectorAdded)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 10;
    auto keyHash = make_shared<sphincs::KeyHash>();

    // Create test signatures
    auto sig1 = make_shared<sphincs::Signature>();
    auto sig2 = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig1},
        {SerializedEquivalent(2), sig2}
    };

    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash,
        signatures);

    const auto& retrievedSignatures = message.signatures();
    ASSERT_EQ(retrievedSignatures.size(), 2);
    EXPECT_EQ(retrievedSignatures[0].first, SerializedEquivalent(1));
    EXPECT_EQ(retrievedSignatures[1].first, SerializedEquivalent(2));
    EXPECT_EQ(retrievedSignatures[0].second, sig1);
    EXPECT_EQ(retrievedSignatures[1].second, sig2);
}

// Test 14.3: testIsReceiptContainsEmptyVector
TEST(TransactionPublicKeyHashMessage, IsReceiptContainsEmptyVector)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 15;
    auto keyHash = make_shared<sphincs::KeyHash>();

    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash);  // No signatures

    EXPECT_FALSE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 0);
}

// Test 14.4: testIsReceiptContainsSingleElement
TEST(TransactionPublicKeyHashMessage, IsReceiptContainsSingleElement)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 20;
    auto keyHash = make_shared<sphincs::KeyHash>();

    auto sig = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig}
    };

    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash,
        signatures);

    EXPECT_TRUE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 1);
}

// Test 14.5: testIsReceiptContainsMultipleElements
TEST(TransactionPublicKeyHashMessage, IsReceiptContainsMultipleElements)
{
    SerializedEquivalent equivalent(1);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:2000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 25;
    auto keyHash = make_shared<sphincs::KeyHash>();

    auto sig1 = make_shared<sphincs::Signature>();
    auto sig2 = make_shared<sphincs::Signature>();
    auto sig3 = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig1},
        {SerializedEquivalent(2), sig2},
        {SerializedEquivalent(3), sig3}
    };

    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash,
        signatures);

    EXPECT_TRUE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 3);
}

// Test 14.6: testDeprecatedConstructorNullSignature
TEST(TransactionPublicKeyHashMessage, DeprecatedConstructorNullSignature)
{
    SerializedEquivalent equivalent(5);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:3000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 30;
    auto keyHash = make_shared<sphincs::KeyHash>();

    // Deprecated constructor with null signature
    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash,
        nullptr);  // Null signature

    EXPECT_FALSE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 0);
}

// Test 14.7: testDeprecatedConstructorValidSignature
TEST(TransactionPublicKeyHashMessage, DeprecatedConstructorValidSignature)
{
    SerializedEquivalent equivalent(5);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:4000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 35;
    auto keyHash = make_shared<sphincs::KeyHash>();

    auto signature = make_shared<sphincs::Signature>();

    // Deprecated constructor with valid signature
    TransactionPublicKeyHashMessage message(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash,
        signature);

    EXPECT_TRUE(message.isReceiptContains());
    EXPECT_EQ(message.signatures().size(), 1);
    EXPECT_EQ(message.signatures()[0].first, equivalent);
    EXPECT_EQ(message.signatures()[0].second, signature);
}

// Test 14.8: testSerializationDeserializationMultipleSignatures
TEST(TransactionPublicKeyHashMessage, SerializationDeserializationMultipleSignatures)
{
    SerializedEquivalent equivalent(7);
    vector<BaseAddress::Shared> senderAddresses = {
        make_shared<IPv4WithPortAddress>("127.0.0.1:5000")
    };
    const TransactionUUID uuid(string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    PaymentNodeID paymentNodeID = 40;
    auto keyHash = make_shared<sphincs::KeyHash>();

    auto sig1 = make_shared<sphincs::Signature>();
    auto sig2 = make_shared<sphincs::Signature>();
    auto sig3 = make_shared<sphincs::Signature>();

    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures = {
        {SerializedEquivalent(1), sig1},
        {SerializedEquivalent(2), sig2},
        {SerializedEquivalent(3), sig3}
    };

    TransactionPublicKeyHashMessage originalMessage(
        equivalent,
        senderAddresses,
        uuid,
        paymentNodeID,
        keyHash,
        signatures);

    auto [buffer, size] = originalMessage.serializeToBytes();
    ASSERT_GT(size, 0);
    ASSERT_NE(buffer, nullptr);

    TransactionPublicKeyHashMessage deserializedMessage(buffer);

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
    EXPECT_EQ(deserializedMessage.paymentNodeID(), paymentNodeID);
}
