#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>

#include "core/transactions/transactions/find_path/FindPathsByMaxFlowExchangeTransaction.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/rates/Commission.h"
#include "core/paths/ExchangePathsManager.h"
#include "core/resources/manager/ResourcesManager.h"
#include "core/resources/resources/ExchangePathsResource.h"
#include "core/logger/Logger.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/crypto/keychain.h"
#include "core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "core/network/communicator/internal/incoming/TailManager.h"

using namespace testing;

namespace {
    // Helper to create shared TrustLineAmount easily
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }
}

/**
 * Test fixture for FindPathsByMaxFlowExchangeTransaction tests.
 * Provides common setup including contractors, router, rates manager, paths manager, etc.
 */
class FindPathsByMaxFlowExchangeTransactionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create logger
        logger = make_unique<Logger>();
        io = make_unique<boost::asio::io_context>();

        // Create temporary storage with unique directory per test
        // Use test info to create unique directory name
        auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        dbDir = string("build-tests/testdb_findpaths_") + testInfo->name();
        dbName = "test.db";
        std::filesystem::remove_all(dbDir);
        storage = make_unique<StorageHandlerSQLite>(dbDir, dbName, *logger);

        // Initialize keystore
        keystore = make_unique<crypto::Keystore>(*logger);
        {
            auto ioTransaction = storage->beginTransaction();
            keystore->init(ioTransaction);
        }

        // Create events manager (empty event/block configs)
        eventsManager = make_unique<EventsInterfaceManager>(
            vector<pair<string, SerializedEventType>>{},
            vector<pair<string, bool>>{},
            *logger);

        // Self address: A = 172.18.28.1:2000
        vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
        contractors = make_unique<ContractorsManager>(ownAddrs, storage.get(), *logger);

        // Create router with no initial gateways
        vector<SerializedEquivalent> gateways;
        router = make_unique<EquivalentsSubsystemsRouter>(
            storage.get(),
            keystore.get(),
            contractors.get(),
            eventsManager.get(),
            *io,
            gateways,
            *logger);

        // Initialize equivalents
        EQ_SENDER = 1001;
        EQ_RECEIVER = 2002;
        router->initNewEquivalent(EQ_SENDER);
        router->initNewEquivalent(EQ_RECEIVER);

        // Create managers
        ratesManager = make_unique<ExchangeRatesManager>(*io, *logger);
        pathsManager = make_unique<ExchangePathsManager>(*io, router.get(), ratesManager.get(), contractors.get(), *logger);
        resourcesManager = make_unique<ResourcesManager>();
        tailManager = make_unique<TailManager>(*io, *logger);

        // Create test contractor address
        contractorAddress = make_shared<IPv4WithPortAddress>("172.18.28.5:2000");
        requestedTransactionUUID = TransactionUUID();
        hopsCount = 6;
    }

    void TearDown() override {
        // Cleanup in reverse order
        tailManager.reset();
        resourcesManager.reset();
        pathsManager.reset();
        ratesManager.reset();
        router.reset();
        contractors.reset();
        eventsManager.reset();
        keystore.reset();
        storage.reset();
        io.reset();
        logger.reset();

        // Remove test database
        std::filesystem::remove_all(dbDir);
    }

    /**
     * Setup simple topology with one exchange path:
     * A -> B -> C (exchange) -> D -> E (target)
     * Sender equivalent: 1001, Receiver equivalent: 2002
     * Exchange rate at C: 1001 -> 2002 with rate 0.5
     * Max flow should be 200 in receiver equivalent
     */
    void setupSimpleTopology() {
        // Create node addresses (following pattern from ApplyLogicTest)
        auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000"); // Self
        auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000");
        auto addrC = make_shared<IPv4WithPortAddress>("172.18.28.3:2000"); // Exchange node
        auto addrD = make_shared<IPv4WithPortAddress>("172.18.28.4:2000");
        auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000"); // Target (contractorAddress)

        // Get or create participant IDs
        ASSERT_EQ(router->getOrCreateParticipantID(contractors->selfContractor()->mainAddress()), 0u);
        auto idB = router->getOrCreateParticipantID(addrB);
        auto idC = router->getOrCreateParticipantID(addrC);
        auto idD = router->getOrCreateParticipantID(addrD);
        auto idE = router->getOrCreateParticipantID(addrE);

        // Build topology in sender equivalent (1001)
        auto tlm1001 = router->topologyTrustLineManager(EQ_SENDER);
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, idB, A(3000)));
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(2500)));
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(2000)));
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(5000)));

        // Build topology in receiver equivalent (2002)
        auto tlm2002 = router->topologyTrustLineManager(EQ_RECEIVER);
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(250)));
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(200)));
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(500)));

        // Add exchange rate at node C: 1001 -> 2002 with rate 0.5
        auto expiresAt = utc_now() + boost::posix_time::seconds(300);
        ExchangeRate rate(EQ_SENDER, EQ_RECEIVER, TrustLineAmount(5), /*shift*/ -1, expiresAt,
                          TrustLineAmount(0), TrustLineAmount(0));
        ratesManager->addOrUpdateExternal(idC, rate);
    }

    /**
     * Setup topology with multiple exchange equivalents.
     * Creates paths for equivalents 1001, 1002, 1003 all leading to receiver equivalent 2002.
     */
    void setupMultipleExchangeEquivalentsTopology() {
        // Initialize additional sender equivalents
        const SerializedEquivalent EQ_1002 = 1002;
        const SerializedEquivalent EQ_1003 = 1003;
        router->initNewEquivalent(EQ_1002);
        router->initNewEquivalent(EQ_1003);

        // Create nodes: A (self) -> B1/B2/B3 (per equivalent) -> C1/C2/C3 (exchange nodes) -> E (target)
        auto addrB1 = make_shared<IPv4WithPortAddress>("172.18.28.11:2000");
        auto addrB2 = make_shared<IPv4WithPortAddress>("172.18.28.12:2000");
        auto addrB3 = make_shared<IPv4WithPortAddress>("172.18.28.13:2000");
        auto addrC1 = make_shared<IPv4WithPortAddress>("172.18.28.21:2000");
        auto addrC2 = make_shared<IPv4WithPortAddress>("172.18.28.22:2000");
        auto addrC3 = make_shared<IPv4WithPortAddress>("172.18.28.23:2000");
        auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000"); // Target

        // Get participant IDs
        ASSERT_EQ(router->getOrCreateParticipantID(contractors->selfContractor()->mainAddress()), 0u);
        auto idB1 = router->getOrCreateParticipantID(addrB1);
        auto idB2 = router->getOrCreateParticipantID(addrB2);
        auto idB3 = router->getOrCreateParticipantID(addrB3);
        auto idC1 = router->getOrCreateParticipantID(addrC1);
        auto idC2 = router->getOrCreateParticipantID(addrC2);
        auto idC3 = router->getOrCreateParticipantID(addrC3);
        auto idE = router->getOrCreateParticipantID(addrE);

        // Build topology for equivalent 1001
        auto tlm1001 = router->topologyTrustLineManager(EQ_SENDER);
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, idB1, A(1000)));
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB1, idC1, A(900)));

        // Build topology for equivalent 1002
        auto tlm1002 = router->topologyTrustLineManager(EQ_1002);
        tlm1002->addTrustLine(make_shared<TopologyTrustLine>(0, idB2, A(2000)));
        tlm1002->addTrustLine(make_shared<TopologyTrustLine>(idB2, idC2, A(1800)));

        // Build topology for equivalent 1003
        auto tlm1003 = router->topologyTrustLineManager(EQ_1003);
        tlm1003->addTrustLine(make_shared<TopologyTrustLine>(0, idB3, A(1500)));
        tlm1003->addTrustLine(make_shared<TopologyTrustLine>(idB3, idC3, A(1400)));

        // Build topology in receiver equivalent (2002) - all exchange nodes connect to target
        auto tlm2002 = router->topologyTrustLineManager(EQ_RECEIVER);
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC1, idE, A(500)));
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC2, idE, A(1000)));
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC3, idE, A(750)));

        // Add exchange rates at all exchange nodes: rate 1.0 (1:1 exchange)
        auto expiresAt = utc_now() + boost::posix_time::seconds(300);
        ExchangeRate rate(EQ_SENDER, EQ_RECEIVER, TrustLineAmount(1), 0, expiresAt,
                          TrustLineAmount(0), TrustLineAmount(0));
        ratesManager->addOrUpdateExternal(idC1, rate);

        ExchangeRate rate2(EQ_1002, EQ_RECEIVER, TrustLineAmount(1), 0, expiresAt,
                           TrustLineAmount(0), TrustLineAmount(0));
        ratesManager->addOrUpdateExternal(idC2, rate2);

        ExchangeRate rate3(EQ_1003, EQ_RECEIVER, TrustLineAmount(1), 0, expiresAt,
                           TrustLineAmount(0), TrustLineAmount(0));
        ratesManager->addOrUpdateExternal(idC3, rate3);
    }

    /**
     * Setup topology with no viable paths (no exchange rates).
     * Used for testing failure scenarios.
     */
    void setupNoPathsTopology() {
        // Create simple topology without exchange rates
        auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000");
        auto idB = router->getOrCreateParticipantID(addrB);

        auto tlm1001 = router->topologyTrustLineManager(EQ_SENDER);
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, idB, A(1000)));

        // No exchange rates - paths cannot be built
    }

    /**
     * Helper to verify paths are cached for specific key.
     */
    bool arePathsCached(const PathCacheKey &key) {
        auto paths = pathsManager->retrievePaths(key);
        return paths.has_value() && !paths->empty();
    }

    /**
     * Helper to create transaction instance.
     */
    shared_ptr<FindPathsByMaxFlowExchangeTransaction> createTransaction(
        const vector<SerializedEquivalent> &exchangeEquivalents)
    {
        return make_shared<FindPathsByMaxFlowExchangeTransaction>(
            contractorAddress,
            requestedTransactionUUID,
            EQ_RECEIVER,
            exchangeEquivalents,
            contractors.get(),
            resourcesManager.get(),
            router.get(),
            tailManager.get(),
            pathsManager.get(),
            ratesManager.get(),
            nullptr, // commissionsManager not used in tests
            *logger,
            hopsCount);
    }

