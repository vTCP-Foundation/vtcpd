#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "core/network/rpc/BlockNumberCache.h"
#include "core/logger/Logger.h"

class BlockNumberCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = std::make_unique<Logger>();
        cache = std::make_unique<BlockNumberCache>(*logger);
    }

    void TearDown() override {
        cache.reset();
        logger.reset();
    }

    std::unique_ptr<Logger> logger;
    std::unique_ptr<BlockNumberCache> cache;
};

// T1: testInitialStateHasNoCache
TEST_F(BlockNumberCacheTest, testInitialStateHasNoCache) {
    EXPECT_TRUE(cache->needsRefresh());
}

// T2: testUpdateStoresBlockNumberAndTimestamp
TEST_F(BlockNumberCacheTest, testUpdateStoresBlockNumberAndTimestamp) {
    BlockNumber blockNumber = 12345;
    cache->update(blockNumber);

    EXPECT_FALSE(cache->needsRefresh());
    EXPECT_EQ(cache->getCachedBlockNumber(), blockNumber);
}

// T3: testClearResetsCache
TEST_F(BlockNumberCacheTest, testClearResetsCache) {
    cache->update(12345);
    EXPECT_FALSE(cache->needsRefresh());

    cache->clear();
    EXPECT_TRUE(cache->needsRefresh());
}

// T4: testNeedsRefreshReturnsTrueWhenCacheEmpty
TEST_F(BlockNumberCacheTest, testNeedsRefreshReturnsTrueWhenCacheEmpty) {
    // Fresh cache without any update
    EXPECT_TRUE(cache->needsRefresh());

    // After clear
    cache->update(100);
    cache->clear();
    EXPECT_TRUE(cache->needsRefresh());
}

// T5: testNeedsRefreshReturnsFalseWithinTTL
TEST_F(BlockNumberCacheTest, testNeedsRefreshReturnsFalseWithinTTL) {
    cache->update(12345);

    // Immediately after update
    EXPECT_FALSE(cache->needsRefresh());

    // After small delay (still within TTL)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(cache->needsRefresh());
}

// T6: testGetCachedBlockNumberReturnsExactValueWhenJustUpdated
TEST_F(BlockNumberCacheTest, testGetCachedBlockNumberReturnsExactValueWhenJustUpdated) {
    BlockNumber blockNumber = 99999;
    cache->update(blockNumber);

    // Immediately after update, should return exact value
    BlockNumber result = cache->getCachedBlockNumber();
    EXPECT_EQ(result, blockNumber);
}

// T7: testGetCachedBlockNumberNeverDecreasesOnPrediction
TEST_F(BlockNumberCacheTest, testGetCachedBlockNumberNeverDecreasesOnPrediction) {
    BlockNumber initialBlock = 5000;
    cache->update(initialBlock);

    BlockNumber first = cache->getCachedBlockNumber();
    EXPECT_GE(first, initialBlock);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    BlockNumber second = cache->getCachedBlockNumber();
    EXPECT_GE(second, first);
}

// T8: testConstantsHaveExpectedValues
TEST_F(BlockNumberCacheTest, testConstantsHaveExpectedValues) {
    EXPECT_EQ(BlockNumberCache::kBlockGenerationSeconds, 60);
    EXPECT_EQ(BlockNumberCache::kCacheTTLSeconds, 600);
}

