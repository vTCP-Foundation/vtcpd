#ifndef VTCPD_BLOCKNUMBERCACHE_H
#define VTCPD_BLOCKNUMBERCACHE_H

#include "../../common/Types.h"
#include "../../common/time/TimeUtils.h"
#include "../../logger/Logger.h"

#include <string>

using namespace std;

/**
 * Caches latest block number and predicts future block height based on time.
 */
class BlockNumberCache
{
public:
    // Block generation cadence in seconds.
    static constexpr uint32_t kBlockGenerationSeconds = 60; // 1 minute
    // Cache time-to-live in seconds.
    static constexpr uint32_t kCacheTTLSeconds = 600; // 10 minutes

public:
    /**
     * Initializes cache in empty state and binds logger.
     */
    explicit BlockNumberCache(Logger &logger);

    /**
     * Returns true when cache is empty or the TTL has expired.
     */
    bool needsRefresh() const;

    /**
     * Returns cached block number with time-based prediction applied.
     */
    BlockNumber getCachedBlockNumber() const;

    /**
     * Updates cache with fresh block number and timestamp.
     */
    void update(BlockNumber blockNumber);

    /**
     * Resets cache to empty state.
     */
    void clear();

private:
    /**
     * Provides subsystem name for logging.
     */
    string logHeader() const;

    /**
     * Returns debug logger stream bound to subsystem.
     */
    LoggerStream debug() const;

    /**
     * Returns info logger stream bound to subsystem.
     */
    LoggerStream info() const;

private:
    // Last known block number from observer.
    BlockNumber mCachedBlockNumber;
    // Timestamp of the cached block number.
    DateTime mCacheTimestamp;
    // Indicates whether cache has valid data.
    bool mHasValue;
    // Logger reference for debug output.
    Logger &mLogger;
};

#endif // VTCPD_BLOCKNUMBERCACHE_H
