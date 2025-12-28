#include "BlockNumberCache.h"

BlockNumberCache::BlockNumberCache(
    Logger &logger) :
    mCachedBlockNumber(0),
    mCacheTimestamp(),
    mHasValue(false),
    mLogger(logger)
{}

bool BlockNumberCache::needsRefresh() const
{
    // Cache is invalid until the first update.
    if (!mHasValue) {
        return true;
    }

    // Refresh when the cache TTL expires.
    const auto now = utc_now();
    const auto expirationTime = mCacheTimestamp + pt::seconds(kCacheTTLSeconds);
    return now > expirationTime;
}

BlockNumber BlockNumberCache::getCachedBlockNumber() const
{
    // Compute elapsed seconds since the cached value was captured.
    const auto now = utc_now();
    const auto elapsed = now - mCacheTimestamp;
    auto elapsedSeconds = elapsed.total_seconds();

    if (elapsedSeconds < 0) {
        // Guard against system time moving backwards.
        debug() << "Cache timestamp is in the future; clamping elapsed time to 0 seconds";
        elapsedSeconds = 0;
    }

    // Predict blocks produced since the cached timestamp.
    const auto predictedBlocks = static_cast<BlockNumber>(
        elapsedSeconds / static_cast<int64_t>(kBlockGenerationSeconds));
    return mCachedBlockNumber + predictedBlocks;
}

void BlockNumberCache::update(
    BlockNumber blockNumber)
{
    // Store latest block number and its retrieval time.
    mCachedBlockNumber = blockNumber;
    mCacheTimestamp = utc_now();
    mHasValue = true;

    debug() << "Block number cache updated to " << blockNumber;
}

void BlockNumberCache::clear()
{
    // Reset cache state for testing or forced refresh.
    mCachedBlockNumber = 0;
    mCacheTimestamp = DateTime();
    mHasValue = false;

    debug() << "Block number cache cleared";
}

string BlockNumberCache::logHeader() const
{
    return "BlockNumberCache";
}

LoggerStream BlockNumberCache::debug() const
{
    return mLogger.debug(logHeader());
}

LoggerStream BlockNumberCache::info() const
{
    return mLogger.info(logHeader());
}
