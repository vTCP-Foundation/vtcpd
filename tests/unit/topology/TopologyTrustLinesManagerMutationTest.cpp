#include <gtest/gtest.h>
#include <memory>

#include "../../../src/core/topology/manager/TopologyTrustLinesManager.h"
#include "../../../src/core/topology/TopologyTrustLine.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"

class TopologyTrustLinesManagerMutationTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = std::make_unique<Logger>();
        ownAddress = std::make_shared<IPv4WithPortAddress>("127.0.0.1:2000");
        manager = std::make_unique<TopologyTrustLinesManager>(
            7,              // equivalent ID
            ownAddress,
            false,          // not a gateway for tests
            ioContext,
            *logger);
    }

    void TearDown() override {
        manager.reset();
        logger.reset();
        ownAddress.reset();
    }

    std::unique_ptr<Logger> logger;
    std::unique_ptr<TopologyTrustLinesManager> manager;
    BaseAddress::Shared ownAddress;
    boost::asio::io_context ioContext;
};

TEST_F(TopologyTrustLinesManagerMutationTest, removeTrustLineRemovesSpecificEdge) {
    auto amount = std::make_shared<TrustLineAmount>(100);

    manager->addTrustLine(std::make_shared<TopologyTrustLine>(1, 2, amount));
    manager->addTrustLine(std::make_shared<TopologyTrustLine>(1, 3, amount));
    ASSERT_EQ(manager->trustLinesCounts(), 2);

    manager->removeTrustLine(1, 2);

    EXPECT_EQ(manager->trustLinesCounts(), 1);
    auto remaining = manager->trustLinePtrsSet(1);
    ASSERT_EQ(remaining.size(), 1);
    auto it = remaining.begin();
    EXPECT_EQ((*it)->topologyTrustLine()->targetID(), 3);
}

TEST_F(TopologyTrustLinesManagerMutationTest, removeTrustLineRemovesLastEdgeAndCleansContainer) {
    auto amount = std::make_shared<TrustLineAmount>(50);

    manager->addTrustLine(std::make_shared<TopologyTrustLine>(2, 9, amount));
    ASSERT_EQ(manager->trustLinesCounts(), 1);

    manager->removeTrustLine(2, 9);

    EXPECT_EQ(manager->trustLinesCounts(), 0);
    auto removedSet = manager->trustLinePtrsSet(2);
    EXPECT_TRUE(removedSet.empty());
}

TEST_F(TopologyTrustLinesManagerMutationTest, removeTrustLineGracefullyHandlesMissingEntries) {
    auto amount = std::make_shared<TrustLineAmount>(20);
    manager->addTrustLine(std::make_shared<TopologyTrustLine>(4, 5, amount));
    ASSERT_EQ(manager->trustLinesCounts(), 1);

    // Attempt to remove non-existing direction and foreign source
    manager->removeTrustLine(4, 6);
    manager->removeTrustLine(11, 42);

    EXPECT_EQ(manager->trustLinesCounts(), 1);
}

TEST_F(TopologyTrustLinesManagerMutationTest, removeTrustLineIsIdempotentForSameEdge) {
    auto amount = std::make_shared<TrustLineAmount>(75);
    manager->addTrustLine(std::make_shared<TopologyTrustLine>(6, 8, amount));

    manager->removeTrustLine(6, 8);
    ASSERT_EQ(manager->trustLinesCounts(), 0);

    // A second removal of the same edge must be a no-op and leave the topology intact.
    manager->removeTrustLine(6, 8);
    EXPECT_EQ(manager->trustLinesCounts(), 0);
    auto snapshot = manager->trustLinePtrsSet(6);
    EXPECT_TRUE(snapshot.empty());
}
