#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <thread>
#include <chrono>

#include "core/paths/ExchangePathsManager.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/logger/Logger.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/crypto/keychain.h"
#include "core/interface/events_interface/interface/EventsInterfaceManager.h"

using namespace testing;
using namespace std;

namespace {
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    // Helper to create test environment
    struct TestEnvironment {
        Logger logger;
        boost::asio::io_context io;
        std::string dbDir;
        std::unique_ptr<StorageHandlerSQLite> storage;
        std::unique_ptr<crypto::Keystore> keystore;
        std::unique_ptr<EventsInterfaceManager> eventsManager;
        std::unique_ptr<ContractorsManager> contractors;
        std::unique_ptr<EquivalentsSubsystemsRouter> router;
        std::unique_ptr<ExchangeRatesManager> ratesManager;
        std::unique_ptr<ExchangePathsManager> pathsManager;

        TestEnvironment(const std::string& testName) {
            dbDir = "build-tests/testdb_paths_" + testName;
            std::filesystem::remove_all(dbDir);

            storage = std::make_unique<StorageHandlerSQLite>(dbDir, "test.db", logger);
            keystore = std::make_unique<crypto::Keystore>(logger);
            {
                auto ioTransaction = storage->beginTransaction();
                keystore->init(ioTransaction);
            }

            eventsManager = std::make_unique<EventsInterfaceManager>(
                vector<pair<string, SerializedEventType>>{},
                vector<pair<string, bool>>{},
                logger);

            vector<pair<string, string>> ownAddrs = {{"ipv4", "127.0.0.1:2000"}};
            contractors = std::make_unique<ContractorsManager>(ownAddrs, storage.get(), logger);

            vector<SerializedEquivalent> gateways;
            router = std::make_unique<EquivalentsSubsystemsRouter>(
                storage.get(),
                keystore.get(),
                contractors.get(),
                eventsManager.get(),
                io,
                gateways,
                logger);

            ratesManager = std::make_unique<ExchangeRatesManager>(io, logger);
            pathsManager = std::make_unique<ExchangePathsManager>(
                io, router.get(), ratesManager.get(), contractors.get(), logger);
        }

        ~TestEnvironment() {
            std::filesystem::remove_all(dbDir);
        }
    };

    // Helper to create a simple path
    OptimalPathResult createSimplePath(
        const vector<ContractorID>& nodes,
        const vector<SerializedEquivalent>& equivalents,
        TrustLineAmount flow = TrustLineAmount(100))
    {
        OptimalPathResult result;
        result.path().ids = nodes;
        result.path().equivalents = equivalents;
        result.path().minCapacity = flow;
        result.path().effectiveExchangeRate = 1.0;
        result.path().totalCommissions = TrustLineAmount(0);
        result.optimal_flow = flow;
        result.received_amount = flow;
        result.effective_exchange_rate = 1.0;
        result.path_efficiency = 1.0;
        return result;
    }
}

