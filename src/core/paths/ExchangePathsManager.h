#ifndef VTCPD_EXCHANGEPATHSMANAGER_H
#define VTCPD_EXCHANGEPATHSMANAGER_H

#include "lib/OptimalPathResult.h"
#include "lib/ExchangePath.h"
#include "lib/EdgeKey.h"
#include "../logger/Logger.h"
#include "../common/Types.h"
#include "../common/time/TimeUtils.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>
#include <set>

using namespace std;
namespace as = boost::asio;

// Forward declarations
class EquivalentsSubsystemsRouter;
class ExchangeRatesManager;
class ContractorsManager;

// Unique identifier for cached path set
struct PathCacheKey {
    ContractorID contractor;
    SerializedEquivalent senderEquivalent;
    SerializedEquivalent receiverEquivalent;

    bool operator<(const PathCacheKey &other) const noexcept
    {
        if (contractor != other.contractor) return contractor < other.contractor;
        if (senderEquivalent != other.senderEquivalent)
            return senderEquivalent < other.senderEquivalent;
        return receiverEquivalent < other.receiverEquivalent;
    }
};

class ExchangePathsManager
{
public:
    struct MaxFlowResult {
        TrustLineAmount maxFlow;
        vector<OptimalPathResult> optimalPaths;
    };

public:
    ExchangePathsManager(
        as::io_context &ioContext,
        EquivalentsSubsystemsRouter *router,
        ExchangeRatesManager *ratesManager,
        ContractorsManager *contractorsManager,
        Logger &logger);

public:
    // Main calculation method for max flow with exchange paths
    MaxFlowResult calculateMaxFlow(
        ContractorID targetContractor,
        SerializedEquivalent receiverEquivalent,
        const vector<SerializedEquivalent> &senderEquivalents,
        ContractorID senderID,
        uint8_t hopsCount);

    // Cache management methods
    void storePaths(
        const PathCacheKey &key,
        const vector<OptimalPathResult> &paths);

    optional<vector<OptimalPathResult>> retrievePaths(
        const PathCacheKey &key);

    void invalidatePaths(
        const PathCacheKey &key);

    void invalidatePathsForContractor(
        ContractorID contractor);

    void invalidatePathsForEquivalent(
        SerializedEquivalent equivalent);

    // Path simulation algorithms for estimation
    double forwardSimulatePath(
        const ExchangePath &path,
        double inputAmount,
        set<pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
        map<EdgeKey, double> &edgeRemainingCapacity);

    double inverseSimulatePath(
        const ExchangePath &path,
        double targetOutputAmount,
        set<pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
        map<EdgeKey, double> &edgeRemainingCapacity);

private:
    // Path enumeration methods
    vector<ExchangePath> enumerateAllFeasiblePaths(
        ContractorID targetContractor,
        SerializedEquivalent receiverEquivalent,
        const vector<SerializedEquivalent> &senderEquivalents,
        ContractorID senderID,
        int maxPathLength = 10);

    void enumeratePathsFromEquivalent(
        SerializedEquivalent startEquivalent,
        ContractorID targetContractor,
        SerializedEquivalent targetEquivalent,
        ContractorID senderID,
        vector<ExchangePath> &allPaths,
        int maxPathLength = 10);

    void dfsEnumeratePaths(
        ContractorID currentNode,
        SerializedEquivalent currentEquivalent,
        ContractorID targetNode,
        SerializedEquivalent targetEquivalent,
        vector<ContractorID> &currentPath,
        vector<SerializedEquivalent> &currentEquivPath,
        vector<ExchangeStep> &currentExchanges,
        vector<ExchangePath> &results,
        int maxDepth,
        int currentDepth = 0);

    TrustLineAmount getSenderBalance(
        SerializedEquivalent equivalent,
        ContractorID senderID);

    // Helper methods for path calculation
    static double fetchEdgeCapacity(
        EquivalentsSubsystemsRouter *router,
        ContractorID from,
        ContractorID to,
        SerializedEquivalent equivalent);

    static double findExchangeRateForStep(
        const ExchangePath &path,
        size_t index);

    static bool commissionAppliedOnPath(
        const vector<pair<ContractorID, SerializedEquivalent>> &applied,
        ContractorID node,
        SerializedEquivalent equivalent);

    static double computeMaxRawAllowed(
        const ExchangePath &path,
        const vector<pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
        const map<EdgeKey, double> &edgeRemaining,
        EquivalentsSubsystemsRouter *router,
        double solverRaw);

    static set<ContractorID> collectExchangeNodes(
        const vector<ExchangePath> &paths);

    static string formatDetailedPathWithRouter(
        const OptimalPathResult &pathResult,
        EquivalentsSubsystemsRouter *router,
        const set<ContractorID> &exchangeNodes,
        set<pair<ContractorID, SerializedEquivalent>> *processedCommissions,
        map<EdgeKey, double> *edgeRemaining,
        double *deliveredOut);

    static double simulatePathNetAmount(
        const ExchangePath &path,
        double sourceFlow,
        EquivalentsSubsystemsRouter *router,
        const set<ContractorID> &exchangeNodes,
        set<pair<ContractorID, SerializedEquivalent>> *appliedOnce = nullptr,
        vector<pair<ContractorID, SerializedEquivalent>> *appliedNow = nullptr);

    // Cache management methods
    void scheduleExpiryTimer();

    void onExpiryTimer(
        const boost::system::error_code &error);

    DateTime earliestExpiryTime() const;

    void removeExpiredPaths();

    // Logging helpers
    LoggerStream info() const;
    LoggerStream debug() const;
    LoggerStream warning() const;
    LoggerStream error() const;

    const string logHeader() const;

public:
    static const uint32_t kPathResultsTTLSeconds = 600; // 10 minutes

private:
    struct CachedPathResult {
        vector<OptimalPathResult> paths;
        DateTime computedAt;
    };

    as::io_context &mIOContext;
    EquivalentsSubsystemsRouter *mRouter;
    ExchangeRatesManager *mRatesManager;
    ContractorsManager *mContractorsManager;
    Logger &mLogger;
    map<PathCacheKey, CachedPathResult> mCachedPaths;
    mutable mutex mCacheMutex;
    unique_ptr<as::steady_timer> mExpiryTimer;
};

#endif // VTCPD_EXCHANGEPATHSMANAGER_H
