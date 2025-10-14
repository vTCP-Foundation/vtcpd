#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/paths/lib/OptimalPathResult.h"
#include "core/paths/lib/ExchangePath.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/common/exceptions/ValueError.h"
#include "core/common/exceptions/NotFoundError.h"

using namespace testing;

namespace {

// Helper function to create TrustLineAmount
inline TrustLineAmount A(uint64_t v) {
    return TrustLineAmount(v);
}

// Helper to create IPv4WithPortAddress
inline BaseAddress::Shared createAddress(const string& addr) {
    return make_shared<IPv4WithPortAddress>(addr);
}

} // namespace

// Test Category 2: OptimalPathResult Enhancement (20+ tests)

TEST(OptimalPathResult, FieldsAdded) {
    OptimalPathResult result;

    // Verify new fields exist and are accessible
    result.mMaxPathFlow = A(1000);
    result.mIsValid = true;
    result.mIntermediateNodesStates.push_back(OptimalPathResult::NodeState::ReservationRequestDoesntSent);

    EXPECT_EQ(result.mMaxPathFlow, A(1000));
    EXPECT_TRUE(result.mIsValid);
    EXPECT_EQ(result.mIntermediateNodesStates.size(), 1);
}

TEST(OptimalPathResult, SetNodeState) {
    OptimalPathResult result;
    result.mIntermediateNodesStates.resize(5, OptimalPathResult::NodeState::ReservationRequestDoesntSent);

    result.setNodeState(2, OptimalPathResult::NodeState::ReservationApproved);

    EXPECT_EQ(result.mIntermediateNodesStates[2], OptimalPathResult::NodeState::ReservationApproved);
    EXPECT_EQ(result.mIntermediateNodesStates[0], OptimalPathResult::NodeState::ReservationRequestDoesntSent);
}

TEST(OptimalPathResult, MaxFlow) {
    OptimalPathResult result;
    result.mMaxPathFlow = A(5000);

    EXPECT_EQ(result.maxFlow(), A(5000));
}

TEST(OptimalPathResult, ShortageMaxFlowReduces) {
    OptimalPathResult result;
    result.mMaxPathFlow = A(1000);

    result.shortageMaxFlow(A(500));

    EXPECT_EQ(result.mMaxPathFlow, A(500));
}

TEST(OptimalPathResult, ShortageMaxFlowInitializes) {
    OptimalPathResult result;
    // mMaxPathFlow is 0 by default

    result.shortageMaxFlow(A(800));

    EXPECT_EQ(result.mMaxPathFlow, A(800));
}

TEST(OptimalPathResult, ShortageMaxFlowThrowsOnIncrease) {
    OptimalPathResult result;
    result.mMaxPathFlow = A(500);

    EXPECT_THROW(result.shortageMaxFlow(A(1000)), ValueError);
}

TEST(OptimalPathResult, PathReturnsExchangePathReference) {
    OptimalPathResult result;
    result.mPath.ids = {1, 2, 3};
    result.mPath.equivalents = {1, 1, 1};

    ExchangePath& path = result.path();

    EXPECT_EQ(path.ids.size(), 3);
    EXPECT_EQ(path.ids[0], 1);
}

TEST(OptimalPathResult, PathConstReturnsExchangePathReference) {
    OptimalPathResult result;
    result.mPath.ids = {1, 2, 3};
    result.mPath.equivalents = {1, 1, 1};

    const OptimalPathResult& constResult = result;
    const ExchangePath& path = constResult.path();

    EXPECT_EQ(path.ids.size(), 3);
}

TEST(OptimalPathResult, ContainsIntermediateNodesTrue) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationRequestDoesntSent,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent
    };

    EXPECT_TRUE(result.containsIntermediateNodes());
}

TEST(OptimalPathResult, ContainsIntermediateNodesFalse) {
    OptimalPathResult result;
    // mIntermediateNodesStates is empty

    EXPECT_FALSE(result.containsIntermediateNodes());
}

TEST(OptimalPathResult, CurrentIntermediateNodeAndPos) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    result.mPath.nodes = {addr1, addr2, addr3};
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestSent,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent
    };

    auto [node, pos] = result.currentIntermediateNodeAndPos();

    EXPECT_EQ(node, addr2);
    EXPECT_EQ(pos, 1);
}

