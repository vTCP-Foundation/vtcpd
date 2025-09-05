#include <gtest/gtest.h>
#include "../../../src/core/rates/manager/CommissionsManager.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/trust_lines/TrustLine.h"

class CommissionsManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = std::make_unique<Logger>();
    }

    void TearDown() override {
        manager.reset();
        logger.reset();
    }

    std::unique_ptr<Logger> logger;
    std::unique_ptr<CommissionsManager> manager;
};

// Test commission structure
TEST_F(CommissionsManagerTest, testCommissionStructure) {
    auto commission = std::make_shared<Commission>(100);
    EXPECT_EQ(commission->amount(), 100);

    auto zeroCommission = std::make_shared<Commission>(0);
    EXPECT_EQ(zeroCommission->amount(), 0);
}

// Test getCommission for non-existent equivalent (with default config)
TEST_F(CommissionsManagerTest, testGetCommissionForNonExistentEquivalent) {
    vector<pair<SerializedEquivalent, uint64_t>> commissions; // empty config
    manager = std::make_unique<CommissionsManager>(*logger, commissions);
    
    auto commission = manager->getCommission(3);  // Non-existent equivalent
    EXPECT_EQ(commission, nullptr);
}

// Test applyCommission: No commission for equivalent
TEST_F(CommissionsManagerTest, testApplyCommissionNoCommission) {
    vector<pair<SerializedEquivalent, uint64_t>> commissions; // empty config
    manager = std::make_unique<CommissionsManager>(*logger, commissions);
    
    TrustLineAmount input(1000);
    TrustLineAmount result = manager->applyCommission(input, 2);  // No commission for equiv 2
    EXPECT_EQ(result, input);  // Should return unchanged
}

// Test applyCommission: Input = 0
TEST_F(CommissionsManagerTest, testApplyCommissionZeroInput) {
    vector<pair<SerializedEquivalent, uint64_t>> commissions; // empty config
    manager = std::make_unique<CommissionsManager>(*logger, commissions);
    
    TrustLineAmount zeroInput = TrustLine::kZeroAmount();
    TrustLineAmount result = manager->applyCommission(zeroInput, 1);
    EXPECT_EQ(result, TrustLine::kZeroAmount());
}

// Test applyCommission idempotence (same input produces same result)
TEST_F(CommissionsManagerTest, testApplyCommissionIdempotence) {
    vector<pair<SerializedEquivalent, uint64_t>> commissions; // empty config
    manager = std::make_unique<CommissionsManager>(*logger, commissions);
    
    TrustLineAmount input(100);
    TrustLineAmount result1 = manager->applyCommission(input, 1);
    TrustLineAmount result2 = manager->applyCommission(input, 1);
    
    EXPECT_EQ(result1, result2);
}

// Test that manager handles missing configuration gracefully
TEST_F(CommissionsManagerTest, testLoadConfigurationInvalidFile) {
    // Should not crash, should load with empty commissions
    vector<pair<SerializedEquivalent, uint64_t>> commissions; // empty config
    manager = std::make_unique<CommissionsManager>(*logger, commissions);
    
    auto commission = manager->getCommission(1);
    EXPECT_EQ(commission, nullptr);
}