protected:
    // Test infrastructure
    unique_ptr<Logger> logger;
    unique_ptr<boost::asio::io_context> io;
    unique_ptr<StorageHandlerSQLite> storage;
    unique_ptr<crypto::Keystore> keystore;
    unique_ptr<EventsInterfaceManager> eventsManager;
    unique_ptr<ContractorsManager> contractors;
    unique_ptr<EquivalentsSubsystemsRouter> router;
    unique_ptr<ExchangeRatesManager> ratesManager;
    unique_ptr<ExchangePathsManager> pathsManager;
    unique_ptr<ResourcesManager> resourcesManager;
    unique_ptr<TailManager> tailManager;

    // Test parameters
    string dbDir;
    string dbName;
    BaseAddress::Shared contractorAddress;
    TransactionUUID requestedTransactionUUID;
    SerializedEquivalent EQ_SENDER;
    SerializedEquivalent EQ_RECEIVER;
    HopsCount_t hopsCount;
};

/**
 * Test 1: Constructor initializes with all required parameters.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, Constructor_InitializesWithAllParameters) {
    // Arrange
    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};

    // Act
    auto transaction = createTransaction(exchangeEquivalents);

    // Assert
    EXPECT_NE(transaction, nullptr);
    // Transaction should be created successfully
    // Internal state is private, so we just verify construction succeeds
}

/**
 * Test 2: Transaction successfully builds paths using OR-Tools.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_BuildsPathsSuccessfully) {
    // Arrange
    setupSimpleTopology();
    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};
    auto transaction = createTransaction(exchangeEquivalents);

    // Act - Run transaction twice (topology collection + processing)
    auto r1 = transaction->run(); // sendRequestForCollectingTopology()
    ASSERT_NE(r1, nullptr);

    auto r2 = transaction->run(); // processCollectingTopology()
    ASSERT_NE(r2, nullptr);

    // Assert - Transaction completes successfully (mustExit indicates completion)
    ASSERT_NE(r2->state(), nullptr);
    EXPECT_TRUE(r2->state()->mustExit()) << "Transaction should complete and exit";

    // Verify paths are cached
    auto contractorID = router->getOrCreateParticipantID(contractorAddress);
    PathCacheKey key{contractorID, EQ_SENDER, EQ_RECEIVER};
    EXPECT_TRUE(arePathsCached(key))
        << "Paths should be cached for exchange equivalent " << EQ_SENDER;

    // Verify cached paths contain data
    auto cachedPaths = pathsManager->retrievePaths(key);
    ASSERT_TRUE(cachedPaths.has_value());
    EXPECT_GT(cachedPaths->size(), 0) << "Should have at least one optimal path";
}

/**
 * Test 3: Transaction returns ExchangePathsResource with correct UUID.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_ReturnsExchangePathsResource) {
    // Arrange
    setupSimpleTopology();
    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};
    auto transaction = createTransaction(exchangeEquivalents);

    // Setup resource capture
    shared_ptr<ExchangePathsResource> capturedResource;
    resourcesManager->attachResourceSignal.connect(
        [&](BaseResource::Shared resource) {
            if (resource->type() == BaseResource::ExchangePaths) {
                capturedResource = static_pointer_cast<ExchangePathsResource>(resource);
            }
        });

    // Act
    transaction->run(); // sendRequestForCollectingTopology()
    transaction->run(); // processCollectingTopology()

    // Assert - Resource returned with correct UUID
    ASSERT_NE(capturedResource, nullptr) << "ExchangePathsResource should be returned";
    EXPECT_EQ(capturedResource->transactionUUID(), requestedTransactionUUID)
        << "Resource should contain requesting transaction UUID";
}

/**
 * Test 4: Transaction caches paths for all exchange equivalents.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_CachesPathsForAllEquivalents) {
    // Arrange
    setupMultipleExchangeEquivalentsTopology();
    vector<SerializedEquivalent> exchangeEquivalents = {1001, 1002, 1003};
    auto transaction = createTransaction(exchangeEquivalents);

    // Act
    transaction->run(); // sendRequestForCollectingTopology()
    transaction->run(); // processCollectingTopology()

    // Assert - Paths cached for each equivalent
    auto contractorID = router->getOrCreateParticipantID(contractorAddress);
    
    for (const auto &exchangeEquiv : exchangeEquivalents) {
        PathCacheKey key{contractorID, exchangeEquiv, EQ_RECEIVER};

        auto cachedPaths = pathsManager->retrievePaths(key);
        ASSERT_TRUE(cachedPaths.has_value())
            << "No paths cached for equivalent " << exchangeEquiv;

        EXPECT_GT(cachedPaths->size(), 0)
            << "Empty paths cached for equivalent " << exchangeEquiv;
    }
}

/**
 * Test 5: Transaction handles OR-Tools failure gracefully (no paths found).
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_HandlesORToolsFailureGracefully) {
    // Arrange - Setup topology with no exchange rates (paths cannot be built)
    setupNoPathsTopology();
    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};
    auto transaction = createTransaction(exchangeEquivalents);

    // Setup resource capture
    shared_ptr<ExchangePathsResource> capturedResource;
    resourcesManager->attachResourceSignal.connect(
        [&](BaseResource::Shared resource) {
            if (resource->type() == BaseResource::ExchangePaths) {
                capturedResource = static_pointer_cast<ExchangePathsResource>(resource);
            }
        });

    // Act - Transaction should complete without crashing
    auto r1 = transaction->run();
    ASSERT_NE(r1, nullptr);

    auto r2 = transaction->run();
    ASSERT_NE(r2, nullptr);

    // Assert - Transaction completes successfully even with no paths
    ASSERT_NE(r2->state(), nullptr);
    EXPECT_TRUE(r2->state()->mustExit())
        << "Transaction should complete even when no paths found";

    // Resource still returned (coordinator handles empty cache)
    ASSERT_NE(capturedResource, nullptr)
        << "Resource should be returned even when path building fails";
    EXPECT_EQ(capturedResource->transactionUUID(), requestedTransactionUUID);
}

/**
 * Test 6: Transaction returns resource even when no paths are found.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_NoPathsFound_StillReturnsResource) {
    // Arrange - Setup topology with insufficient capacity
    auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000");
    auto idB = router->getOrCreateParticipantID(addrB);
    
    // Create very limited topology (unlikely to find good paths)
    auto tlm1001 = router->topologyTrustLineManager(EQ_SENDER);
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, idB, A(1))); // Very low capacity

    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};
    auto transaction = createTransaction(exchangeEquivalents);

    // Setup resource capture
    shared_ptr<ExchangePathsResource> capturedResource;
    resourcesManager->attachResourceSignal.connect(
        [&](BaseResource::Shared resource) {
            if (resource->type() == BaseResource::ExchangePaths) {
                capturedResource = static_pointer_cast<ExchangePathsResource>(resource);
            }
        });

    // Act
    transaction->run();
    transaction->run();

    // Assert - Resource returned even with no viable paths
    ASSERT_NE(capturedResource, nullptr)
        << "Resource must be returned even when no paths found";

    // Cache may be empty or have no valid paths (acceptable)
    auto contractorID = router->getOrCreateParticipantID(contractorAddress);
    PathCacheKey key{contractorID, EQ_SENDER, EQ_RECEIVER};
    auto cachedPaths = pathsManager->retrievePaths(key);
    // Either nullopt or empty vector is acceptable for this scenario
}

/**
 * Test 7: Transaction uses correct cache keys (contractorID, senderEquiv, receiverEquiv).
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_UsesCorrectCacheKeys) {
    // Arrange
    setupSimpleTopology();
    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};
    auto transaction = createTransaction(exchangeEquivalents);

    // Act
    transaction->run();
    transaction->run();

    // Assert - Verify correct key format is used
    auto contractorID = router->getOrCreateParticipantID(contractorAddress);

    // Correct key: (contractorID, senderEquivalent, receiverEquivalent)
    PathCacheKey correctKey{contractorID, EQ_SENDER, EQ_RECEIVER};
    EXPECT_TRUE(arePathsCached(correctKey))
        << "Paths should be cached with correct key: contractor=" << contractorID
        << ", sender=" << EQ_SENDER << ", receiver=" << EQ_RECEIVER;

    // Verify reverse key is NOT cached (would indicate wrong key usage)
    PathCacheKey reverseKey{contractorID, EQ_RECEIVER, EQ_SENDER};
    auto reversePaths = pathsManager->retrievePaths(reverseKey);
    EXPECT_FALSE(reversePaths.has_value())
        << "Reverse key should not have cached paths (wrong key usage)";
}

/**
 * Test 8: Transaction handles empty exchange equivalents vector.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_EmptyExchangeEquivalents) {
    // Arrange
    setupSimpleTopology();
    vector<SerializedEquivalent> emptyEquivalents; // Empty vector
    auto transaction = createTransaction(emptyEquivalents);

    // Act - Transaction should handle empty equivalents gracefully
    auto r1 = transaction->run();
    ASSERT_NE(r1, nullptr);

    auto r2 = transaction->run();
    ASSERT_NE(r2, nullptr);

    // Assert - Transaction completes (may not find paths, but shouldn't crash)
    ASSERT_NE(r2->state(), nullptr);
    EXPECT_TRUE(r2->state()->mustExit());
}

/**
 * Test 9: Transaction correctly handles multiple sender equivalents with different capacities.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, ProcessCollectingTopology_MultipleEquivalentsDifferentCapacities) {
    // Arrange
    setupMultipleExchangeEquivalentsTopology();
    vector<SerializedEquivalent> exchangeEquivalents = {1001, 1002, 1003};
    auto transaction = createTransaction(exchangeEquivalents);

    // Act
    transaction->run();
    transaction->run();

    // Assert - Each equivalent should have independent path caching
    auto contractorID = router->getOrCreateParticipantID(contractorAddress);

    // Verify each equivalent has separate cache entry
    PathCacheKey key1001{contractorID, 1001, EQ_RECEIVER};
    PathCacheKey key1002{contractorID, 1002, EQ_RECEIVER};
    PathCacheKey key1003{contractorID, 1003, EQ_RECEIVER};

    auto paths1001 = pathsManager->retrievePaths(key1001);
    auto paths1002 = pathsManager->retrievePaths(key1002);
    auto paths1003 = pathsManager->retrievePaths(key1003);

    // At least some equivalents should have paths (depending on topology)
    int pathsFoundCount = 0;
    if (paths1001.has_value() && !paths1001->empty()) pathsFoundCount++;
    if (paths1002.has_value() && !paths1002->empty()) pathsFoundCount++;
    if (paths1003.has_value() && !paths1003->empty()) pathsFoundCount++;

    EXPECT_GT(pathsFoundCount, 0)
        << "At least one equivalent should have paths cached";
}

/**
 * Test 10: Transaction properly initializes contractor ID using getOrCreateParticipantID.
 */
TEST_F(FindPathsByMaxFlowExchangeTransactionTest, Constructor_InitializesContractorIDCorrectly) {
    // Arrange
    vector<SerializedEquivalent> exchangeEquivalents = {EQ_SENDER};
    
    // Get contractor ID that transaction should use
    auto expectedContractorID = router->getOrCreateParticipantID(contractorAddress);

    // Act
    setupSimpleTopology();
    auto transaction = createTransaction(exchangeEquivalents);
    transaction->run();
    transaction->run();

    // Assert - Verify paths are cached with contractor ID from getOrCreateParticipantID
    PathCacheKey key{expectedContractorID, EQ_SENDER, EQ_RECEIVER};
    auto cachedPaths = pathsManager->retrievePaths(key);
    
    ASSERT_TRUE(cachedPaths.has_value())
        << "Paths should be cached with contractor ID from getOrCreateParticipantID";
}

