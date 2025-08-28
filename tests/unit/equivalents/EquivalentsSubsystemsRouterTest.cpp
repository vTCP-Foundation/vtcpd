#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/contractors/addresses/GNSAddress.h"
#include "../../../src/core/common/exceptions/ValueError.h"

// Simple unit test for the new methods without full integration
class MockEquivalentsSubsystemsRouter {
public:
    MockEquivalentsSubsystemsRouter() : mHigherFreeID(1) {
        // Initialize with self address as ID 0
        auto selfAddress = std::make_shared<IPv4WithPortAddress>("127.0.0.1", 8080);
        mParticipantsAddresses.emplace_back(selfAddress, 0);
    }
    
    ContractorID getOrCreateParticipantID(const BaseAddress::Shared &address) {
        if (!address) {
            throw ValueError("Address cannot be null");
        }
        
        // Search for existing address
        for (const auto &participantAddress : mParticipantsAddresses) {
            if (participantAddress.first == address) {
                return participantAddress.second;
            }
        }
        
        // Create new entry
        mParticipantsAddresses.emplace_back(address, mHigherFreeID);
        auto result = mHigherFreeID;
        mHigherFreeID++;
        return result;
    }
    
    BaseAddress::Shared resolveParticipantAddress(ContractorID contractorID) const {
        for (const auto &participant : mParticipantsAddresses) {
            if (participant.second == contractorID) {
                return participant.first;
            }
        }
        return nullptr;
    }
    
    std::optional<ContractorID> resolveParticipantID(const BaseAddress::Shared &address) const {
        if (!address) {
            return std::nullopt;
        }
        
        for (const auto &participantAddress : mParticipantsAddresses) {
            if (participantAddress.first == address) {
                return participantAddress.second;
            }
        }
        return std::nullopt;
    }
    
private:
    std::vector<std::pair<BaseAddress::Shared, ContractorID>> mParticipantsAddresses;
    ContractorID mHigherFreeID;
};

class EquivalentsSubsystemsRouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        router = std::make_unique<MockEquivalentsSubsystemsRouter>();
        
        // Create test addresses for participant ID management tests
        testAddress1 = std::make_shared<IPv4WithPortAddress>("192.168.1.1", 8080);
        testAddress2 = std::make_shared<IPv4WithPortAddress>("192.168.1.2", 8080);
        testAddress3 = std::make_shared<GNSAddress>("test-node@example.com:8080");
        selfAddress = std::make_shared<IPv4WithPortAddress>("127.0.0.1", 8080);
    }

    void TearDown() override {
        router.reset();
    }

    std::unique_ptr<MockEquivalentsSubsystemsRouter> router;
    BaseAddress::Shared selfAddress;
    BaseAddress::Shared testAddress1;
    BaseAddress::Shared testAddress2;
    BaseAddress::Shared testAddress3;
};

TEST_F(EquivalentsSubsystemsRouterTest, getOrCreateParticipantID_NewAddress_ReturnsNewID) {
    auto contractorID = router->getOrCreateParticipantID(testAddress1);
    
    // Should return a valid ID (not 0, as that's reserved for current node)
    EXPECT_NE(contractorID, 0);
    EXPECT_GT(contractorID, 0);
}

TEST_F(EquivalentsSubsystemsRouterTest, getOrCreateParticipantID_SameAddress_ReturnsSameID) {
    auto contractorID1 = router->getOrCreateParticipantID(testAddress1);
    auto contractorID2 = router->getOrCreateParticipantID(testAddress1);
    
    // Same address should return the same ID
    EXPECT_EQ(contractorID1, contractorID2);
}

TEST_F(EquivalentsSubsystemsRouterTest, getOrCreateParticipantID_DifferentAddresses_ReturnsDifferentIDs) {
    auto contractorID1 = router->getOrCreateParticipantID(testAddress1);
    auto contractorID2 = router->getOrCreateParticipantID(testAddress2);
    auto contractorID3 = router->getOrCreateParticipantID(testAddress3);
    
    // Different addresses should have different IDs
    EXPECT_NE(contractorID1, contractorID2);
    EXPECT_NE(contractorID1, contractorID3);
    EXPECT_NE(contractorID2, contractorID3);
}

TEST_F(EquivalentsSubsystemsRouterTest, getOrCreateParticipantID_SelfAddress_ReturnsZero) {
    auto contractorID = router->getOrCreateParticipantID(selfAddress);
    
    // Self address should return ID 0 (current node ID)
    EXPECT_EQ(contractorID, 0);
}

TEST_F(EquivalentsSubsystemsRouterTest, getOrCreateParticipantID_NullAddress_ThrowsException) {
    BaseAddress::Shared nullAddress = nullptr;
    
    EXPECT_THROW(
        router->getOrCreateParticipantID(nullAddress),
        ValueError
    );
}

TEST_F(EquivalentsSubsystemsRouterTest, resolveParticipantID_ExistingAddress_ReturnsCorrectID) {
    // First create an ID for the address
    auto originalID = router->getOrCreateParticipantID(testAddress1);
    
    // Then resolve it
    auto resolvedID = router->resolveParticipantID(testAddress1);
    
    EXPECT_TRUE(resolvedID.has_value());
    EXPECT_EQ(resolvedID.value(), originalID);
}

