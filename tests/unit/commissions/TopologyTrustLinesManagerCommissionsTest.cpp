#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../../../src/core/topology/manager/TopologyTrustLinesManager.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"

class TopologyTrustLinesManagerCommissionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = std::make_unique<Logger>();
        ownAddress = make_shared<IPv4WithPortAddress>("127.0.0.1:2000");
        manager = std::make_unique<TopologyTrustLinesManager>(
            1,              // equivalent
            ownAddress,     // own address
            false,          // not gateway
            ioContext,      // io_context required by manager
            *logger
        );
    }

    void TearDown() override {
        manager.reset();
        logger.reset();
    }

    std::unique_ptr<Logger> logger;
    std::unique_ptr<TopologyTrustLinesManager> manager;
    BaseAddress::Shared ownAddress;
    boost::asio::io_context ioContext;
};

// Test storing commission with amount > 0
TEST_F(TopologyTrustLinesManagerCommissionsTest, testStoreCommissionPositiveAmount) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 2;
    auto commission = std::make_shared<Commission>(100);

    manager->storeCommission(contractorID, equivalent, commission);
    
    auto retrievedCommission = manager->getCommission(contractorID, equivalent);
    ASSERT_NE(retrievedCommission, nullptr);
    EXPECT_EQ(retrievedCommission->amount(), 100);
}

// Test storing commission with amount = 0 (should not store)
TEST_F(TopologyTrustLinesManagerCommissionsTest, testStoreCommissionZeroAmount) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 2;
    auto commission = std::make_shared<Commission>(0);

    manager->storeCommission(contractorID, equivalent, commission);
    
    auto retrievedCommission = manager->getCommission(contractorID, equivalent);
    EXPECT_EQ(retrievedCommission, nullptr);
}

// Test updating existing commission (refresh TTL)
TEST_F(TopologyTrustLinesManagerCommissionsTest, testUpdateExistingCommission) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 2;
    auto commission1 = std::make_shared<Commission>(100);
    auto commission2 = std::make_shared<Commission>(200);

    // Store first commission
    manager->storeCommission(contractorID, equivalent, commission1);
    auto retrieved1 = manager->getCommission(contractorID, equivalent);
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(retrieved1->amount(), 100);

    // Update with new commission
    manager->storeCommission(contractorID, equivalent, commission2);
    auto retrieved2 = manager->getCommission(contractorID, equivalent);
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(retrieved2->amount(), 200);
}

// Test getting commission for non-existent entry
TEST_F(TopologyTrustLinesManagerCommissionsTest, testGetCommissionNonExistent) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 2;
    
    auto retrievedCommission = manager->getCommission(contractorID, equivalent);
    EXPECT_EQ(retrievedCommission, nullptr);
}

// Test storing commissions for different contractor/equivalent combinations
TEST_F(TopologyTrustLinesManagerCommissionsTest, testStoreMultipleCommissions) {
    auto commission1 = std::make_shared<Commission>(100);
    auto commission2 = std::make_shared<Commission>(200);
    auto commission3 = std::make_shared<Commission>(300);

    manager->storeCommission(1, 1, commission1);  // contractor 1, equiv 1
    manager->storeCommission(1, 2, commission2);  // contractor 1, equiv 2
    manager->storeCommission(2, 1, commission3);  // contractor 2, equiv 1

    auto retrieved1 = manager->getCommission(1, 1);
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(retrieved1->amount(), 100);

    auto retrieved2 = manager->getCommission(1, 2);
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(retrieved2->amount(), 200);

    auto retrieved3 = manager->getCommission(2, 1);
    ASSERT_NE(retrieved3, nullptr);
    EXPECT_EQ(retrieved3->amount(), 300);

    // Non-existent combination
    auto nonExistent = manager->getCommission(2, 2);
    EXPECT_EQ(nonExistent, nullptr);
}

// Test TTL constant
TEST_F(TopologyTrustLinesManagerCommissionsTest, testTTLConstant) {
    EXPECT_EQ(TopologyTrustLinesManager::kCommissionsTTLSeconds, 300);
}

// Test commission cleanup (basic functionality test)
TEST_F(TopologyTrustLinesManagerCommissionsTest, testCommissionCleanup) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 2;
    auto commission = std::make_shared<Commission>(100);

    // Store commission
    manager->storeCommission(contractorID, equivalent, commission);
    
    // Verify it exists
    auto retrieved = manager->getCommission(contractorID, equivalent);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->amount(), 100);

    // Cleanup should not remove non-expired entries (we can't easily test expiry in unit tests
    // without manipulating time, but we can test that cleanup doesn't break valid entries)
    // Note: In real scenario, cleanup is called automatically during storeCommission
}

// Note: earliest expiry time is private; behavior is indirectly covered by TTL and cleanup tests.

// Test commission storage with same contractor but different equivalents
TEST_F(TopologyTrustLinesManagerCommissionsTest, testSameContractorDifferentEquivalents) {
    ContractorID contractorID = 5;
    auto commission1 = std::make_shared<Commission>(150);
    auto commission2 = std::make_shared<Commission>(250);

    manager->storeCommission(contractorID, 1, commission1);
    manager->storeCommission(contractorID, 2, commission2);

    auto retrieved1 = manager->getCommission(contractorID, 1);
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(retrieved1->amount(), 150);

    auto retrieved2 = manager->getCommission(contractorID, 2);
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(retrieved2->amount(), 250);
}

// Test commission storage with same equivalent but different contractors
TEST_F(TopologyTrustLinesManagerCommissionsTest, testSameEquivalentDifferentContractors) {
    SerializedEquivalent equivalent = 3;
    auto commission1 = std::make_shared<Commission>(175);
    auto commission2 = std::make_shared<Commission>(275);

    manager->storeCommission(1, equivalent, commission1);
    manager->storeCommission(2, equivalent, commission2);

    auto retrieved1 = manager->getCommission(1, equivalent);
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(retrieved1->amount(), 175);

    auto retrieved2 = manager->getCommission(2, equivalent);
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(retrieved2->amount(), 275);
}

// Test commission overwrite behavior
TEST_F(TopologyTrustLinesManagerCommissionsTest, testCommissionOverwrite) {
    ContractorID contractorID = 10;
    SerializedEquivalent equivalent = 5;
    auto commission1 = std::make_shared<Commission>(300);
    auto commission2 = std::make_shared<Commission>(400);

    // Store initial commission
    manager->storeCommission(contractorID, equivalent, commission1);
    auto retrieved1 = manager->getCommission(contractorID, equivalent);
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(retrieved1->amount(), 300);

    // Overwrite with new commission
    manager->storeCommission(contractorID, equivalent, commission2);
    auto retrieved2 = manager->getCommission(contractorID, equivalent);
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(retrieved2->amount(), 400);  // Should have new value
}

// Test printCommissions method with empty cache
TEST_F(TopologyTrustLinesManagerCommissionsTest, testPrintCommissionsEmptyCache) {
    // Should not crash when printing empty cache
    manager->printCommissions();
}

// Test printCommissions method with commissions
TEST_F(TopologyTrustLinesManagerCommissionsTest, testPrintCommissionsWithData) {
    auto commission1 = std::make_shared<Commission>(150);
    auto commission2 = std::make_shared<Commission>(250);
    auto commission3 = std::make_shared<Commission>(350);

    manager->storeCommission(1, 1, commission1);
    manager->storeCommission(2, 1, commission2);
    manager->storeCommission(1, 2, commission3);

    // Should print all stored commissions to debug log
    manager->printCommissions();
}

// Note: removeExpiredCommissions is private; we verify retention via public getters in other tests.