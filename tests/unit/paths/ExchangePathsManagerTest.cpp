#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>

#include "core/paths/ExchangePathsManager.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/logger/Logger.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/crypto/keychain.h"
#include "core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "core/rates/Commission.h"

using namespace testing;
using namespace std;

namespace {
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    // Helper to create test environment for ExchangePathsManager tests
    struct ExchangePathsManagerTestEnv {
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

        ExchangePathsManagerTestEnv(const std::string& testName) {
            dbDir = "build-tests/testdb_exchpaths_" + testName;
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

        ~ExchangePathsManagerTestEnv() {
            std::filesystem::remove_all(dbDir);
        }

        // Setup simple exchange topology for testing path caching
        void setupSimpleExchangeTopology(
            ContractorID targetID,
            SerializedEquivalent senderEq,
            SerializedEquivalent receiverEq)
        {
            // Initialize equivalents
            router->initNewEquivalent(senderEq);
            router->initNewEquivalent(receiverEq);

            // Simple path: self(0) -> node1 -> target (in senderEq)
            //              exchange at node1: senderEq -> receiverEq (rate 1.0)
            //              node1 -> target (in receiverEq)
            auto tlmSender = router->topologyTrustLineManager(senderEq);
            auto tlmReceiver = router->topologyTrustLineManager(receiverEq);

            ContractorID node1 = 1;
            
            // Sender equivalent path: 0 -> 1 -> target
            tlmSender->addTrustLine(make_shared<TopologyTrustLine>(0, node1, A(100)));
            tlmSender->addTrustLine(make_shared<TopologyTrustLine>(node1, targetID, A(100)));

            // Receiver equivalent path: 1 -> target
            tlmReceiver->addTrustLine(make_shared<TopologyTrustLine>(node1, targetID, A(100)));

            // Add exchange rate at node1: senderEq -> receiverEq, rate = 1.0
            auto expiresAt = utc_now() + boost::posix_time::seconds(300);
            ExchangeRate rate(senderEq, receiverEq, TrustLineAmount(1), 0, expiresAt,
                            TrustLineAmount(0), TrustLineAmount(0));
            ratesManager->addOrUpdateExternal(node1, rate);
        }

        // Cache paths for given key
        void cachePaths(const PathCacheKey &key) {
            // Use calculateMaxFlow to build paths, then store them
            auto result = pathsManager->calculateMaxFlow(
                key.contractor,           // target contractor
                key.receiverEquivalent,   // receiver equivalent
                {key.senderEquivalent},   // sender equivalents
                0,                        // sender ID (self)
                5);                       // hopsCount
            
            // Store the calculated paths in cache
            if (!result.optimalPaths.empty()) {
                pathsManager->storePaths(key, result.optimalPaths);
            }
        }
    };
}

// Test 1: Default behavior preserved (no customTTL)
TEST(ExchangePathsManagerTest, RetrievePathsWithoutCustomTTL_UsesDefaultTTL)
{
    ExchangePathsManagerTestEnv env("default_ttl");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths using calculateMaxFlow
    env.cachePaths(key);

    // Act - call without customTTL parameter (should use default 600s)
    auto result = env.pathsManager->retrievePaths(key);

    // Assert - paths should be returned (age=0, well under default 600s TTL)
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

// Test 2: Custom TTL used when provided
TEST(ExchangePathsManagerTest, RetrievePathsWithCustomTTL_UsesProvidedTTL)
{
    ExchangePathsManagerTestEnv env("custom_ttl");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    // Act - call with custom TTL of 150 seconds
    auto result = env.pathsManager->retrievePaths(key, 150);

    // Assert - paths should be returned (age is 0, well under 150s)
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

// Test 3: TTL boundary semantics (age >= TTL means expired)
// Use short TTL (2 seconds) for fast test execution
TEST(ExchangePathsManagerTest, RetrievePathsAtExactTTLBoundary_ReturnsNullopt)
{
    ExchangePathsManagerTestEnv env("ttl_boundary");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    // Sleep for exactly 2 seconds to test boundary
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Act - retrieve with TTL=2 seconds
    auto result = env.pathsManager->retrievePaths(key, 2);

    // Assert - paths should be expired (age >= 2, TTL = 2)
    EXPECT_FALSE(result.has_value());
}

// Test 4: Just under TTL is valid (age < TTL)
// Use short TTL (3 seconds) with 1 second wait for fast test
TEST(ExchangePathsManagerTest, RetrievePathsJustUnderTTL_ReturnsPaths)
{
    ExchangePathsManagerTestEnv env("under_ttl");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    // Sleep for 1 second (just under 3s TTL)
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Act - retrieve with TTL=3 seconds
    auto result = env.pathsManager->retrievePaths(key, 3);

    // Assert - paths should still be valid (age = 1 < 3)
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

// Test 5: Missing paths return nullopt
TEST(ExchangePathsManagerTest, RetrievePathsForMissingKey_ReturnsNullopt)
{
    ExchangePathsManagerTestEnv env("missing_key");

    PathCacheKey key{123, 1001, 2002};
    // Don't cache any paths

    // Act - retrieve with custom TTL
    auto result = env.pathsManager->retrievePaths(key, 150);

    // Assert - should return nullopt (key not found)
    EXPECT_FALSE(result.has_value());

    // Also test with default TTL
    auto result2 = env.pathsManager->retrievePaths(key);
    EXPECT_FALSE(result2.has_value());
}

// Test 6: Thread safety with customTTL
TEST(ExchangePathsManagerTest, RetrievePathsConcurrently_ThreadSafe)
{
    ExchangePathsManagerTestEnv env("thread_safe");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    const int numThreads = 10;
    const int retrievalsPerThread = 100;
    std::atomic<int> successCount{0};

    // Act - concurrent retrievals with different TTL values
    vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            uint32_t customTTL = 150 + (i % 3) * 50; // Use 150, 200, or 250 seconds

            for (int j = 0; j < retrievalsPerThread; ++j) {
                auto result = env.pathsManager->retrievePaths(key, customTTL);
                if (result.has_value()) {
                    successCount++;
                }
            }
        });
    }

    // Wait for all threads
    for (auto &thread : threads) {
        thread.join();
    }

    // Assert - all retrievals should succeed (paths are fresh)
    EXPECT_EQ(successCount, numThreads * retrievalsPerThread);
}

// Test 7: Zero TTL always expires
TEST(ExchangePathsManagerTest, RetrievePathsWithZeroTTL_AlwaysExpired)
{
    ExchangePathsManagerTestEnv env("zero_ttl");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    // Small delay to ensure age > 0
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Act - retrieve with TTL=0 (immediate expiry)
    auto result = env.pathsManager->retrievePaths(key, 0);

    // Assert - should be expired (any age >= 0)
    EXPECT_FALSE(result.has_value());
}

// Test 8: Very large TTL always valid
TEST(ExchangePathsManagerTest, RetrievePathsWithVeryLargeTTL_AlwaysValid)
{
    ExchangePathsManagerTestEnv env("large_ttl");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    // Wait a short time
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Act - retrieve with very large TTL (1 year = 31536000 seconds)
    auto result = env.pathsManager->retrievePaths(key, 365 * 24 * 60 * 60);

    // Assert - should still be valid
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

// Test 9: Verify paths expired immediately after TTL boundary
// Uses 2 second TTL with 3 second wait
TEST(ExchangePathsManagerTest, RetrievePathsAfterTTLExpiry_ReturnsNullopt)
{
    ExchangePathsManagerTestEnv env("after_expiry");

    const SerializedEquivalent senderEq = 1001;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    env.setupSimpleExchangeTopology(targetID, senderEq, receiverEq);

    PathCacheKey key{targetID, senderEq, receiverEq};

    // Cache paths
    env.cachePaths(key);

    // Wait 3 seconds (well past 2s TTL)
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Act - retrieve with TTL=2 seconds
    auto result = env.pathsManager->retrievePaths(key, 2);

    // Assert - paths should be expired (age = 3 > 2)
    EXPECT_FALSE(result.has_value());
}

// Test 10: Multiple keys with different TTLs
TEST(ExchangePathsManagerTest, MultipleKeysWithDifferentTTLs_IndependentExpiry)
{
    ExchangePathsManagerTestEnv env("multi_key");

    const SerializedEquivalent senderEq1 = 1001;
    const SerializedEquivalent senderEq2 = 1002;
    const SerializedEquivalent receiverEq = 2002;
    const ContractorID targetID = 10;

    // Setup topology for two different sender equivalents
    env.setupSimpleExchangeTopology(targetID, senderEq1, receiverEq);
    
    // Setup second sender equivalent (reuse some infrastructure)
    env.router->initNewEquivalent(senderEq2);
    auto tlmSender2 = env.router->topologyTrustLineManager(senderEq2);
    ContractorID node1 = 1;
    tlmSender2->addTrustLine(make_shared<TopologyTrustLine>(0, node1, A(100)));
    tlmSender2->addTrustLine(make_shared<TopologyTrustLine>(node1, targetID, A(100)));
    
    // Add exchange rate for second sender equivalent
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate2(senderEq2, receiverEq, TrustLineAmount(1), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    env.ratesManager->addOrUpdateExternal(node1, rate2);

    PathCacheKey key1{targetID, senderEq1, receiverEq};
    PathCacheKey key2{targetID, senderEq2, receiverEq};

    // Cache paths for both keys
    env.cachePaths(key1);
    env.cachePaths(key2);

    // Wait 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Act - retrieve key1 with TTL=3 (valid), key2 with TTL=2 (expired)
    auto result1 = env.pathsManager->retrievePaths(key1, 3);
    auto result2 = env.pathsManager->retrievePaths(key2, 2);

    // Assert - key1 valid (age=2 < 3), key2 expired (age=2 >= 2)
    EXPECT_TRUE(result1.has_value());
    EXPECT_FALSE(result2.has_value());
}