TEST(OptimalPathResult, CurrentIntermediateNodeAndPosThrowsWhenAllProcessed) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");

    result.mPath.nodes = {addr1};
    result.mIntermediateNodesStates = {OptimalPathResult::NodeState::ReservationApproved};

    EXPECT_THROW(result.currentIntermediateNodeAndPos(), NotFoundError);
}

TEST(OptimalPathResult, NextIntermediateNodeAndPosFirstNode) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000"); // first intermediate
    auto addr2 = createAddress("127.0.0.1:2001"); // second intermediate

    // mPath.nodes contains only intermediate nodes (coordinator not included)
    result.mPath.nodes = {addr1, addr2};
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::NeighbourReservationApproved, // first intermediate (idx 0)
        OptimalPathResult::NodeState::ReservationRequestDoesntSent  // second intermediate (idx 1)
    };

    auto [node, pos] = result.nextIntermediateNodeAndPos();

    EXPECT_EQ(node, addr1); // first intermediate node
    EXPECT_EQ(pos, 0);      // index 0 in mIntermediateNodesStates
}

TEST(OptimalPathResult, NextIntermediateNodeAndPosSecondNode) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");

    result.mPath.nodes = {addr1, addr2};
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent
    };

    auto [node, pos] = result.nextIntermediateNodeAndPos();

    EXPECT_EQ(node, addr2);
    EXPECT_EQ(pos, 1);
}

TEST(OptimalPathResult, NextIntermediateNodeAndPosThrowsWhenAllProcessed) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");

    result.mPath.nodes = {addr1};
    result.mIntermediateNodesStates = {OptimalPathResult::NodeState::ReservationApproved};

    EXPECT_THROW(result.nextIntermediateNodeAndPos(), NotFoundError);
}

TEST(OptimalPathResult, ReservationRequestSentToAllNodesTrue) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestSent
    };

    EXPECT_TRUE(result.reservationRequestSentToAllNodes());
}

TEST(OptimalPathResult, ReservationRequestSentToAllNodesFalse) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent
    };

    EXPECT_FALSE(result.reservationRequestSentToAllNodes());
}

TEST(OptimalPathResult, IsNeighborAmountReservedTrue) {
    OptimalPathResult result;
    // Index 0 is first intermediate (neighbor), coordinator not included
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::NeighbourReservationApproved // neighbor (idx 0)
    };

    EXPECT_TRUE(result.isNeighborAmountReserved());
}

TEST(OptimalPathResult, IsNeighborAmountReservedFalse) {
    OptimalPathResult result;
    // Index 0 is first intermediate (neighbor), coordinator not included
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::NeighbourReservationRequestSent // neighbor (idx 0)
    };

    EXPECT_FALSE(result.isNeighborAmountReserved());
}

TEST(OptimalPathResult, IsWaitingForNeighborReservationResponseTrue) {
    OptimalPathResult result;
    // Index 0 is first intermediate (neighbor), coordinator not included
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::NeighbourReservationRequestSent // neighbor (idx 0)
    };

    EXPECT_TRUE(result.isWaitingForNeighborReservationResponse());
}

TEST(OptimalPathResult, IsWaitingForNeighborReservationResponseFalse) {
    OptimalPathResult result;
    // Index 0 is first intermediate (neighbor), coordinator not included
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::NeighbourReservationApproved // neighbor (idx 0)
    };

    EXPECT_FALSE(result.isWaitingForNeighborReservationResponse());
}

TEST(OptimalPathResult, IsWaitingForNeighborReservationPropagationResponseTrue) {
    OptimalPathResult result;
    // Index 0 is first intermediate (neighbor), coordinator not included
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationRequestSent // neighbor (idx 0)
    };

    EXPECT_TRUE(result.isWaitingForNeighborReservationPropagationResponse());
}

