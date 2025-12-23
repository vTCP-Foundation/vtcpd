#include <gtest/gtest.h>
#include <cstring>
#include <algorithm>

#include "core/transactions/transactions/regular/payments/CompletedPaymentsObserverMonitoringTransaction.h"
#include "core/crypto/sphincskeys.h"
#include "core/crypto/sphincsscheme.h"
#include "core/logger/Logger.h"

using namespace crypto::sphincs;

// Task 15-06: Unit tests for CompletedPaymentsObserverMonitoringTransaction

/**
 * Test subclass to access protected members and serialization methods.
 */
class TestableMonitoringTransaction : public CompletedPaymentsObserverMonitoringTransaction
{
public:
    using CompletedPaymentsObserverMonitoringTransaction::serializeGetClaimStatusesForSigning;
    using CompletedPaymentsObserverMonitoringTransaction::serializeSubmitClaimVotesForSigning;
    using CompletedPaymentsObserverMonitoringTransaction::ClaimsList;

    TestableMonitoringTransaction(
        StorageHandler *storageHandler,
        crypto::Keystore *keystore,
        Logger &log)
        : CompletedPaymentsObserverMonitoringTransaction(storageHandler, keystore, log)
    {}

    // Expose protected members for testing
    StorageHandler* getStorageHandler() const { return mStorageHandler; }
    crypto::Keystore* getKeystore() const { return mKeystore; }

    // Override run to prevent actual execution
    TransactionResult::SharedConst run() override
    {
        return nullptr;
    }
};

class CompletedPaymentsObserverMonitoringTransactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mLogger = std::make_unique<Logger>();
    }

    void TearDown() override
    {
        mLogger.reset();
    }

    PublicKey::Shared createTestPublicKey(byte_t fillValue = 0xAB)
    {
        std::vector<byte_t> keyData(PublicKey::keySize(), fillValue);
        return std::make_shared<PublicKey>(keyData.data());
    }

    Signature::Shared createTestSignature(byte_t fillValue = 0xCD)
    {
        std::vector<byte_t> sigData(Signature::signatureSize(), fillValue);
        return std::make_shared<Signature>(sigData.data());
    }

    std::unique_ptr<Logger> mLogger;
};

// Test: kMaxTransactionsPerCycle constant equals 100
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, MaxTransactionsConstantEquals100)
{
    EXPECT_EQ(CompletedPaymentsObserverMonitoringTransaction::kMaxTransactionsPerCycle, 100);
}

// Test: Transaction type is Payments_CompletedPaymentsObserverMonitoring (307)
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, TransactionTypeIsCorrect)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    EXPECT_EQ(tx.transactionType(), BaseTransaction::Payments_CompletedPaymentsObserverMonitoring);
}

// Test: Constructor stores dependencies correctly
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, ConstructorStoresDependenciesCorrectly)
{
    // Use non-null pointers to verify they are stored (we don't dereference them)
    auto dummyStorageHandler = reinterpret_cast<StorageHandler*>(0x1234);
    auto dummyKeystore = reinterpret_cast<crypto::Keystore*>(0x5678);

    TestableMonitoringTransaction tx(dummyStorageHandler, dummyKeystore, *mLogger);

    EXPECT_EQ(tx.getStorageHandler(), dummyStorageHandler);
    EXPECT_EQ(tx.getKeystore(), dummyKeystore);
}

// Test: Constructor accepts nullptr dependencies without throwing
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, ConstructorWithNullDependencies)
{
    EXPECT_NO_THROW({
        CompletedPaymentsObserverMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    });
}

// Test: serializeGetClaimStatusesForSigning - correct size calculation
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeGetClaimStatusesCorrectSize)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TestableMonitoringTransaction::ClaimsList claims;
    TransactionUUID uuid1(std::string("11111111-1111-4111-8111-111111111111"));
    TransactionUUID uuid2(std::string("22222222-2222-4222-8222-222222222222"));
    claims.emplace_back(uuid1, 100);
    claims.emplace_back(uuid2, 200);

    auto [data, size] = tx.serializeGetClaimStatusesForSigning(claims, publicKey);

    // Expected size: 4 (count) + 2*(16+8) (claims) + keySize (public key)
    const size_t expectedSize = sizeof(uint32_t) +
                                2 * (TransactionUUID::kBytesSize + sizeof(BlockNumber)) +
                                PublicKey::keySize();
    EXPECT_EQ(size, expectedSize);
    EXPECT_NE(data, nullptr);
}