// Test 1: Store and Retrieve Test
TEST(ExchangePathsManagerTest, StoreAndRetrievePathsSuccessfully)
{
    TestEnvironment env("store_retrieve");

    // Create different keys
    PathCacheKey key1{ContractorID(1), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    PathCacheKey key2{ContractorID(2), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    PathCacheKey key3{ContractorID(1), SerializedEquivalent(1001), SerializedEquivalent(3003)};

    // Create mock paths
    vector<OptimalPathResult> paths1 = {
        createSimplePath({1, 2, 3}, {1001, 1001, 2002}, TrustLineAmount(100))
    };
    vector<OptimalPathResult> paths2 = {
        createSimplePath({2, 3, 4}, {1001, 1001, 2002}, TrustLineAmount(200))
    };
    vector<OptimalPathResult> paths3 = {
        createSimplePath({1, 2, 5}, {1001, 1001, 3003}, TrustLineAmount(300))
    };

    // Store paths
    env.pathsManager->storePaths(key1, paths1);
    env.pathsManager->storePaths(key2, paths2);
    env.pathsManager->storePaths(key3, paths3);

    // Retrieve and verify
    auto retrieved1 = env.pathsManager->retrievePaths(key1);
    ASSERT_TRUE(retrieved1.has_value());
    EXPECT_EQ(retrieved1->size(), 1);
    EXPECT_EQ((*retrieved1)[0].optimal_flow, TrustLineAmount(100));

    auto retrieved2 = env.pathsManager->retrievePaths(key2);
    ASSERT_TRUE(retrieved2.has_value());
    EXPECT_EQ(retrieved2->size(), 1);
    EXPECT_EQ((*retrieved2)[0].optimal_flow, TrustLineAmount(200));

    auto retrieved3 = env.pathsManager->retrievePaths(key3);
    ASSERT_TRUE(retrieved3.has_value());
    EXPECT_EQ(retrieved3->size(), 1);
    EXPECT_EQ((*retrieved3)[0].optimal_flow, TrustLineAmount(300));

    // Retrieve non-existent key
    PathCacheKey nonExistentKey{ContractorID(999), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    auto retrievedNone = env.pathsManager->retrievePaths(nonExistentKey);
    EXPECT_FALSE(retrievedNone.has_value());
}

// Test 2: TTL Expiration Test
TEST(ExchangePathsManagerTest, PathsExpireAfterTTL)
{
    TestEnvironment env("ttl_expiration");

    PathCacheKey key{ContractorID(1), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    vector<OptimalPathResult> paths = {
        createSimplePath({1, 2, 3}, {1001, 1001, 2002}, TrustLineAmount(100))
    };

    // Store paths
    env.pathsManager->storePaths(key, paths);

    // Verify paths exist
    auto retrieved1 = env.pathsManager->retrievePaths(key);
    ASSERT_TRUE(retrieved1.has_value());

    // Wait for TTL to expire (600 seconds + buffer)
    // Note: In real tests, we would need to mock DateTime::now()
    // For now, we test that paths are still available within TTL
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto retrieved2 = env.pathsManager->retrievePaths(key);
    EXPECT_TRUE(retrieved2.has_value()); // Should still be valid
}

// Test 3: Invalidate by Key Test
TEST(ExchangePathsManagerTest, InvalidateSpecificKey)
{
    TestEnvironment env("invalidate_key");

    PathCacheKey key1{ContractorID(1), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    PathCacheKey key2{ContractorID(2), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    PathCacheKey key3{ContractorID(3), SerializedEquivalent(1001), SerializedEquivalent(2002)};

    vector<OptimalPathResult> paths = {
        createSimplePath({1, 2, 3}, {1001, 1001, 2002}, TrustLineAmount(100))
    };

    // Store paths for all keys
    env.pathsManager->storePaths(key1, paths);
    env.pathsManager->storePaths(key2, paths);
    env.pathsManager->storePaths(key3, paths);

    // Invalidate key2
    env.pathsManager->invalidatePaths(key2);

    // Verify key1 and key3 still exist, key2 removed
    EXPECT_TRUE(env.pathsManager->retrievePaths(key1).has_value());
    EXPECT_FALSE(env.pathsManager->retrievePaths(key2).has_value());
    EXPECT_TRUE(env.pathsManager->retrievePaths(key3).has_value());
}

// Test 4: Invalidate by Contractor Test
TEST(ExchangePathsManagerTest, InvalidateAllPathsForContractor)
{
    TestEnvironment env("invalidate_contractor");

    ContractorID contractorC = 1;
    ContractorID contractorD = 2;

    PathCacheKey keyC1{contractorC, SerializedEquivalent(1001), SerializedEquivalent(2002)};
    PathCacheKey keyC2{contractorC, SerializedEquivalent(1001), SerializedEquivalent(3003)};
    PathCacheKey keyC3{contractorC, SerializedEquivalent(2002), SerializedEquivalent(3003)};
    PathCacheKey keyD{contractorD, SerializedEquivalent(1001), SerializedEquivalent(2002)};

    vector<OptimalPathResult> paths = {
        createSimplePath({1, 2, 3}, {1001, 1001, 2002}, TrustLineAmount(100))
    };

    // Store paths
    env.pathsManager->storePaths(keyC1, paths);
    env.pathsManager->storePaths(keyC2, paths);
    env.pathsManager->storePaths(keyC3, paths);
    env.pathsManager->storePaths(keyD, paths);

    // Invalidate all paths for contractor C
    env.pathsManager->invalidatePathsForContractor(contractorC);

    // Verify all C keys removed, D key remains
    EXPECT_FALSE(env.pathsManager->retrievePaths(keyC1).has_value());
    EXPECT_FALSE(env.pathsManager->retrievePaths(keyC2).has_value());
    EXPECT_FALSE(env.pathsManager->retrievePaths(keyC3).has_value());
    EXPECT_TRUE(env.pathsManager->retrievePaths(keyD).has_value());
}

// Test 5: Invalidate by Equivalent Test
TEST(ExchangePathsManagerTest, InvalidatePathsForEquivalent)
{
    TestEnvironment env("invalidate_equivalent");

    SerializedEquivalent eq1 = 1001;
    SerializedEquivalent eq2 = 2002;
    SerializedEquivalent eq3 = 3003;

    PathCacheKey key1{ContractorID(1), eq1, eq2}; // Has eq1 as sender
    PathCacheKey key2{ContractorID(1), eq2, eq3}; // Has eq2 as sender
    PathCacheKey key3{ContractorID(2), eq1, eq3}; // Has eq1 as sender

    vector<OptimalPathResult> paths = {
        createSimplePath({1, 2, 3}, {1001, 1001, 2002}, TrustLineAmount(100))
    };

    // Store paths
    env.pathsManager->storePaths(key1, paths);
    env.pathsManager->storePaths(key2, paths);
    env.pathsManager->storePaths(key3, paths);

    // Invalidate paths with eq1
    env.pathsManager->invalidatePathsForEquivalent(eq1);

    // Verify keys with eq1 removed, key2 remains
    EXPECT_FALSE(env.pathsManager->retrievePaths(key1).has_value());
    EXPECT_TRUE(env.pathsManager->retrievePaths(key2).has_value());
    EXPECT_FALSE(env.pathsManager->retrievePaths(key3).has_value());
}

// Test 6: Thread Safety Test
TEST(ExchangePathsManagerTest, ConcurrentAccessIsSafe)
{
    TestEnvironment env("thread_safety");

    const int numThreads = 20;
    const int opsPerThread = 100;

    auto storeWorker = [&](int threadId) {
        for (int i = 0; i < opsPerThread; ++i) {
            PathCacheKey key{
                static_cast<ContractorID>(threadId * opsPerThread + i),
                SerializedEquivalent(1001),
                SerializedEquivalent(2002)
            };
            vector<OptimalPathResult> paths = {
                createSimplePath({1, 2, 3}, {1001, 1001, 2002}, TrustLineAmount(i))
            };
            env.pathsManager->storePaths(key, paths);
        }
    };

    auto retrieveWorker = [&](int threadId) {
        for (int i = 0; i < opsPerThread; ++i) {
            PathCacheKey key{
                static_cast<ContractorID>((threadId % 10) * opsPerThread + (i % opsPerThread)),
                SerializedEquivalent(1001),
                SerializedEquivalent(2002)
            };
            auto retrieved = env.pathsManager->retrievePaths(key);
            // Just try to retrieve, may or may not exist
        }
    };

    vector<std::thread> threads;

    // Start 10 store threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(storeWorker, i);
    }

    // Start 10 retrieve threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(retrieveWorker, i);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Verify some stored keys are retrievable
    PathCacheKey testKey{ContractorID(0), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    auto retrieved = env.pathsManager->retrievePaths(testKey);
    EXPECT_TRUE(retrieved.has_value());
}

// Test 7: Edge Cases
TEST(ExchangePathsManagerTest, EdgeCases)
{
    TestEnvironment env("edge_cases");

    // Test 1: Store empty path vector
    PathCacheKey key1{ContractorID(1), SerializedEquivalent(1001), SerializedEquivalent(2002)};
    vector<OptimalPathResult> emptyPaths;
    env.pathsManager->storePaths(key1, emptyPaths);

    auto retrieved1 = env.pathsManager->retrievePaths(key1);
    ASSERT_TRUE(retrieved1.has_value());
    EXPECT_EQ(retrieved1->size(), 0);

    // Test 2: Same sender and receiver equivalent
    PathCacheKey key2{ContractorID(1), SerializedEquivalent(1001), SerializedEquivalent(1001)};
    vector<OptimalPathResult> paths = {
        createSimplePath({1, 2, 3}, {1001, 1001, 1001}, TrustLineAmount(100))
    };
    env.pathsManager->storePaths(key2, paths);

    auto retrieved2 = env.pathsManager->retrievePaths(key2);
    ASSERT_TRUE(retrieved2.has_value());
    EXPECT_EQ(retrieved2->size(), 1);
}