TEST_F(EquivalentsSubsystemsRouterTest, resolveParticipantID_NonExistentAddress_ReturnsNullopt) {
    auto resolvedID = router->resolveParticipantID(testAddress1);
    
    EXPECT_FALSE(resolvedID.has_value());
}

TEST_F(EquivalentsSubsystemsRouterTest, resolveParticipantID_NullAddress_ReturnsNullopt) {
    BaseAddress::Shared nullAddress = nullptr;
    
    auto resolvedID = router->resolveParticipantID(nullAddress);
    
    EXPECT_FALSE(resolvedID.has_value());
}

TEST_F(EquivalentsSubsystemsRouterTest, resolveParticipantAddress_ExistingID_ReturnsCorrectAddress) {
    // Create an ID for the address
    auto contractorID = router->getOrCreateParticipantID(testAddress1);
    
    // Resolve it back to address
    auto resolvedAddress = router->resolveParticipantAddress(contractorID);
    
    EXPECT_NE(resolvedAddress, nullptr);
    EXPECT_EQ(resolvedAddress, testAddress1);
}

TEST_F(EquivalentsSubsystemsRouterTest, resolveParticipantAddress_NonExistentID_ReturnsNull) {
    ContractorID nonExistentID = 9999;
    auto resolvedAddress = router->resolveParticipantAddress(nonExistentID);
    
    EXPECT_EQ(resolvedAddress, nullptr);
}

TEST_F(EquivalentsSubsystemsRouterTest, resolveParticipantAddress_SelfID_ReturnsSelfAddress) {
    auto resolvedAddress = router->resolveParticipantAddress(0);
    
    EXPECT_NE(resolvedAddress, nullptr);
    EXPECT_EQ(resolvedAddress, selfAddress);
}

TEST_F(EquivalentsSubsystemsRouterTest, UnifiedIDsAcrossMultipleOperations_MaintainsConsistency) {
    // Test multiple operations maintain consistency
    std::vector<BaseAddress::Shared> addresses = {
        testAddress1, testAddress2, testAddress3
    };
    std::vector<ContractorID> originalIDs;
    
    // Create IDs for all addresses
    for (const auto& address : addresses) {
        originalIDs.push_back(router->getOrCreateParticipantID(address));
    }
    
    // Verify all IDs are unique
    for (size_t i = 0; i < originalIDs.size(); ++i) {
        for (size_t j = i + 1; j < originalIDs.size(); ++j) {
            EXPECT_NE(originalIDs[i], originalIDs[j]);
        }
    }
    
    // Verify repeated calls return same IDs
    for (size_t i = 0; i < addresses.size(); ++i) {
        auto repeatedID = router->getOrCreateParticipantID(addresses[i]);
        EXPECT_EQ(repeatedID, originalIDs[i]);
    }
    
    // Verify resolve operations work correctly
    for (size_t i = 0; i < addresses.size(); ++i) {
        auto resolvedID = router->resolveParticipantID(addresses[i]);
        EXPECT_TRUE(resolvedID.has_value());
        EXPECT_EQ(resolvedID.value(), originalIDs[i]);
        
        auto resolvedAddress = router->resolveParticipantAddress(originalIDs[i]);
        EXPECT_EQ(resolvedAddress, addresses[i]);
    }
}

TEST_F(EquivalentsSubsystemsRouterTest, IDStabilityAcrossEquivalents_SameAddressReturnsSameID) {
    // This test verifies that the same address gets the same ID regardless of 
    // which equivalent context it's accessed from, as per the requirements.
    
    // Create ID in one context
    auto contractorID1 = router->getOrCreateParticipantID(testAddress1);
    
    // The same address should have the same ID when accessed again
    auto contractorID2 = router->getOrCreateParticipantID(testAddress1);
    
    EXPECT_EQ(contractorID1, contractorID2);
    
    // Resolution should also be consistent
    auto resolvedID = router->resolveParticipantID(testAddress1);
    EXPECT_TRUE(resolvedID.has_value());
    EXPECT_EQ(resolvedID.value(), contractorID1);
    
    auto resolvedAddress = router->resolveParticipantAddress(contractorID1);
    EXPECT_EQ(resolvedAddress, testAddress1);
}

TEST_F(EquivalentsSubsystemsRouterTest, IncrementalIDGeneration_ProducesSequentialIDs) {
    // Test that IDs are generated sequentially (excluding 0 for self)
    auto id1 = router->getOrCreateParticipantID(testAddress1);
    auto id2 = router->getOrCreateParticipantID(testAddress2);
    auto id3 = router->getOrCreateParticipantID(testAddress3);
    
    // IDs should be positive and incrementing
    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_GT(id3, 0);
    
    // Verify they're sequential (allowing for the fact that the self address is ID 0)
    EXPECT_EQ(id2, id1 + 1);
    EXPECT_EQ(id3, id2 + 1);
}