// Test: serializeGetClaimStatusesForSigning - claims sorted by UUID
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeGetClaimStatusesSortedByUUID)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    // Add in reverse order to ensure sorting happens
    TestableMonitoringTransaction::ClaimsList claims;
    TransactionUUID uuid2(std::string("22222222-2222-4222-8222-222222222222"));
    TransactionUUID uuid1(std::string("11111111-1111-4111-8111-111111111111"));
    claims.emplace_back(uuid2, 200); // Add larger UUID first
    claims.emplace_back(uuid1, 100); // Add smaller UUID second

    auto [data, size] = tx.serializeGetClaimStatusesForSigning(claims, publicKey);

    // Verify count is 2
    uint32_t count;
    std::memcpy(&count, data.get(), sizeof(uint32_t));
    EXPECT_EQ(count, 2);

    // First UUID after count should be uuid1 (smaller, comes first after sorting)
    TransactionUUID firstUUID(data.get() + sizeof(uint32_t));
    EXPECT_EQ(firstUUID.stringUUID(), uuid1.stringUUID());

    // Second UUID should be uuid2
    TransactionUUID secondUUID(data.get() + sizeof(uint32_t) + TransactionUUID::kBytesSize + sizeof(BlockNumber));
    EXPECT_EQ(secondUUID.stringUUID(), uuid2.stringUUID());
}

// Test: serializeGetClaimStatusesForSigning - empty claims
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeGetClaimStatusesEmptyClaims)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TestableMonitoringTransaction::ClaimsList claims; // empty

    auto [data, size] = tx.serializeGetClaimStatusesForSigning(claims, publicKey);

    // Expected size: 4 (count=0) + keySize
    const size_t expectedSize = sizeof(uint32_t) + PublicKey::keySize();
    EXPECT_EQ(size, expectedSize);

    uint32_t count;
    std::memcpy(&count, data.get(), sizeof(uint32_t));
    EXPECT_EQ(count, 0);
}

// Test: serializeSubmitClaimVotesForSigning - correct size calculation
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeSubmitClaimVotesCorrectSize)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    BlockNumber blockNumber = 12345;

    std::map<PaymentNodeID, Signature::Shared> votes;
    votes[1] = createTestSignature(0x11);
    votes[2] = createTestSignature(0x22);
    votes[3] = createTestSignature(0x33);

    auto [data, size] = tx.serializeSubmitClaimVotesForSigning(
        transactionUUID, blockNumber, votes, publicKey);

    // Expected size: 16 (UUID) + 8 (block) + 4 (count) + 3*(2+sigSize) + keySize
    const size_t expectedSize = TransactionUUID::kBytesSize +
                                sizeof(BlockNumber) +
                                sizeof(uint32_t) +
                                3 * (sizeof(PaymentNodeID) + Signature::signatureSize()) +
                                PublicKey::keySize();
    EXPECT_EQ(size, expectedSize);
    EXPECT_NE(data, nullptr);
}

// Test: serializeSubmitClaimVotesForSigning - votes sorted by PaymentNodeID
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeSubmitClaimVotesSortedByPaymentNodeID)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    BlockNumber blockNumber = 12345;

    // std::map is already sorted by key, but let's explicitly add in different order
    std::map<PaymentNodeID, Signature::Shared> votes;
    votes[5] = createTestSignature(0x55);
    votes[2] = createTestSignature(0x22);
    votes[8] = createTestSignature(0x88);

    auto [data, size] = tx.serializeSubmitClaimVotesForSigning(
        transactionUUID, blockNumber, votes, publicKey);

    // Check votes count
    uint32_t votesCount;
    size_t offset = TransactionUUID::kBytesSize + sizeof(BlockNumber);
    std::memcpy(&votesCount, data.get() + offset, sizeof(uint32_t));
    EXPECT_EQ(votesCount, 3);

    // First PaymentNodeID should be 2 (smallest)
    offset += sizeof(uint32_t);
    PaymentNodeID firstNodeId;
    std::memcpy(&firstNodeId, data.get() + offset, sizeof(PaymentNodeID));
    EXPECT_EQ(firstNodeId, 2);

    // Second PaymentNodeID should be 5
    offset += sizeof(PaymentNodeID) + Signature::signatureSize();
    PaymentNodeID secondNodeId;
    std::memcpy(&secondNodeId, data.get() + offset, sizeof(PaymentNodeID));
    EXPECT_EQ(secondNodeId, 5);

    // Third PaymentNodeID should be 8
    offset += sizeof(PaymentNodeID) + Signature::signatureSize();
    PaymentNodeID thirdNodeId;
    std::memcpy(&thirdNodeId, data.get() + offset, sizeof(PaymentNodeID));
    EXPECT_EQ(thirdNodeId, 8);
}

