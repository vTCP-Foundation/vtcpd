#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <vector>

#include "../../../src/core/equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../src/core/contractors/ContractorsManager.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/crypto/keychain.h"
#include "../../../src/core/interface/events_interface/interface/EventsInterfaceManager.h"

class EquivalentsSubsystemsRouterParticipantsTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        testDbDir = std::string("build-tests/testdb_router_participants_") + testInfo->name();
        std::filesystem::remove_all(testDbDir);

        eventsManager = std::make_unique<EventsInterfaceManager>(
            std::vector<std::pair<std::string, SerializedEventType>>{},
            std::vector<std::pair<std::string, bool>>{},
            logger);

        storage = std::make_unique<StorageHandlerSQLite>(testDbDir, "router.db", logger);
        keystore = std::make_unique<crypto::Keystore>(logger);
        {
            auto ioTransaction = storage->beginTransaction();
            keystore->init(ioTransaction);
        }

        std::vector<std::pair<std::string, std::string>> ownAddresses = {{"ipv4", "127.0.0.1:2000"}};
        contractors = std::make_unique<ContractorsManager>(ownAddresses, storage.get(), logger);

        std::vector<SerializedEquivalent> gateways;
        router = std::make_unique<EquivalentsSubsystemsRouter>(
            storage.get(),
            keystore.get(),
            contractors.get(),
            eventsManager.get(),
            ioContext,
            gateways,
            logger);
    }

    void TearDown() override {
        router.reset();
        contractors.reset();
        keystore.reset();
        storage.reset();
        eventsManager.reset();
        std::filesystem::remove_all(testDbDir);
    }

    Logger logger;
    boost::asio::io_context ioContext;
    std::unique_ptr<EventsInterfaceManager> eventsManager;
    std::unique_ptr<StorageHandlerSQLite> storage;
    std::unique_ptr<crypto::Keystore> keystore;
    std::unique_ptr<ContractorsManager> contractors;
    std::unique_ptr<EquivalentsSubsystemsRouter> router;
    std::string testDbDir;
};

TEST_F(EquivalentsSubsystemsRouterParticipantsTest, participantsIDsIncludesSelfAndFollowsRegistrationOrder) {
    auto snapshot = router->participantsIDs();
    ASSERT_FALSE(snapshot.empty());
    EXPECT_EQ(snapshot.front(), 0);

    auto addr1 = std::make_shared<IPv4WithPortAddress>("192.168.10.1:10001");
    auto addr2 = std::make_shared<IPv4WithPortAddress>("192.168.10.2:10002");

    auto id1 = router->getOrCreateParticipantID(addr1);
    auto id2 = router->getOrCreateParticipantID(addr2);
    ASSERT_NE(id1, id2);

    auto orderedIds = router->participantsIDs();
    std::vector<ContractorID> expected{0, id1, id2};
    EXPECT_EQ(orderedIds, expected);
}

TEST_F(EquivalentsSubsystemsRouterParticipantsTest, participantsIDsDoesNotDuplicateExistingContractors) {
    auto addr = std::make_shared<IPv4WithPortAddress>("10.30.0.5:3005");

    auto id = router->getOrCreateParticipantID(addr);
    ASSERT_GT(id, 0);

    // Re-register the same address multiple times.
    for (int i = 0; i < 3; ++i) {
        auto repeatID = router->getOrCreateParticipantID(addr);
        EXPECT_EQ(repeatID, id);
    }

    auto ids = router->participantsIDs();
    std::vector<ContractorID> expected{0, id};
    EXPECT_EQ(ids, expected);
}

TEST_F(EquivalentsSubsystemsRouterParticipantsTest, participantsIDsReturnsSnapshotCopy) {
    auto firstBatch = router->participantsIDs();
    ASSERT_FALSE(firstBatch.empty());

    // Mutate caller-owned copy to ensure router's internal storage is unaffected.
    firstBatch.push_back(9999);

    auto addr = std::make_shared<IPv4WithPortAddress>("10.30.0.6:3006");
    auto newId = router->getOrCreateParticipantID(addr);

    auto secondBatch = router->participantsIDs();
    EXPECT_NE(std::find(secondBatch.begin(), secondBatch.end(), newId), secondBatch.end());
    EXPECT_EQ(std::find(secondBatch.begin(), secondBatch.end(), 9999), secondBatch.end());
}
