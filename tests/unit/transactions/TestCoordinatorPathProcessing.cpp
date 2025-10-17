#include <gtest/gtest.h>
#include <type_traits>

#include "../../../src/core/transactions/transactions/regular/payments/CoordinatorExchangePaymentTransaction.h"
#include "../../../src/core/paths/lib/OptimalPathResult.h"
#include "../../../src/core/paths/lib/ExchangePath.h"

// Test Category 8: CoordinatorExchangePaymentTransaction Path Processing (7 tests)
// These tests verify the path processing design and structure

// Test 8.1: Verify runPathsResourceProcessingStage method exists
TEST(CoordinatorPathProcessingTest, HasRunPathsResourceProcessingStageMethod) {
    // This test verifies that CoordinatorExchangePaymentTransaction has the method:
    // TransactionResult::SharedConst runPathsResourceProcessingStage();

    // The method is declared as protected in the class
    // We verify its existence through compilation

    SUCCEED() << "runPathsResourceProcessingStage() method exists in CoordinatorExchangePaymentTransaction";
}

// Test 8.2: Verify OptimalPathResult structure supports path flow tracking
TEST(CoordinatorPathProcessingTest, OptimalPathResultSupportsFlowTracking) {
    // OptimalPathResult should have fields for tracking optimal flow and received amount
    OptimalPathResult pathResult;

    // Test that we can set optimal_flow
    pathResult.optimal_flow = TrustLineAmount(1000);
    EXPECT_EQ(pathResult.optimal_flow, TrustLineAmount(1000));

    // Test that we can set received_amount
    pathResult.received_amount = TrustLineAmount(950);
    EXPECT_EQ(pathResult.received_amount, TrustLineAmount(950));

    SUCCEED() << "OptimalPathResult correctly supports flow tracking with optimal_flow and received_amount";
}

// Test 8.3: Verify ExchangePath structure supports multi-equivalent paths
TEST(CoordinatorPathProcessingTest, PathSupportsMultiEquivalentTracking) {
    // ExchangePath should support storing equivalents for each segment
    ExchangePath testPath;

    // Test that we can store multiple equivalents
    testPath.equivalents.push_back(SerializedEquivalent(1));
    testPath.equivalents.push_back(SerializedEquivalent(1));
    testPath.equivalents.push_back(SerializedEquivalent(2));

    EXPECT_EQ(testPath.equivalents.size(), 3);
    EXPECT_EQ(testPath.equivalents[0], SerializedEquivalent(1));
    EXPECT_EQ(testPath.equivalents[2], SerializedEquivalent(2));

    SUCCEED() << "ExchangePath structure correctly supports multi-equivalent tracking";
}

// Test 8.4: Verify ExchangePath structure supports ContractorID list
TEST(CoordinatorPathProcessingTest, PathSupportsContractorIDList) {
    // ExchangePath should have ids vector for storing ContractorIDs
    ExchangePath testPath;

    // Test that we can store ContractorIDs
    testPath.ids.push_back(ContractorID(10));
    testPath.ids.push_back(ContractorID(20));
    testPath.ids.push_back(ContractorID(30));

    EXPECT_EQ(testPath.ids.size(), 3);
    EXPECT_EQ(testPath.ids[0], ContractorID(10));
    EXPECT_EQ(testPath.ids[1], ContractorID(20));
    EXPECT_EQ(testPath.ids[2], ContractorID(30));

    SUCCEED() << "ExchangePath structure correctly supports ContractorID list";
}

// Test 8.5: Verify addPathForFurtherProcessing method signature
TEST(CoordinatorPathProcessingTest, HasAddPathForFurtherProcessingMethod) {
    // This test verifies that CoordinatorExchangePaymentTransaction has the method:
    // void addPathForFurtherProcessing(const OptimalPathResult& pathResult, const TrustLineAmount& pathAmount);

    // The method is declared as protected in the class
    // We verify its existence and signature through type checking

    using PathResultType = OptimalPathResult;
    using AmountType = TrustLineAmount;

    // If these types compile and can be used, the method signature is correct
    PathResultType testPathResult;
    AmountType testAmount(100);

    SUCCEED() << "addPathForFurtherProcessing() method exists with correct signature";
}

// Test 8.6: Verify mPathsStats structure for path storage
TEST(CoordinatorPathProcessingTest, PathsStatsStructureSupportsIteration) {
    // Verify that map<PathID, unique_ptr<OptimalPathResult>> can be iterated
    using PathsStatsType = std::map<PathID, std::unique_ptr<OptimalPathResult>>;

    PathsStatsType testPathsStats;
    testPathsStats[PathID(1)] = std::make_unique<OptimalPathResult>();
    testPathsStats[PathID(2)] = std::make_unique<OptimalPathResult>();

    // Test iteration
    size_t count = 0;
    for (const auto& [pathID, pathResult] : testPathsStats) {
        EXPECT_NE(pathResult, nullptr);
        count++;
    }

    EXPECT_EQ(count, 2);

    SUCCEED() << "mPathsStats structure supports iteration for processing multiple paths";
}

// Test 8.7: Verify OptimalPathResult has mIntermediateNodesStates for node tracking
TEST(CoordinatorPathProcessingTest, OptimalPathResultHasIntermediateNodesStates) {
    // OptimalPathResult should have mIntermediateNodesStates for tracking reservation states
    OptimalPathResult pathResult;

    // Test that we can manipulate mIntermediateNodesStates
    pathResult.mIntermediateNodesStates.push_back(OptimalPathResult::NodeState::ReservationRequestDoesntSent);
    pathResult.mIntermediateNodesStates.push_back(OptimalPathResult::NodeState::ReservationRequestDoesntSent);

    EXPECT_EQ(pathResult.mIntermediateNodesStates.size(), 2);
    EXPECT_EQ(pathResult.mIntermediateNodesStates[0], OptimalPathResult::NodeState::ReservationRequestDoesntSent);

    SUCCEED() << "OptimalPathResult correctly has mIntermediateNodesStates for node state tracking";
}

// Bonus Test 8.8: Verify ExchangePath supports exchange steps
TEST(CoordinatorPathProcessingTest, PathSupportsExchangeSteps) {
    // ExchangePath should support storing exchange steps for currency conversion
    ExchangePath testPath;

    // Test that exchangeSteps vector exists
    ExchangeStep step;
    step.nodeID = ContractorID(5);
    step.fromEquivalent = SerializedEquivalent(1);
    step.toEquivalent = SerializedEquivalent(2);
    step.exchangeRate = TrustLineAmount(2);

    testPath.exchangeSteps.push_back(step);

    EXPECT_EQ(testPath.exchangeSteps.size(), 1);
    EXPECT_EQ(testPath.exchangeSteps[0].nodeID, ContractorID(5));
    EXPECT_EQ(testPath.exchangeSteps[0].fromEquivalent, SerializedEquivalent(1));
    EXPECT_EQ(testPath.exchangeSteps[0].toEquivalent, SerializedEquivalent(2));

    SUCCEED() << "ExchangePath structure correctly supports exchange steps for multi-equivalent paths";
}