// Test: serializeSubmitClaimVotesForSigning - empty votes
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeSubmitClaimVotesEmptyVotes)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    BlockNumber blockNumber = 12345;

    std::map<PaymentNodeID, Signature::Shared> votes; // empty

    auto [data, size] = tx.serializeSubmitClaimVotesForSigning(
        transactionUUID, blockNumber, votes, publicKey);

    // Expected size: 16 (UUID) + 8 (block) + 4 (count=0) + keySize
    const size_t expectedSize = TransactionUUID::kBytesSize +
                                sizeof(BlockNumber) +
                                sizeof(uint32_t) +
                                PublicKey::keySize();
    EXPECT_EQ(size, expectedSize);

    // Check votes count is 0
    uint32_t votesCount;
    size_t offset = TransactionUUID::kBytesSize + sizeof(BlockNumber);
    std::memcpy(&votesCount, data.get() + offset, sizeof(uint32_t));
    EXPECT_EQ(votesCount, 0);
}

// Test: serializeGetClaimStatusesForSigning - block numbers are serialized correctly
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeGetClaimStatusesBlockNumbers)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TestableMonitoringTransaction::ClaimsList claims;
    TransactionUUID uuid1(std::string("11111111-1111-4111-8111-111111111111"));
    BlockNumber expectedBlockNumber = 0x123456789ABCDEF0ULL;
    claims.emplace_back(uuid1, expectedBlockNumber);

    auto [data, size] = tx.serializeGetClaimStatusesForSigning(claims, publicKey);

    // Read block number from serialized data
    BlockNumber serializedBlockNumber;
    size_t offset = sizeof(uint32_t) + TransactionUUID::kBytesSize;
    std::memcpy(&serializedBlockNumber, data.get() + offset, sizeof(BlockNumber));

    EXPECT_EQ(serializedBlockNumber, expectedBlockNumber);
}

// Test: serializeSubmitClaimVotesForSigning - block number is serialized correctly
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeSubmitClaimVotesBlockNumber)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    auto publicKey = createTestPublicKey();

    TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    BlockNumber expectedBlockNumber = 0xFEDCBA9876543210ULL;

    std::map<PaymentNodeID, Signature::Shared> votes;
    votes[1] = createTestSignature();

    auto [data, size] = tx.serializeSubmitClaimVotesForSigning(
        transactionUUID, expectedBlockNumber, votes, publicKey);

    // Read block number from serialized data
    BlockNumber serializedBlockNumber;
    std::memcpy(&serializedBlockNumber, data.get() + TransactionUUID::kBytesSize, sizeof(BlockNumber));

    EXPECT_EQ(serializedBlockNumber, expectedBlockNumber);
}

// Test: Public key is appended at the end of serialized data
TEST_F(CompletedPaymentsObserverMonitoringTransactionTest, SerializeGetClaimStatusesPublicKeyAtEnd)
{
    TestableMonitoringTransaction tx(nullptr, nullptr, *mLogger);
    
    // Create public key with known pattern
    std::vector<byte_t> keyPattern(PublicKey::keySize());
    for (size_t i = 0; i < keyPattern.size(); ++i) {
        keyPattern[i] = static_cast<byte_t>(i % 256);
    }
    auto publicKey = std::make_shared<PublicKey>(keyPattern.data());

    TestableMonitoringTransaction::ClaimsList claims;
    claims.emplace_back(TransactionUUID(), 100);

    auto [data, size] = tx.serializeGetClaimStatusesForSigning(claims, publicKey);

    // Public key should be at the end
    const byte_t* publicKeyStart = data.get() + size - PublicKey::keySize();
    EXPECT_EQ(std::memcmp(publicKeyStart, keyPattern.data(), PublicKey::keySize()), 0);
}