TEST(OptimalPathResult, IsWaitingForNeighborReservationPropagationResponseFalse) {
    OptimalPathResult result;
    // Index 0 is first intermediate (neighbor), coordinator not included
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::NeighbourReservationApproved // neighbor (idx 0)
    };

    EXPECT_FALSE(result.isWaitingForNeighborReservationPropagationResponse());
}

TEST(OptimalPathResult, IsWaitingForReservationResponseTrue) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");

    result.mPath.nodes = {addr1, addr2};
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestSent
    };

    EXPECT_TRUE(result.isWaitingForReservationResponse());
}

TEST(OptimalPathResult, IsWaitingForReservationResponseFalse) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");

    result.mPath.nodes = {addr1};
    result.mIntermediateNodesStates = {OptimalPathResult::NodeState::ReservationApproved};

    EXPECT_FALSE(result.isWaitingForReservationResponse());
}

TEST(OptimalPathResult, IsWaitingForReservationResponseFalseForDirectPath) {
    OptimalPathResult result;
    result.mPath.nodes = {createAddress("127.0.0.1:2000")};

    EXPECT_FALSE(result.isWaitingForReservationResponse());
}

TEST(OptimalPathResult, IsReadyToSendNextReservationRequestTrue) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent
    };

    EXPECT_TRUE(result.isReadyToSendNextReservationRequest());
}

TEST(OptimalPathResult, IsReadyToSendNextReservationRequestFalseWhenWaiting) {
    OptimalPathResult result;
    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");

    result.mPath.nodes = {addr1, addr2};
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestSent
    };

    EXPECT_FALSE(result.isReadyToSendNextReservationRequest());
}

TEST(OptimalPathResult, IsLastIntermediateNodeProcessedTrue) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestSent
    };

    EXPECT_TRUE(result.isLastIntermediateNodeProcessed());
}

TEST(OptimalPathResult, IsLastIntermediateNodeProcessedFalse) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent
    };

    EXPECT_FALSE(result.isLastIntermediateNodeProcessed());
}

TEST(OptimalPathResult, IsLastIntermediateNodeApprovedTrue) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::NeighbourReservationApproved
    };

    EXPECT_TRUE(result.isLastIntermediateNodeApproved());
}

TEST(OptimalPathResult, IsLastIntermediateNodeApprovedTrueWithReservationApproved) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationApproved
    };

    EXPECT_TRUE(result.isLastIntermediateNodeApproved());
}

TEST(OptimalPathResult, IsLastIntermediateNodeApprovedFalse) {
    OptimalPathResult result;
    result.mIntermediateNodesStates = {
        OptimalPathResult::NodeState::ReservationApproved,
        OptimalPathResult::NodeState::ReservationRequestSent
    };

    EXPECT_FALSE(result.isLastIntermediateNodeApproved());
}

TEST(OptimalPathResult, IsValidTrue) {
    OptimalPathResult result;
    result.mIsValid = true;

    EXPECT_TRUE(result.isValid());
}

TEST(OptimalPathResult, IsValidFalse) {
    OptimalPathResult result;
    result.mIsValid = false;

    EXPECT_FALSE(result.isValid());
}

TEST(OptimalPathResult, SetUnusable) {
    OptimalPathResult result;
    result.mIsValid = true;
    result.mMaxPathFlow = A(1000);

    result.setUnusable();

    EXPECT_FALSE(result.mIsValid);
    EXPECT_EQ(result.mMaxPathFlow, A(0));
}

TEST(OptimalPathResult, NoMPathFieldPresent) {
    // This test verifies that mPath (Path::Shared) field is NOT present
    // Compilation test - if this compiles, the requirement is met
    OptimalPathResult result;

    // We should have mPath as ExchangePath, not Path::Shared
    ExchangePath& path = result.path();
    (void)path; // Suppress unused variable warning

    SUCCEED();
}

// Test Category: calculateFlows method (Task 06-12)

