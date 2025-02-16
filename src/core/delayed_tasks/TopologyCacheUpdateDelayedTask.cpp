#include "TopologyCacheUpdateDelayedTask.h"

TopologyCacheUpdateDelayedTask::TopologyCacheUpdateDelayedTask(
    const SerializedEquivalent equivalent,
    as::io_context &ioCtx,
    TopologyCacheManager *topologyCacheManager,
    TopologyTrustLinesManager *topologyTrustLineManager,
    MaxFlowCacheManager *maxFlowCalculationNodeCacheManager,
    Logger &logger):

    mEquivalent(equivalent),
    mIOCtx(ioCtx),
    mTopologyCacheManager(topologyCacheManager),
    mTopologyTrustLineManager(topologyTrustLineManager),
    mMaxFlowCalculationNodeCacheManager(maxFlowCalculationNodeCacheManager),
    mLog(logger)
{
    mTopologyCacheUpdateTimer = make_unique<as::steady_timer>(
                                    mIOCtx);

    Duration microsecondsDelay = minimalAwakeningTimestamp() - utc_now();
    mTopologyCacheUpdateTimer->expires_after(
        chrono::milliseconds(
            microsecondsDelay.total_milliseconds()));
    mTopologyCacheUpdateTimer->async_wait(boost::bind(
            &TopologyCacheUpdateDelayedTask::runSignalTopologyCacheUpdate,
            this,
            as::placeholders::error));
}

void TopologyCacheUpdateDelayedTask::runSignalTopologyCacheUpdate(
    const boost::system::error_code &error)
{
    if (error != boost::system::errc::success) {
        return;
    }

    DateTime nextUpdateTime = updateCache();
    Duration microsecondsDelay = nextUpdateTime - utc_now();
    mTopologyCacheUpdateTimer->expires_after(
        chrono::milliseconds(
            microsecondsDelay.total_milliseconds()));
    mTopologyCacheUpdateTimer->async_wait(boost::bind(
            &TopologyCacheUpdateDelayedTask::runSignalTopologyCacheUpdate,
            this,
            as::placeholders::error));
    mTopologyTrustLineManager->setPreventDeleting(false);
}

DateTime TopologyCacheUpdateDelayedTask::minimalAwakeningTimestamp()
{
    DateTime closestCacheManagerTimeEvent = mTopologyCacheManager->closestTimeEvent();
    DateTime closestTrustLineManagerTimeEvent = mTopologyTrustLineManager->closestTimeEvent();
    DateTime closestNodeCacheManagerTimeEvent = mMaxFlowCalculationNodeCacheManager->closestTimeEvent();
    DateTime result = closestCacheManagerTimeEvent;
    if (closestTrustLineManagerTimeEvent < result) {
        result = closestTrustLineManagerTimeEvent;
    }
    if (closestNodeCacheManagerTimeEvent < result) {
        result = closestNodeCacheManagerTimeEvent;
    }
#ifdef  DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "TopologyCacheUpdateDelayedTask::minimalAwakeningTimestamp " << result;
#endif
    return result;
}

DateTime TopologyCacheUpdateDelayedTask::updateCache()
{
    mTopologyCacheManager->updateCaches();
    DateTime result = mTopologyCacheManager->closestTimeEvent();
    mMaxFlowCalculationNodeCacheManager->updateCaches();
    DateTime closestNodeCacheManagerTimeEvent = mMaxFlowCalculationNodeCacheManager->closestTimeEvent();
    if (closestNodeCacheManagerTimeEvent < result) {
        result = closestNodeCacheManagerTimeEvent;
    }
    // if MaxFlowCalculation transaction not finished it is forbidden to delete trustlines
    // and should increase trustlines caches time
    DateTime closestTrustLineManagerTimeEvent;
    if (mTopologyTrustLineManager->preventDeleting()) {
        closestTrustLineManagerTimeEvent = utc_now() + kProlongationTrustLineUpdatingDuration();
    } else {
        // if at least one trustline was deleted
        if (mTopologyTrustLineManager->deleteLegacyTrustLines()) {
            mTopologyCacheManager->resetInitiatorCache();
        }
        closestTrustLineManagerTimeEvent = mTopologyTrustLineManager->closestTimeEvent();
    }
    if (closestTrustLineManagerTimeEvent < result) {
        result = closestTrustLineManagerTimeEvent;
    }
    return result;
}

LoggerStream TopologyCacheUpdateDelayedTask::debug() const
{
    return mLog.debug(logHeader());
}

LoggerStream TopologyCacheUpdateDelayedTask::info() const
{
    return mLog.info(logHeader());
}

LoggerStream TopologyCacheUpdateDelayedTask::warning() const
{
    return mLog.warning(logHeader());
}

const string TopologyCacheUpdateDelayedTask::logHeader() const
{
    stringstream s;
    s << "TopologyCacheUpdateDelayedTask: " << mEquivalent << " ";
    return s.str();
}
