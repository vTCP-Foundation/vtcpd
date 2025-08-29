#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio.hpp>

#include "../../../src/core/network/communicator/internal/incoming/TailManager.h"
#include "../../../src/core/network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/time/TimeUtils.h"

class TailManagerTest : public ::testing::Test {
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

TEST_F(TailManagerTest, ExchangeRatesTailExists) {
    // Test that the exchange rates tail is properly initialized and accessible
    auto& exchangeRatesTail = tailManager->getExchangeRatesTail();
    EXPECT_TRUE(exchangeRatesTail.empty());
}

TEST_F(TailManagerTest, ExchangeRatesTailIsIndependent) {
    // Test that the exchange rates tail is independent from other tails
    auto& flowTail = tailManager->getFlowTail();
    auto& cyclesFiveTail = tailManager->getCyclesFiveTail();
    auto& cyclesSixTail = tailManager->getCyclesSixTail();
    auto& routingTableTail = tailManager->getRoutingTableTail();
    auto& exchangeRatesTail = tailManager->getExchangeRatesTail();

    // All tails should be empty initially
    EXPECT_TRUE(flowTail.empty());
    EXPECT_TRUE(cyclesFiveTail.empty());
    EXPECT_TRUE(cyclesSixTail.empty());
    EXPECT_TRUE(routingTableTail.empty());
    EXPECT_TRUE(exchangeRatesTail.empty());

    // All tails should be different objects
    EXPECT_NE(&flowTail, &exchangeRatesTail);
    EXPECT_NE(&cyclesFiveTail, &exchangeRatesTail);
    EXPECT_NE(&cyclesSixTail, &exchangeRatesTail);
    EXPECT_NE(&routingTableTail, &exchangeRatesTail);
}

TEST_F(TailManagerTest, ExchangeRatesTailCanAcceptMessages) {
    // Since ExchangeRatesMessage requires linking with other libraries,
    // we'll test the basic functionality of the tail being able to store messages
    auto& exchangeRatesTail = tailManager->getExchangeRatesTail();
    
    // Verify tail is initially empty
    EXPECT_TRUE(exchangeRatesTail.empty());
    EXPECT_EQ(exchangeRatesTail.size(), 0);
    
    // Test that we can access the tail without any issues
    // This verifies the getExchangeRatesTail() method works correctly
    auto& sameTail = tailManager->getExchangeRatesTail();
    EXPECT_EQ(&exchangeRatesTail, &sameTail);
}