TEST(OptimalPathResultFlowCalculation, SimplePathWithoutExchanges) {
    OptimalPathResult result;
    result.optimal_flow = A(3000);
    result.mPath.ids = {1, 2, 3, 4}; // 4 nodes
    result.mPath.equivalents = {1001, 1001, 1001, 1001}; // all same equivalent

    result.calculateFlows(A(2010));

    EXPECT_EQ(result.paymentFlow, A(2010));
    EXPECT_EQ(result.flows.size(), 3); // N-1 flows for N nodes
    EXPECT_EQ(result.flows[0].first, A(2010));
    EXPECT_EQ(result.flows[0].second, 1001);
    EXPECT_EQ(result.flows[1].first, A(2010));
    EXPECT_EQ(result.flows[1].second, 1001);
    EXPECT_EQ(result.flows[2].first, A(2010));
    EXPECT_EQ(result.flows[2].second, 1001);
}

TEST(OptimalPathResultFlowCalculation, PathWithOneExchange) {
    OptimalPathResult result;
    result.optimal_flow = A(3010);
    result.received_amount = A(150);

    // Path: A → B → C(exchange) → C → D
    // Exchange at C: 1001 → 2002, rate 0.05 (exchangeRate=5, exchangeRateShift=-2)
    result.mPath.ids = {1, 2, 3, 3, 4};
    result.mPath.equivalents = {1001, 1001, 1001, 2002, 2002};

    ExchangeStep exchange;
    exchange.nodeID = 3;
    exchange.fromEquivalent = 1001;
    exchange.toEquivalent = 2002;
    exchange.exchangeRate = A(5);
    exchange.exchangeRateShift = -2; // rate = 5 * 10^(-2) = 0.05
    result.mPath.exchangeSteps.push_back(exchange);

    result.calculateFlows(A(2000));

    EXPECT_EQ(result.paymentFlow, A(2000));
    EXPECT_EQ(result.flows.size(), 3); // 4 regular edges (excluding exchange self-edge)

    // A→B: 2000 in equiv 1001
    EXPECT_EQ(result.flows[0].first, A(2000));
    EXPECT_EQ(result.flows[0].second, 1001);

    // B→C: 2000 in equiv 1001
    EXPECT_EQ(result.flows[1].first, A(2000));
    EXPECT_EQ(result.flows[1].second, 1001);

    // C→D: 2000 * 0.05 = 100 in equiv 2002
    EXPECT_EQ(result.flows[2].first, A(100));
    EXPECT_EQ(result.flows[2].second, 2002);
}

TEST(OptimalPathResultFlowCalculation, PathWithMultipleExchanges) {
    OptimalPathResult result;
    result.optimal_flow = A(5000);

    // Path with two exchanges
    result.mPath.ids = {1, 2, 2, 3, 3, 4};
    result.mPath.equivalents = {1001, 1001, 2002, 2002, 3003, 3003};

    // First exchange at node 2: 1001 → 2002, rate 2.0
    ExchangeStep exchange1;
    exchange1.nodeID = 2;
    exchange1.fromEquivalent = 1001;
    exchange1.toEquivalent = 2002;
    exchange1.exchangeRate = A(2);
    exchange1.exchangeRateShift = 0; // rate = 2.0
    result.mPath.exchangeSteps.push_back(exchange1);

    // Second exchange at node 3: 2002 → 3003, rate 0.5
    ExchangeStep exchange2;
    exchange2.nodeID = 3;
    exchange2.fromEquivalent = 2002;
    exchange2.toEquivalent = 3003;
    exchange2.exchangeRate = A(5);
    exchange2.exchangeRateShift = -1; // rate = 0.5
    result.mPath.exchangeSteps.push_back(exchange2);

    result.calculateFlows(A(1000));

    EXPECT_EQ(result.paymentFlow, A(1000));
    EXPECT_EQ(result.flows.size(), 3); // 3 regular edges

    // 1→2: 1000 in equiv 1001
    EXPECT_EQ(result.flows[0].first, A(1000));
    EXPECT_EQ(result.flows[0].second, 1001);

    // 2→3: 1000 * 2.0 = 2000 in equiv 2002
    EXPECT_EQ(result.flows[1].first, A(2000));
    EXPECT_EQ(result.flows[1].second, 2002);

    // 3→4: 2000 * 0.5 = 1000 in equiv 3003
    EXPECT_EQ(result.flows[2].first, A(1000));
    EXPECT_EQ(result.flows[2].second, 3003);
}

