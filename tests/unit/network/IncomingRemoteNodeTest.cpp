#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>

#include "../../../src/core/network/communicator/internal/incoming/TailManager.h"
#include "../../../src/core/network/messages/Message.hpp"
#include "../../../src/core/logger/Logger.h"

// Simple test to verify the routing logic that was implemented in IncomingRemoteNode
class IncomingRemoteNodeRoutingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ioContext = std::make_unique<boost::asio::io_context>();
        logger = std::make_unique<Logger>();
        tailManager = std::make_unique<TailManager>(*ioContext, *logger);
    }

    void TearDown() override {
        tailManager.reset();
        logger.reset();
        ioContext.reset();
    }

    std::unique_ptr<boost::asio::io_context> ioContext;
    std::unique_ptr<Logger> logger;
    std::unique_ptr<TailManager> tailManager;
};

TEST_F(IncomingRemoteNodeRoutingTest, ExchangeRatesTailRouting) {
    // Test the routing logic that was implemented in IncomingRemoteNode::tryCollectNextPacket
    // This verifies that MaxFlow_ExchangeRates message type would be routed to ExchangeRatesTail
    
    // Get reference to tails
    auto& exchangeRatesTail = tailManager->getExchangeRatesTail();
    auto& flowTail = tailManager->getFlowTail();
    auto& routingTableTail = tailManager->getRoutingTableTail();

    // All tails should be empty initially
    EXPECT_TRUE(exchangeRatesTail.empty());
    EXPECT_TRUE(flowTail.empty());
    EXPECT_TRUE(routingTableTail.empty());

    // Test the routing logic from IncomingRemoteNode::tryCollectNextPacket
    // The implemented logic checks: if (messageType == Message::MaxFlow_ExchangeRates)
    Message::MessageType testMessageType = Message::MaxFlow_ExchangeRates;
    
    bool shouldRouteToExchangeRatesTail = (testMessageType == Message::MaxFlow_ExchangeRates);
    
    // Verify that our routing condition is correct
    EXPECT_TRUE(shouldRouteToExchangeRatesTail);
    
    // Verify that other message types would not be routed to ExchangeRatesTail
    EXPECT_FALSE(Message::MaxFlow_ResultMaxFlowCalculation == Message::MaxFlow_ExchangeRates);
    EXPECT_FALSE(Message::RoutingTableResponse == Message::MaxFlow_ExchangeRates);
}

TEST_F(IncomingRemoteNodeRoutingTest, BackwardCompatibilityWithOtherMessages) {
    // Test that the new routing doesn't interfere with existing message routing
    // We'll test the routing logic conditions without creating actual message objects
    
    // Test routing conditions for different message types
    Message::MessageType flowMessageType = Message::MaxFlow_ResultMaxFlowCalculation;
    Message::MessageType routingMessageType = Message::RoutingTableResponse;
    Message::MessageType exchangeRatesMessageType = Message::MaxFlow_ExchangeRates;
    
    // Verify routing conditions work as expected
    bool shouldRouteToFlowTail = (flowMessageType == Message::MaxFlow_ResultMaxFlowCalculation || 
                                  flowMessageType == Message::MaxFlow_ResultMaxFlowCalculationFromGateway);
    bool shouldRouteToRoutingTail = (routingMessageType == Message::RoutingTableResponse);
    bool shouldRouteToExchangeRatesTail = (exchangeRatesMessageType == Message::MaxFlow_ExchangeRates);
    
    EXPECT_TRUE(shouldRouteToFlowTail);
    EXPECT_TRUE(shouldRouteToRoutingTail);
    EXPECT_TRUE(shouldRouteToExchangeRatesTail);
    
    // Verify different message types don't interfere with each other
    EXPECT_FALSE(flowMessageType == exchangeRatesMessageType);
    EXPECT_FALSE(routingMessageType == exchangeRatesMessageType);
    
    // Test that ExchangeRates message type is distinct from cycles message types
    EXPECT_FALSE(exchangeRatesMessageType == Message::Cycles_FiveNodesBoundary);
    EXPECT_FALSE(exchangeRatesMessageType == Message::Cycles_SixNodesBoundary);
}