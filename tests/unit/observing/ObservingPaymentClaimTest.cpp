#include <gtest/gtest.h>
#include <sodium.h>
#include <boost/uuid/uuid_generators.hpp>

#include "../../../src/core/observing/ObservingPaymentClaim.h"
#include "../../../src/core/crypto/sphincskeys.h"
#include "../../../src/core/crypto/sphincsscheme.h"

using namespace crypto::sphincs;

/**
 * Unit tests for ObservingPaymentClaim class
 * 
 * This test suite verifies the core functionality of the ObservingPaymentClaim class,
 * including constructor initialization, status management, and getter methods.
 */
class ObservingPaymentClaimTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize sodium library
        if (sodium_init() < 0) {
            FAIL() << "Failed to initialize sodium library";
        }
        
        // Generate test UUID
        boost::uuids::random_generator uuidGen;
        testUUID = uuidGen();
        
        // Set test block number
        testBlockNumber = 12345;
        
        // Create test public keys and signatures
        auto keyPair1 = util::generateKeyPair();
        auto keyPair2 = util::generateKeyPair();
        auto keyPair3 = util::generateKeyPair();
        
        // Create participants map
        testParticipants[0] = keyPair1.second;
        testParticipants[1] = keyPair2.second;
        testParticipants[2] = keyPair3.second;
        
        // Create node's own keys
        auto ownKeyPair = util::generateKeyPair();
        testPublicKey = ownKeyPair.second;
        
        // Create signature (using any key for test purposes)
        const string testData = "test_data_for_signature";
        testSignature = util::signData(*ownKeyPair.first, testData);
    }

    TransactionUUID testUUID;
    BlockNumber testBlockNumber;
    map<PaymentNodeID, sphincs::PublicKey::Shared> testParticipants;
    sphincs::PublicKey::Shared testPublicKey;
    sphincs::Signature::Shared testSignature;
};

// Test 1: Constructor initializes all fields correctly
TEST_F(ObservingPaymentClaimTest, ConstructorInitializesAllFieldsCorrectly) {
    ObservingPaymentClaim claim(
        testUUID,
        testBlockNumber,
        testParticipants,
        testPublicKey,
        testSignature);
    
    // Verify transactionUUID matches
    EXPECT_EQ(claim.transactionUUID(), testUUID);
    
    // Verify maxBlockNumberForClaiming matches
    EXPECT_EQ(claim.maxBlockNumberForClaiming(), testBlockNumber);
    
    // Verify participantsPublicKeys map size and contents
    const auto& participants = claim.participantsPublicKeys();
    EXPECT_EQ(participants.size(), testParticipants.size());
    EXPECT_EQ(participants.size(), 3u);
    
    // Verify each participant is present
    for (const auto& [nodeId, publicKey] : testParticipants) {
        ASSERT_TRUE(participants.count(nodeId) > 0);
        EXPECT_EQ(*participants.at(nodeId), *publicKey);
    }
    
    // Verify publicKey pointer matches
    ASSERT_NE(claim.publicKey(), nullptr);
    EXPECT_EQ(*claim.publicKey(), *testPublicKey);
    
    // Verify signature pointer matches
    ASSERT_NE(claim.signature(), nullptr);
    EXPECT_EQ(*claim.signature(), *testSignature);
}

// Test 2: Constructor sets status to NoInfo
TEST_F(ObservingPaymentClaimTest, ConstructorSetsStatusToNoInfo) {
    ObservingPaymentClaim claim(
        testUUID,
        testBlockNumber,
        testParticipants,
        testPublicKey,
        testSignature);
    
    // Verify status() returns ClaimStatus::NoInfo
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::NoInfo);
}

// Test 3: Status enum values correct
TEST_F(ObservingPaymentClaimTest, StatusEnumValuesCorrect) {
    // Verify enum values match specification
    EXPECT_EQ(static_cast<int>(ObservingPaymentClaim::NoInfo), 0);
    EXPECT_EQ(static_cast<int>(ObservingPaymentClaim::Observing), 1);
    EXPECT_EQ(static_cast<int>(ObservingPaymentClaim::ParticipantsVotesPresent), 2);
    EXPECT_EQ(static_cast<int>(ObservingPaymentClaim::RejectedByObserving), 3);
    EXPECT_EQ(static_cast<int>(ObservingPaymentClaim::Done), 4);
}

// Test 4: Getters return correct values
TEST_F(ObservingPaymentClaimTest, GettersReturnCorrectValues) {
    ObservingPaymentClaim claim(
        testUUID,
        testBlockNumber,
        testParticipants,
        testPublicKey,
        testSignature);
    
    // Call each getter and verify returned values match constructor inputs
    
    // Test transactionUUID getter
    const TransactionUUID& returnedUUID = claim.transactionUUID();
    EXPECT_EQ(returnedUUID, testUUID);
    
    // Test maxBlockNumberForClaiming getter
    BlockNumber returnedBlockNumber = claim.maxBlockNumberForClaiming();
    EXPECT_EQ(returnedBlockNumber, testBlockNumber);
    
    // Test participantsPublicKeys getter
    const auto& returnedParticipants = claim.participantsPublicKeys();
    EXPECT_EQ(returnedParticipants.size(), testParticipants.size());
    for (const auto& [nodeId, publicKey] : testParticipants) {
        ASSERT_TRUE(returnedParticipants.count(nodeId) > 0);
        EXPECT_EQ(*returnedParticipants.at(nodeId), *publicKey);
    }
    
    // Test publicKey getter
    sphincs::PublicKey::Shared returnedPublicKey = claim.publicKey();
    ASSERT_NE(returnedPublicKey, nullptr);
    EXPECT_EQ(*returnedPublicKey, *testPublicKey);
    
    // Test signature getter
    sphincs::Signature::Shared returnedSignature = claim.signature();
    ASSERT_NE(returnedSignature, nullptr);
    EXPECT_EQ(*returnedSignature, *testSignature);
    
    // Test status getter
    ObservingPaymentClaim::ClaimStatus returnedStatus = claim.status();
    EXPECT_EQ(returnedStatus, ObservingPaymentClaim::NoInfo);
}

// Test 5: setStatus updates status correctly
TEST_F(ObservingPaymentClaimTest, SetStatusUpdatesStatusCorrectly) {
    ObservingPaymentClaim claim(
        testUUID,
        testBlockNumber,
        testParticipants,
        testPublicKey,
        testSignature);
    
    // Initial status should be NoInfo
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::NoInfo);
    
    // Call setStatus(Observing)
    claim.setStatus(ObservingPaymentClaim::Observing);
    // Verify status() returns Observing
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::Observing);
    
    // Call setStatus(ParticipantsVotesPresent)
    claim.setStatus(ObservingPaymentClaim::ParticipantsVotesPresent);
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::ParticipantsVotesPresent);
    
    // Call setStatus(RejectedByObserving)
    claim.setStatus(ObservingPaymentClaim::RejectedByObserving);
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::RejectedByObserving);
    
    // Call setStatus(Done)
    claim.setStatus(ObservingPaymentClaim::Done);
    // Verify status() returns Done
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::Done);
    
    // Verify we can go back to NoInfo
    claim.setStatus(ObservingPaymentClaim::NoInfo);
    EXPECT_EQ(claim.status(), ObservingPaymentClaim::NoInfo);
}