TEST(OptimalPathResultFlowCalculation, ExceedsOptimalFlowThrowsError) {
    OptimalPathResult result;
    result.optimal_flow = A(1000);
    result.mPath.ids = {1, 2};
    result.mPath.equivalents = {1001, 1001};

    EXPECT_THROW(result.calculateFlows(A(1500)), ValueError);
}

TEST(OptimalPathResultFlowCalculation, ExchangeLimitMinThrowsError) {
    OptimalPathResult result;
    result.optimal_flow = A(5000);
    result.mPath.ids = {1, 2, 2, 3};
    result.mPath.equivalents = {1001, 1001, 2002, 2002};

    ExchangeStep exchange;
    exchange.nodeID = 2;
    exchange.fromEquivalent = 1001;
    exchange.toEquivalent = 2002;
    exchange.exchangeRate = A(1);
    exchange.exchangeRateShift = 0;
    exchange.minExchangeAmount = A(1000); // Minimum 1000
    result.mPath.exchangeSteps.push_back(exchange);

    // Try to exchange 500, which is below minimum
    EXPECT_THROW(result.calculateFlows(A(500)), ValueError);
}

TEST(OptimalPathResultFlowCalculation, ExchangeLimitMaxThrowsError) {
    OptimalPathResult result;
    result.optimal_flow = A(5000);
    result.mPath.ids = {1, 2, 2, 3};
    result.mPath.equivalents = {1001, 1001, 2002, 2002};

    ExchangeStep exchange;
    exchange.nodeID = 2;
    exchange.fromEquivalent = 1001;
    exchange.toEquivalent = 2002;
    exchange.exchangeRate = A(1);
    exchange.exchangeRateShift = 0;
    exchange.maxExchangeAmount = A(1000); // Maximum 1000
    result.mPath.exchangeSteps.push_back(exchange);

    // Try to exchange 1500, which exceeds maximum
    EXPECT_THROW(result.calculateFlows(A(1500)), ValueError);
}

TEST(OptimalPathResultFlowCalculation, CorrectFlowCount) {
    OptimalPathResult result;
    result.optimal_flow = A(5000);

    // Test with different path lengths
    // 2 nodes → 1 flow
    result.mPath.ids = {1, 2};
    result.mPath.equivalents = {1001, 1001};
    result.calculateFlows(A(1000));
    EXPECT_EQ(result.flows.size(), 1);

    // 5 nodes → 4 flows
    result.mPath.ids = {1, 2, 3, 4, 5};
    result.mPath.equivalents = {1001, 1001, 1001, 1001, 1001};
    result.calculateFlows(A(1000));
    EXPECT_EQ(result.flows.size(), 4);
}

TEST(OptimalPathResultFlowCalculation, Recalculation) {
    OptimalPathResult result;
    result.optimal_flow = A(3000);
    result.mPath.ids = {1, 2, 3};
    result.mPath.equivalents = {1001, 1001, 1001};

    // First calculation
    result.calculateFlows(A(1000));
    EXPECT_EQ(result.paymentFlow, A(1000));
    EXPECT_EQ(result.flows.size(), 2);
    EXPECT_EQ(result.flows[0].first, A(1000));

    // Recalculation with different amount
    result.calculateFlows(A(2000));
    EXPECT_EQ(result.paymentFlow, A(2000));
    EXPECT_EQ(result.flows.size(), 2);
    EXPECT_EQ(result.flows[0].first, A(2000));
}

TEST(OptimalPathResultFlowCalculation, InvalidPathEmptyIds) {
    OptimalPathResult result;
    result.optimal_flow = A(1000);
    result.mPath.ids = {}; // Empty
    result.mPath.equivalents = {};

    EXPECT_THROW(result.calculateFlows(A(500)), ValueError);
}

TEST(OptimalPathResultFlowCalculation, InvalidPathMismatchedSizes) {
    OptimalPathResult result;
    result.optimal_flow = A(1000);
    result.mPath.ids = {1, 2, 3};
    result.mPath.equivalents = {1001, 1001}; // Size mismatch

    EXPECT_THROW(result.calculateFlows(A(500)), ValueError);
}
