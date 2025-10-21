#include "ExchangePathsManager.h"
#include "../equivalents/EquivalentsSubsystemsRouter.h"
#include "../rates/manager/ExchangeRatesManager.h"
#include "../contractors/ContractorsManager.h"

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/base/version.h"
using operations_research::MPSolver;
using operations_research::MPVariable;
using operations_research::MPConstraint;
using operations_research::MPObjective;

#include <cmath>
#include <algorithm>
#include <limits>
#include <sstream>

const uint32_t ExchangePathsManager::kPathResultsTTLSeconds;

ExchangePathsManager::ExchangePathsManager(
    as::io_context &ioContext,
    EquivalentsSubsystemsRouter *router,
    ExchangeRatesManager *ratesManager,
    ContractorsManager *contractorsManager,
    Logger &logger):

    mIOContext(ioContext),
    mRouter(router),
    mRatesManager(ratesManager),
    mContractorsManager(contractorsManager),
    mLogger(logger)
{
    mExpiryTimer = make_unique<as::steady_timer>(mIOContext);
}

void ExchangePathsManager::storePaths(
    const PathCacheKey &key,
    const vector<OptimalPathResult> &paths)
{
    lock_guard<mutex> lock(mCacheMutex);

    CachedPathResult cachedResult;
    cachedResult.paths = paths;
    cachedResult.computedAt = utc_now();

    mCachedPaths[key] = cachedResult;

    debug() << "Stored " << paths.size() << " paths for contractor " << key.contractor
            << " (sender_eq=" << key.senderEquivalent
            << ", receiver_eq=" << key.receiverEquivalent << ")";

    scheduleExpiryTimer();
}

optional<vector<OptimalPathResult>> ExchangePathsManager::retrievePaths(
    const PathCacheKey &key,
    optional<uint32_t> customTTL)
{
    lock_guard<mutex> lock(mCacheMutex);

    auto it = mCachedPaths.find(key);
    if (it == mCachedPaths.end()) {
        return nullopt;
    }

    const auto ttlSeconds = customTTL.value_or(kPathResultsTTLSeconds);
    auto now = utc_now();
    const auto age = now - it->second.computedAt;

    if (age >= boost::posix_time::seconds(ttlSeconds)) {
        debug() << "Cached paths for contractor " << key.contractor
                << " expired (age=" << age.total_seconds() << "s, TTL="
                << ttlSeconds << "s), removing";
        mCachedPaths.erase(it);
        return nullopt;
    }

    debug() << "Retrieved " << it->second.paths.size() << " cached paths for contractor "
            << key.contractor << " (TTL=" << ttlSeconds << "s)";

    return it->second.paths;
}

void ExchangePathsManager::invalidatePaths(
    const PathCacheKey &key)
{
    lock_guard<mutex> lock(mCacheMutex);

    auto it = mCachedPaths.find(key);
    if (it != mCachedPaths.end()) {
        debug() << "Invalidating paths for contractor " << key.contractor
                << " (sender_eq=" << key.senderEquivalent
                << ", receiver_eq=" << key.receiverEquivalent << ")";
        mCachedPaths.erase(it);
    }
}

void ExchangePathsManager::invalidatePathsForContractor(
    ContractorID contractor)
{
    lock_guard<mutex> lock(mCacheMutex);

    auto it = mCachedPaths.begin();
    size_t removedCount = 0;

    while (it != mCachedPaths.end()) {
        if (it->first.contractor == contractor) {
            it = mCachedPaths.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    if (removedCount > 0) {
        debug() << "Invalidated " << removedCount << " path sets for contractor "
                << contractor;
    }
}

void ExchangePathsManager::invalidatePathsForEquivalent(
    SerializedEquivalent equivalent)
{
    lock_guard<mutex> lock(mCacheMutex);

    auto it = mCachedPaths.begin();
    size_t removedCount = 0;

    while (it != mCachedPaths.end()) {
        if (it->first.senderEquivalent == equivalent ||
            it->first.receiverEquivalent == equivalent) {
            it = mCachedPaths.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    if (removedCount > 0) {
        debug() << "Invalidated " << removedCount << " path sets involving equivalent "
                << equivalent;
    }
}

void ExchangePathsManager::scheduleExpiryTimer()
{
    if (mCachedPaths.empty()) {
        return;
    }

    DateTime earliestExpiry = earliestExpiryTime();
    auto now = utc_now();

    if (earliestExpiry <= now) {
        // Some paths have already expired, remove them immediately
        removeExpiredPaths();
        if (mCachedPaths.empty()) {
            return;
        }
        earliestExpiry = earliestExpiryTime();
    }

    Duration delay = earliestExpiry - now;
    auto delayMs = delay.total_milliseconds();
    if (delayMs < 0) {
        delayMs = 0;
    }

    debug() << "Scheduling expiry timer for " << delayMs << "ms";
    mExpiryTimer->expires_after(chrono::milliseconds(delayMs));
    mExpiryTimer->async_wait(boost::bind(
        &ExchangePathsManager::onExpiryTimer,
        this,
        as::placeholders::error));
}

void ExchangePathsManager::onExpiryTimer(
    const boost::system::error_code &error)
{
    if (error == as::error::operation_aborted) {
        return; // Timer was cancelled
    }
    if (error != boost::system::errc::success) {
        return;
    }

    lock_guard<mutex> lock(mCacheMutex);

    debug() << "Expiry timer fired, removing expired paths";
    removeExpiredPaths();

    // Reschedule timer if there are still paths remaining
    if (!mCachedPaths.empty()) {
        scheduleExpiryTimer();
    }
}

DateTime ExchangePathsManager::earliestExpiryTime() const
{
    if (mCachedPaths.empty()) {
        return utc_now();
    }

    DateTime earliest = mCachedPaths.begin()->second.computedAt +
                        boost::posix_time::seconds(kPathResultsTTLSeconds);

    for (const auto &pair : mCachedPaths) {
        DateTime expiresAt = pair.second.computedAt + boost::posix_time::seconds(kPathResultsTTLSeconds);
        if (expiresAt < earliest) {
            earliest = expiresAt;
        }
    }

    return earliest;
}

void ExchangePathsManager::removeExpiredPaths()
{
    auto now = utc_now();
    auto it = mCachedPaths.begin();
    size_t removedCount = 0;

    while (it != mCachedPaths.end()) {
        DateTime expiresAt = it->second.computedAt + boost::posix_time::seconds(kPathResultsTTLSeconds);
        if (expiresAt <= now) {
            debug() << "Removing expired paths for contractor " << it->first.contractor
                    << " (sender_eq=" << it->first.senderEquivalent
                    << ", receiver_eq=" << it->first.receiverEquivalent << ")";
            it = mCachedPaths.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    if (removedCount > 0) {
        debug() << "Removed " << removedCount << " expired path sets";
    }
}

LoggerStream ExchangePathsManager::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream ExchangePathsManager::debug() const
{
    return mLogger.debug(logHeader());
}

LoggerStream ExchangePathsManager::warning() const
{
    return mLogger.warning(logHeader());
}

LoggerStream ExchangePathsManager::error() const
{
    return mLogger.error(logHeader());
}

const string ExchangePathsManager::logHeader() const
{
    return "[ExchangePathsManager]";
}

// ============================================================================
// Path Calculation Implementation
// ============================================================================

namespace {
// Safe conversion from double to TrustLineAmount with flooring and sanity checks.
static TrustLineAmount toAmountSafe(double v)
{
    if (!std::isfinite(v) || v <= 0.0) {
        return TrustLineAmount(0);
    }
    double floored = std::floor(v + 1e-9);
    if (floored <= 0.0) {
        return TrustLineAmount(0);
    }
    uint64_t iv = static_cast<uint64_t>(floored);
    return TrustLineAmount(iv);
}
} // anonymous namespace

double ExchangePathsManager::fetchEdgeCapacity(
    EquivalentsSubsystemsRouter *router,
    ContractorID from,
    ContractorID to,
    SerializedEquivalent equivalent)
{
    try {
        auto tlm = router->topologyTrustLineManager(equivalent);
        for (auto tlPtr : tlm->trustLinePtrsSet(from)) {
            auto tl = tlPtr->topologyTrustLine();
            if (tl->targetID() == to) {
                return tl->freeAmount()->convert_to<double>();
            }
        }
    } catch (...) {
    }
    return 0.0;
}

double ExchangePathsManager::findExchangeRateForStep(
    const ExchangePath &path,
    size_t index)
{
    for (const auto &ex : path.exchangeSteps) {
        if (ex.nodeID == path.ids[index] &&
            ex.fromEquivalent == path.equivalents[index] &&
            ex.toEquivalent == path.equivalents[index + 1]) {
            double rate = ex.exchangeRate.convert_to<double>();
            int16_t sh = ex.exchangeRateShift;
            if (sh > 0) {
                for (int t = 0; t < sh; ++t) {
                    rate *= 10.0;
                }
            }
            if (sh < 0) {
                for (int t = 0; t < -sh; ++t) {
                    rate /= 10.0;
                }
            }
            return rate;
        }
    }
    return 1.0;
}

bool ExchangePathsManager::commissionAppliedOnPath(
    const vector<pair<ContractorID, SerializedEquivalent>> &applied,
    ContractorID node,
    SerializedEquivalent equivalent)
{
    for (const auto &item : applied) {
        if (item.first == node && item.second == equivalent) {
            return true;
        }
    }
    return false;
}

double ExchangePathsManager::computeMaxRawAllowed(
    const ExchangePath &path,
    const vector<pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
    const map<EdgeKey, double> &edgeRemaining,
    EquivalentsSubsystemsRouter *router,
    double solverRaw)
{
    (void)solverRaw;
    double alpha = 1.0;
    double beta = 0.0;
    double maxAllowed = path.calculateMaxCapacity().convert_to<double>();
    if (!std::isfinite(maxAllowed) || maxAllowed < 0.0) {
        maxAllowed = std::numeric_limits<double>::infinity();
    }

    for (size_t k = 0; k + 1 < path.ids.size(); ++k) {
        if (path.ids[k] == path.ids[k + 1] && path.equivalents[k] != path.equivalents[k + 1]) {
            double rate = findExchangeRateForStep(path, k);
            alpha *= rate;
            beta *= rate;
            continue;
        }

        EdgeKey key{path.ids[k], path.ids[k + 1], path.equivalents[k]};
        double residual = 0.0;
        auto it = edgeRemaining.find(key);
        if (it != edgeRemaining.end()) {
            residual = it->second;
        } else {
            residual = fetchEdgeCapacity(router, key.from, key.to, key.equivalent);
        }

        if (alpha > 1e-9) {
            double allowedEdge = (residual + beta) / alpha;
            if (std::isfinite(allowedEdge)) {
                maxAllowed = std::min(maxAllowed, allowedEdge);
            } else {
                maxAllowed = 0.0;
            }
        } else {
            maxAllowed = 0.0;
        }

        ContractorID arrivalNode = path.ids[k + 1];
        SerializedEquivalent arrivalEq = path.equivalents[k + 1];
        if (commissionAppliedOnPath(appliedCommissions, arrivalNode, arrivalEq)) {
            try {
                auto tlm = router->topologyTrustLineManager(arrivalEq);
                auto commission = tlm->getCommission(arrivalNode, arrivalEq);
                if (commission) {
                    beta += static_cast<double>(commission->amount());
                }
            } catch (...) {}
        }
    }

    if (!std::isfinite(maxAllowed) || maxAllowed < 0.0) {
        return 0.0;
    }
    return maxAllowed;
}

set<ContractorID> ExchangePathsManager::collectExchangeNodes(const vector<ExchangePath>& paths) {
    set<ContractorID> exchangeNodes;
    for (const auto& path : paths) {
        for (const auto& exchange : path.exchangeSteps) {
            // Only add actual exchange nodes (fromEquivalent != toEquivalent)
            // Skip commission entries (fromEquivalent == toEquivalent)
            if (exchange.fromEquivalent != exchange.toEquivalent) {
                exchangeNodes.insert(exchange.nodeID);
            }
        }
    }
    return exchangeNodes;
}

string ExchangePathsManager::formatDetailedPathWithRouter(
    const OptimalPathResult &pathResult,
    EquivalentsSubsystemsRouter *router,
    const set<ContractorID> &exchangeNodes,
    set<pair<ContractorID, SerializedEquivalent>> *processedCommissions,
    map<EdgeKey, double> *edgeRemaining,
    double *deliveredOut)
{
    stringstream ss;
    const auto &path = pathResult.path();

    auto addrOf = [&](ContractorID id) -> string {
        auto addr = router->resolveParticipantAddress(id);
        if (addr) return addr->fullAddress();
        return string("node:") + std::to_string(id);
    };

    double currentFlow = pathResult.optimal_flow.convert_to<double>();
    bool needsArrow = false;

    for (size_t i = 0; i + 1 < path.ids.size(); i++) {
        if (path.ids[i] == path.ids[i + 1]) {
            for (const auto &exchange : path.exchangeSteps) {
                if (exchange.nodeID == path.ids[i] &&
                    exchange.fromEquivalent == path.equivalents[i] &&
                    exchange.toEquivalent == path.equivalents[i + 1]) {
                    double r = exchange.exchangeRate.convert_to<double>();
                    int16_t sh = exchange.exchangeRateShift;
                    if (sh > 0) for (int j = 0; j < sh; ++j) r *= 10.0;
                    if (sh < 0) for (int j = 0; j < -sh; ++j) r /= 10.0;
                    double outFlow = currentFlow * r;

                    if (needsArrow) ss << " -> ";
                    ss << "E(node: " << addrOf(exchange.nodeID)
                       << "; id: " << exchange.nodeID
                       << "; eqs [" << exchange.fromEquivalent << "->" << exchange.toEquivalent
                       << "; flows:[" << currentFlow << "->" << outFlow << ")";
                    currentFlow = outFlow;
                    needsArrow = true;
                    break;
                }
            }
            continue;
        }

        if (needsArrow) ss << " -> ";

        bool showCommission = false;
        bool commissionAvailable = false;
        double commissionAmount = 0.0;
        pair<ContractorID, SerializedEquivalent> nodeCommissionKey = {path.ids[i + 1], path.equivalents[i + 1]};

        if (i + 1 > 0 && i + 1 < path.ids.size() - 1 &&
            exchangeNodes.find(path.ids[i + 1]) == exchangeNodes.end()) {
            try {
                auto tlm = router->topologyTrustLineManager(path.equivalents[i + 1]);
                auto commission = tlm->getCommission(path.ids[i + 1], path.equivalents[i + 1]);
                if (commission) {
                    commissionAmount = static_cast<double>(commission->amount());
                    commissionAvailable = commissionAmount > 0.0;

                    if (commissionAvailable) {
                        if (!processedCommissions || processedCommissions->find(nodeCommissionKey) == processedCommissions->end()) {
                            showCommission = true;
                            if (processedCommissions) {
                                processedCommissions->insert(nodeCommissionKey);
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        double flowBeforeCommission = currentFlow;
        if (edgeRemaining) {
            EdgeKey key{path.ids[i], path.ids[i + 1], path.equivalents[i]};
            auto it = edgeRemaining->find(key);
            double &remain = (it != edgeRemaining->end())
                ? it->second
                : (*edgeRemaining)[key] = fetchEdgeCapacity(router, key.from, key.to, key.equivalent);
            if (remain < flowBeforeCommission - 1e-9) {
                flowBeforeCommission = std::max(0.0, remain);
            }
            remain = std::max(0.0, remain - flowBeforeCommission);
        }

        ss << "F(nodes: [" << addrOf(path.ids[i]) << " -> " << addrOf(path.ids[i + 1])
           << "]; ids [" << path.ids[i] << " -> " << path.ids[i + 1]
           << "; flow: " << flowBeforeCommission
           << "; eq: " << path.equivalents[i] << ")";

        currentFlow = flowBeforeCommission;

        if (showCommission && commissionAvailable) {
            currentFlow -= commissionAmount;
            if (currentFlow < 0.0) {
                currentFlow = 0.0;
            }
            ss << " -> C(node: " << addrOf(path.ids[i + 1])
               << "; id: " << path.ids[i + 1]
               << "; commission: " << commissionAmount
               << "; flow after: " << currentFlow
               << "; eq: " << path.equivalents[i + 1] << ")";
        }

        needsArrow = true;
    }

    if (deliveredOut) {
        *deliveredOut = currentFlow;
    }
    return ss.str();
}

double ExchangePathsManager::simulatePathNetAmount(
    const ExchangePath &path,
    double sourceFlow,
    EquivalentsSubsystemsRouter *router,
    const set<ContractorID> &exchangeNodes,
    set<pair<ContractorID, SerializedEquivalent>> *appliedOnce,
    vector<pair<ContractorID, SerializedEquivalent>> *appliedNow)
{
    if (!std::isfinite(sourceFlow) || sourceFlow <= 0.0) {
        return 0.0;
    }
    if (path.ids.empty() || path.ids.size() != path.equivalents.size()) {
        return 0.0;
    }

    double currentAmount = sourceFlow;

    for (size_t k = 0; k + 1 < path.ids.size(); ++k) {
        if (path.ids[k] == path.ids[k + 1] && path.equivalents[k] != path.equivalents[k + 1]) {
            for (const auto &ex : path.exchangeSteps) {
                if (ex.nodeID == path.ids[k] && ex.fromEquivalent == path.equivalents[k]
                    && ex.toEquivalent == path.equivalents[k + 1]) {
                    double r = ex.exchangeRate.convert_to<double>();
                    int16_t sh = ex.exchangeRateShift;
                    if (sh > 0) for (int i = 0; i < sh; ++i) r *= 10.0;
                    if (sh < 0) for (int i = 0; i < -sh; ++i) r /= 10.0;
                    currentAmount *= r;
                    break;
                }
            }
            continue;
        }

        size_t arrivalIndex = k + 1;
        if (arrivalIndex > 0 && arrivalIndex < path.ids.size() - 1) {
            ContractorID nodeId = path.ids[arrivalIndex];
            SerializedEquivalent nodeEq = path.equivalents[arrivalIndex];

            if (exchangeNodes.find(nodeId) == exchangeNodes.end()) {
                try {
                    auto tlm = router->topologyTrustLineManager(nodeEq);
                    auto commission = tlm->getCommission(nodeId, nodeEq);
                    if (commission) {
                        auto key = std::make_pair(nodeId, nodeEq);
                        bool charge = true;
                        if (appliedOnce && appliedOnce->find(key) != appliedOnce->end()) {
                            charge = false;
                        }
                        if (charge) {
                            currentAmount -= static_cast<double>(commission->amount());
                            if (currentAmount <= 0.0) {
                                if (appliedOnce) {
                                    appliedOnce->insert(key);
                                }
                                if (appliedNow) {
                                    appliedNow->push_back(key);
                                }
                                return 0.0;
                            }
                            if (appliedOnce) {
                                appliedOnce->insert(key);
                            }
                            if (appliedNow) {
                                appliedNow->push_back(key);
                            }
                        }
                    }
                } catch (...) {
                }
            }
        }
    }

    return std::max(0.0, currentAmount);
}

TrustLineAmount ExchangePathsManager::getSenderBalance(
    SerializedEquivalent equivalent,
    ContractorID senderID)
{
    auto tlm = mRouter->topologyTrustLineManager(equivalent);
    TrustLineAmount sum = 0;
    for (auto tlPtr : tlm->trustLinePtrsSet(senderID)) {
        sum = sum + *tlPtr->topologyTrustLine()->freeAmount();
    }
    return sum;
}

double ExchangePathsManager::forwardSimulatePath(
    const ExchangePath &path,
    double inputAmount,
    set<pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
    map<EdgeKey, double> &edgeRemainingCapacity)
{
    if (!std::isfinite(inputAmount) || inputAmount <= 0.0) {
        return 0.0;
    }
    if (!path.isValid()) {
        return 0.0;
    }

    double currentAmount = inputAmount;

    // Temporary set for commissions applied on this path only
    // Will be merged into appliedCommissions only if path succeeds
    set<pair<ContractorID, SerializedEquivalent>> commissionsOnThisPath;

    // Temporary map for edge capacity changes on this path only
    // Will be merged into edgeRemainingCapacity only if path succeeds
    map<EdgeKey, double> capacityChangesOnThisPath;

    for (size_t i = 0; i + 1 < path.ids.size(); ++i) {
        ContractorID fromNode = path.ids[i];
        ContractorID toNode = path.ids[i + 1];
        SerializedEquivalent currentEquiv = path.equivalents[i];
        SerializedEquivalent nextEquiv = path.equivalents[i + 1];

        // Check if this is an exchange step (same node, different equivalents)
        if (fromNode == toNode && currentEquiv != nextEquiv) {
            // Apply exchange rate
            for (const auto &ex : path.exchangeSteps) {
                if (ex.nodeID == fromNode &&
                    ex.fromEquivalent == currentEquiv &&
                    ex.toEquivalent == nextEquiv) {

                    // Calculate exchange rate with shift
                    double rate = ex.exchangeRate.convert_to<double>();
                    int16_t shift = ex.exchangeRateShift;
                    double effectiveRate = rate * std::pow(10.0, shift);

                    double amountBeforeExchange = currentAmount;
                    currentAmount *= effectiveRate;

                    // Validate exchange limits if set
                    if (ex.minExchangeAmount > TrustLineAmount(0)) {
                        if (amountBeforeExchange < ex.minExchangeAmount.convert_to<double>()) {
                            // Below minimum exchange amount - skip path
                            return 0.0;
                        }
                    }
                    if (ex.maxExchangeAmount > TrustLineAmount(0)) {
                        if (amountBeforeExchange > ex.maxExchangeAmount.convert_to<double>()) {
                            // Above maximum - cannot use this path
                            return 0.0;
                        }
                    }

                    break;
                }
            }
            continue;
        }

        // Check and update edge capacity for regular edge
        EdgeKey edgeKey{fromNode, toNode, currentEquiv};

        // Get current available capacity (considering both global state and temporary changes)
        double availableCapacity;
        if (capacityChangesOnThisPath.find(edgeKey) != capacityChangesOnThisPath.end()) {
            // Already modified on this path
            availableCapacity = capacityChangesOnThisPath[edgeKey];
        } else if (edgeRemainingCapacity.find(edgeKey) != edgeRemainingCapacity.end()) {
            // Exists in global state
            availableCapacity = edgeRemainingCapacity[edgeKey];
        } else {
            // Not yet tracked - fetch initial capacity
            availableCapacity = fetchEdgeCapacity(mRouter, fromNode, toNode, currentEquiv);
        }

        // Check if edge has sufficient capacity
        if (currentAmount > availableCapacity) {
            // Reduce flow to available capacity
            currentAmount = availableCapacity;
            if (currentAmount <= 0.0) {
                return 0.0;
            }
        }

        // Store capacity change in temporary map
        capacityChangesOnThisPath[edgeKey] = availableCapacity - currentAmount;

        // Apply commission at destination node (if intermediate node)
        if (i + 1 < path.ids.size() - 1) {
            ContractorID nodeId = toNode;
            SerializedEquivalent nodeEq = nextEquiv;

            try {
                auto tlm = mRouter->topologyTrustLineManager(nodeEq);
                auto commission = tlm->getCommission(nodeId, nodeEq);

                if (commission) {
                    auto commissionKey = std::make_pair(nodeId, nodeEq);

                    // Check if commission already applied globally ("charge once" semantics)
                    if (appliedCommissions.find(commissionKey) == appliedCommissions.end()) {
                        double commissionAmount = static_cast<double>(commission->amount());
                        currentAmount -= commissionAmount;

                        if (currentAmount <= 0.0) {
                            // Path failed - don't add commission or capacity changes
                            return 0.0;
                        }

                        // Add to temporary set - will be merged only if path succeeds
                        commissionsOnThisPath.insert(commissionKey);
                    }
                }
            } catch (...) {
                // If commission lookup fails, continue without commission
            }
        }
    }

    double result = std::max(0.0, currentAmount);

    // Only if path succeeded (result > 0), merge temporary changes into global state
    if (result > 0.0) {
        appliedCommissions.insert(commissionsOnThisPath.begin(), commissionsOnThisPath.end());
        for (const auto &entry : capacityChangesOnThisPath) {
            edgeRemainingCapacity[entry.first] = entry.second;
        }
    }

    return result;
}

double ExchangePathsManager::inverseSimulatePath(
    const ExchangePath &path,
    double targetOutputAmount,
    set<pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
    map<EdgeKey, double> &edgeRemainingCapacity)
{
    if (!std::isfinite(targetOutputAmount) || targetOutputAmount <= 0.0) {
        return 0.0;
    }
    if (!path.isValid()) {
        return 0.0;
    }

    double requiredAmount = targetOutputAmount;

    // Temporary set for commissions applied on this path only
    // Will be merged into appliedCommissions only if path succeeds
    set<pair<ContractorID, SerializedEquivalent>> commissionsOnThisPath;

    // Temporary map for edge capacity changes on this path only
    // Will be merged into edgeRemainingCapacity only if path succeeds
    map<EdgeKey, double> capacityChangesOnThisPath;

    // Work backwards through path from receiver to sender
    for (int i = static_cast<int>(path.ids.size()) - 2; i >= 0; --i) {
        ContractorID fromNode = path.ids[i];
        ContractorID toNode = path.ids[i + 1];
        SerializedEquivalent currentEquiv = path.equivalents[i];
        SerializedEquivalent nextEquiv = path.equivalents[i + 1];

        // Apply commission at destination node (if intermediate node)
        // Commission is added when going backwards
        if (i + 1 < static_cast<int>(path.ids.size()) - 1) {
            ContractorID nodeId = toNode;
            SerializedEquivalent nodeEq = nextEquiv;

            try {
                auto tlm = mRouter->topologyTrustLineManager(nodeEq);
                auto commission = tlm->getCommission(nodeId, nodeEq);

                if (commission) {
                    auto commissionKey = std::make_pair(nodeId, nodeEq);

                    // Check if commission already applied globally ("charge once" semantics)
                    if (appliedCommissions.find(commissionKey) == appliedCommissions.end()) {
                        double commissionAmount = static_cast<double>(commission->amount());
                        requiredAmount += commissionAmount;

                        // Add to temporary set - will be merged only if path succeeds
                        commissionsOnThisPath.insert(commissionKey);
                    }
                }
            } catch (...) {
                // If commission lookup fails, continue without commission
            }
        }

        // Check if this is an exchange step (same node, different equivalents)
        if (fromNode == toNode && currentEquiv != nextEquiv) {
            // Apply inverse exchange rate
            for (const auto &ex : path.exchangeSteps) {
                if (ex.nodeID == fromNode &&
                    ex.fromEquivalent == currentEquiv &&
                    ex.toEquivalent == nextEquiv) {

                    // Calculate exchange rate with shift
                    double rate = ex.exchangeRate.convert_to<double>();
                    int16_t shift = ex.exchangeRateShift;
                    double effectiveRate = rate * std::pow(10.0, shift);

                    // Inverse exchange: divide by rate
                    if (effectiveRate <= 0.0) {
                        // Invalid rate - path failed, don't add commissions or capacity changes
                        return 0.0;
                    }

                    double amountAfterInverse = requiredAmount / effectiveRate;

                    // Validate exchange limits if set
                    if (ex.minExchangeAmount > TrustLineAmount(0)) {
                        if (amountAfterInverse < ex.minExchangeAmount.convert_to<double>()) {
                            // Below minimum exchange amount - path cannot satisfy
                            return 0.0;
                        }
                    }
                    if (ex.maxExchangeAmount > TrustLineAmount(0)) {
                        if (amountAfterInverse > ex.maxExchangeAmount.convert_to<double>()) {
                            // Above maximum - cannot use this path
                            return 0.0;
                        }
                    }

                    requiredAmount = amountAfterInverse;
                    break;
                }
            }
            continue;
        }

        // Check edge capacity for regular edge
        EdgeKey edgeKey{fromNode, toNode, currentEquiv};

        // Get current available capacity (considering both global state and temporary changes)
        double availableCapacity;
        if (capacityChangesOnThisPath.find(edgeKey) != capacityChangesOnThisPath.end()) {
            // Already modified on this path
            availableCapacity = capacityChangesOnThisPath[edgeKey];
        } else if (edgeRemainingCapacity.find(edgeKey) != edgeRemainingCapacity.end()) {
            // Exists in global state
            availableCapacity = edgeRemainingCapacity[edgeKey];
        } else {
            // Not yet tracked - fetch initial capacity
            availableCapacity = fetchEdgeCapacity(mRouter, fromNode, toNode, currentEquiv);
        }

        // Check if edge has sufficient capacity for required amount
        if (requiredAmount > availableCapacity) {
            // Path cannot satisfy required amount
            return 0.0;
        }

        // Store capacity change in temporary map
        capacityChangesOnThisPath[edgeKey] = availableCapacity - requiredAmount;
    }

    double result = std::max(0.0, requiredAmount);

    // Only if path succeeded (result > 0), merge temporary changes into global state
    if (result > 0.0) {
        appliedCommissions.insert(commissionsOnThisPath.begin(), commissionsOnThisPath.end());
        for (const auto &entry : capacityChangesOnThisPath) {
            edgeRemainingCapacity[entry.first] = entry.second;
        }
    }

    return result;
}

vector<ExchangePath> ExchangePathsManager::enumerateAllFeasiblePaths(
    ContractorID targetContractor,
    SerializedEquivalent receiverEquivalent,
    const vector<SerializedEquivalent> &senderEquivalents,
    ContractorID senderID,
    int maxPathLength)
{
    vector<ExchangePath> allPaths;

    vector<SerializedEquivalent> startEquivalents;
    if (senderEquivalents.empty()) {
        startEquivalents.push_back(receiverEquivalent);
    } else {
        startEquivalents.reserve(senderEquivalents.size() + 1);
        startEquivalents.insert(startEquivalents.end(),
                                senderEquivalents.begin(),
                                senderEquivalents.end());
        if (std::find(startEquivalents.begin(), startEquivalents.end(), receiverEquivalent) == startEquivalents.end()) {
            startEquivalents.push_back(receiverEquivalent);
        }
    }

    std::sort(startEquivalents.begin(), startEquivalents.end());
    startEquivalents.erase(std::unique(startEquivalents.begin(), startEquivalents.end()), startEquivalents.end());

    for (const auto& senderEquiv : startEquivalents) {
        enumeratePathsFromEquivalent(senderEquiv, targetContractor, receiverEquivalent, senderID, allPaths, maxPathLength);
    }

    info() << "Found " << allPaths.size() << " feasible paths to contractor " << targetContractor;
    return allPaths;
}

void ExchangePathsManager::enumeratePathsFromEquivalent(
    SerializedEquivalent startEquivalent,
    ContractorID targetContractor,
    SerializedEquivalent targetEquivalent,
    ContractorID senderID,
    vector<ExchangePath> &allPaths,
    int maxPathLength)
{
    vector<ContractorID> currentPath;
    vector<SerializedEquivalent> currentEquivPath;
    vector<ExchangeStep> currentExchanges;

    dfsEnumeratePaths(
        senderID, startEquivalent,
        targetContractor, targetEquivalent,
        currentPath, currentEquivPath, currentExchanges,
        allPaths, maxPathLength);
}

void ExchangePathsManager::dfsEnumeratePaths(
    ContractorID currentNode,
    SerializedEquivalent currentEquivalent,
    ContractorID targetNode,
    SerializedEquivalent targetEquivalent,
    vector<ContractorID> &currentPath,
    vector<SerializedEquivalent> &currentEquivPath,
    vector<ExchangeStep> &currentExchanges,
    vector<ExchangePath> &results,
    int maxDepth,
    int currentDepth)
{
    debug() << "DFS: node=" << currentNode << " eq=" << currentEquivalent
            << " target=" << targetNode << " targetEq=" << targetEquivalent
            << " depth=" << currentDepth << "/" << maxDepth;

    // Avoid cycles on (node, equivalent)
    for (size_t i = 0; i < currentPath.size(); ++i) {
        if (currentPath[i] == currentNode && currentEquivPath[i] == currentEquivalent) {
            return;
        }
    }

    // Add current node to path
    currentPath.push_back(currentNode);
    currentEquivPath.push_back(currentEquivalent);

    // If target reached with desired equivalent – finalize the path
    if (currentNode == targetNode && currentEquivalent == targetEquivalent) {
        ExchangePath completePath;
        completePath.ids = currentPath;
        completePath.equivalents = currentEquivPath;
        completePath.exchangeSteps = currentExchanges;

        // Compute path capacity accounting for commission-aware flow distribution.
        TrustLineAmount pathCapacity;

        // Check if this path has exchange steps to identify exchange nodes
        std::set<ContractorID> exchangeNodesInPath;
        for (const auto &ex : currentExchanges) {
            exchangeNodesInPath.insert(ex.nodeID);
        }

        // Simulate flow limitations along the path in terms of source-equivalent units.
        double cumulativeRate = 1.0;
        double maxSourceFlow = std::numeric_limits<double>::infinity();
        double accumulatedSourceCommissions = 0.0;
        bool pathFeasible = true;

        for (size_t k = 0; k + 1 < currentPath.size(); ++k) {
            // Handle exchange self-edge to update cumulative rate
            if (currentPath[k] == currentPath[k + 1] && currentEquivPath[k] != currentEquivPath[k + 1]) {
                for (const auto &ex : currentExchanges) {
                    if (ex.nodeID == currentPath[k] && ex.fromEquivalent == currentEquivPath[k]
                        && ex.toEquivalent == currentEquivPath[k + 1]) {
                        double r = ex.exchangeRate.convert_to<double>();
                        int16_t sh = ex.exchangeRateShift;
                        if (sh > 0) for (int i = 0; i < sh; ++i) r *= 10.0;
                        if (sh < 0) for (int i = 0; i < -sh; ++i) r /= 10.0;
                        cumulativeRate *= r;
                        break;
                    }
                }
                continue;
            }

            // Real edge: obtain capacity in current equivalent and convert to source units
            auto tlm = mRouter->topologyTrustLineManager(currentEquivPath[k]);
            TrustLineAmount edgeCap = TrustLineAmount(0);
            for (auto tlPtr : tlm->trustLinePtrsSet(currentPath[k])) {
                auto tl = tlPtr->topologyTrustLine();
                if (tl->targetID() == currentPath[k + 1]) {
                    edgeCap = *tl->freeAmount();
                    break;
                }
            }

            double edgeCapD = edgeCap.convert_to<double>();
            double edgeCapSourceD = (cumulativeRate > 0.0) ? (edgeCapD / cumulativeRate) : 0.0;
            if (edgeCapSourceD < 0.0 || !std::isfinite(edgeCapSourceD)) {
                edgeCapSourceD = 0.0;
            }

            // Amount that can be injected at the source while respecting this edge
            double allowedByEdge = accumulatedSourceCommissions + edgeCapSourceD;
            if (!std::isfinite(allowedByEdge)) {
                allowedByEdge = accumulatedSourceCommissions;
            }
            maxSourceFlow = std::min(maxSourceFlow, allowedByEdge);

            if (maxSourceFlow < accumulatedSourceCommissions - 1e-9) {
                pathFeasible = false;
                break;
            }

            // Apply commission at the arrival node, if any (skip sender, receiver, and exchange nodes)
            size_t arrivalIndex = k + 1;
            if (arrivalIndex > 0 && arrivalIndex < currentPath.size() - 1) {
                ContractorID arrivalNode = currentPath[arrivalIndex];
                SerializedEquivalent arrivalEquiv = currentEquivPath[arrivalIndex];
                if (exchangeNodesInPath.find(arrivalNode) == exchangeNodesInPath.end()) {
                    try {
                        auto targetTlm = mRouter->topologyTrustLineManager(arrivalEquiv);
                        auto commission = targetTlm->getCommission(arrivalNode, arrivalEquiv);
                        if (commission) {
                            double commissionAmount = static_cast<double>(commission->amount());
                            double commissionSourceD = (cumulativeRate > 0.0)
                                ? (commissionAmount / cumulativeRate)
                                : commissionAmount;
                            if (commissionSourceD < 0.0 || !std::isfinite(commissionSourceD)) {
                                commissionSourceD = 0.0;
                            }

                            accumulatedSourceCommissions += commissionSourceD;
                            if (maxSourceFlow < accumulatedSourceCommissions - 1e-9) {
                                pathFeasible = false;
                                break;
                            }
                        }
                    } catch (...) {
                        // ignore commission lookup errors
                    }
                }
            }
        }

        if (!pathFeasible || !std::isfinite(maxSourceFlow) || maxSourceFlow <= 0.0) {
            maxSourceFlow = 0.0;
        }

        // The path capacity is the maximal source-side flow compatible with commissions and capacities
        pathCapacity = toAmountSafe(std::max(0.0, maxSourceFlow));
        completePath.minCapacity = pathCapacity;

        // Effective rate = product of exchange steps
        double effRate = 1.0;
        for (const auto &ex : currentExchanges) {
            double r = ex.exchangeRate.convert_to<double>();
            int16_t sh = ex.exchangeRateShift;
            if (sh > 0) for (int i = 0; i < sh; ++i) r *= 10.0;
            if (sh < 0) for (int i = 0; i < -sh; ++i) r /= 10.0;
            effRate *= r;
        }
        completePath.effectiveExchangeRate = effRate;

        // Sum fixed commissions along the path in TARGET equivalent only (exclude sender and receiver nodes)
        // Source equivalent commissions are handled separately in LP constraints
        TrustLineAmount sumComm = TrustLineAmount(0);
        for (size_t i = 1; i < currentPath.size() - 1; ++i) { // Skip first (sender) and last (receiver) nodes
            try {
                // Only count commissions in target equivalent (after all exchanges)
                if (currentEquivPath[i] == targetEquivalent) {
                    auto tlm = mRouter->topologyTrustLineManager(currentEquivPath[i]);
                    auto c = tlm->getCommission(currentPath[i], currentEquivPath[i]);
                    if (c) {
                        sumComm = sumComm + TrustLineAmount(c->amount());
                    }
                }
            } catch (...) {
                // ignore
            }
        }
        completePath.totalCommissions = sumComm;

        // Add commissions as ExchangeStep entries (fromEquivalent == toEquivalent)
        // for use in calculateFlows() method
        for (size_t i = 1; i < currentPath.size() - 1; ++i) { // Skip first (sender) and last (receiver) nodes
            ContractorID nodeID = currentPath[i];
            SerializedEquivalent nodeEquiv = currentEquivPath[i];

            // Skip exchange nodes (they don't charge transit commissions)
            if (exchangeNodesInPath.find(nodeID) != exchangeNodesInPath.end()) {
                continue;
            }

            try {
                auto tlm = mRouter->topologyTrustLineManager(nodeEquiv);
                auto c = tlm->getCommission(nodeID, nodeEquiv);
                if (c && c->amount() > TrustLineAmount(0)) {
                    ExchangeStep commissionStep;
                    commissionStep.nodeID = nodeID;
                    commissionStep.fromEquivalent = nodeEquiv;
                    commissionStep.toEquivalent = nodeEquiv;  // Same equivalent - marks this as commission
                    commissionStep.exchangeRate = TrustLineAmount(0);
                    commissionStep.exchangeRateShift = 0;
                    commissionStep.minExchangeAmount = TrustLineAmount(0);
                    commissionStep.maxExchangeAmount = TrustLineAmount(0);
                    commissionStep.commission = c->amount();

                    completePath.exchangeSteps.push_back(commissionStep);
                }
            } catch (...) {
                // ignore commission lookup errors
            }
        }

        if (completePath.isValid()) {
            debug() << "DFS: Valid path added with capacity " << completePath.minCapacity;
            results.push_back(completePath);
        } else {
            warning() << "DFS: Path invalid: nodes=" << completePath.ids.size()
                     << " equivs=" << completePath.equivalents.size();
        }
    } else {
        // Check depth limit before continuing search
        if (currentDepth >= maxDepth) {
            debug() << "DFS: depth limit reached at node " << currentNode;
            // Backtrack
            currentPath.pop_back();
            currentEquivPath.pop_back();
            return;
        }

        // 1) Traverse neighbors in same equivalent
        debug() << "DFS: Looking for neighbors of node " << currentNode << " in eq " << currentEquivalent;
        auto tlm = mRouter->topologyTrustLineManager(currentEquivalent);
        size_t neighborCount = 0;
        for (auto tlPtr : tlm->trustLinePtrsSet(currentNode)) {
            auto tl = tlPtr->topologyTrustLine();
            if (*tl->freeAmount() == TrustLineAmount(0)) {
                continue;
            }
            neighborCount++;
            dfsEnumeratePaths(
                tl->targetID(), currentEquivalent,
                targetNode, targetEquivalent,
                currentPath, currentEquivPath, currentExchanges,
                results, maxDepth, currentDepth + 1);
        }

        // 2) Try exchanges at current node to any available next equivalent (multi-step exchanges)
        auto allRates = mRatesManager->listExternalRates();
        size_t exchangeCount = 0;
        for (const auto &p : allRates) {
            if (p.first != currentNode) continue; // rate not offered here
            auto rate = p.second;
            if (rate->equivalentFrom() != currentEquivalent) continue;

            SerializedEquivalent nextEq = rate->equivalentTo();
            if (nextEq == currentEquivalent) {
                continue;
            }

            exchangeCount++;

            ExchangeStep step;
            step.nodeID = currentNode;
            step.fromEquivalent = currentEquivalent;
            step.toEquivalent = nextEq;
            step.exchangeRate = rate->exchangeRate();
            step.exchangeRateShift = rate->exchangeRateShift();
            step.minExchangeAmount = rate->minExchangeAmount();
            step.maxExchangeAmount = rate->maxExchangeAmount();
            step.commission = TrustLineAmount(0);

            currentExchanges.push_back(step);
            dfsEnumeratePaths(
                currentNode, nextEq,
                targetNode, targetEquivalent,
                currentPath, currentEquivPath, currentExchanges,
                results, maxDepth, currentDepth + 1);
            currentExchanges.pop_back();
        }
    }

    // Backtrack
    currentPath.pop_back();
    currentEquivPath.pop_back();
}

ExchangePathsManager::MaxFlowResult ExchangePathsManager::calculateMaxFlow(
    ContractorID targetContractor,
    SerializedEquivalent receiverEquivalent,
    const vector<SerializedEquivalent> &senderEquivalents,
    ContractorID senderID,
    uint8_t hopsCount)
{
    MaxFlowResult result;
    result.maxFlow = TrustLineAmount(0);

    try {
        // Step 1: Enumerate all feasible paths
        info() << "Step 1: Enumerate all feasible paths to contractor " << targetContractor;
        vector<ExchangePath> feasiblePaths = enumerateAllFeasiblePaths(
            targetContractor, receiverEquivalent, senderEquivalents, senderID, hopsCount);

        if (feasiblePaths.empty()) {
            warning() << "No feasible paths found to contractor " << targetContractor;
            return result;
        }

        // Step 2: Create Linear Programming solver
        info() << "Step 2: Create Linear Programming solver";
        std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("GLOP"));
        if (!solver) {
            error() << "GLOP solver unavailable";
            return result;
        }

        MPObjective* testObjective = solver->MutableObjective();
        if (testObjective == nullptr) {
            error() << "MutableObjective returns null immediately after solver creation";
            return result;
        }

        // Step 3: Create LP variables for each path
        info() << "Step 3: Create LP variables for each path";
        vector<MPVariable*> pathFlowVars;
        for (size_t i = 0; i < feasiblePaths.size(); i++) {
            double maxCapacity = static_cast<double>(feasiblePaths[i].calculateMaxCapacity());
            if (!std::isfinite(maxCapacity) || maxCapacity < 0.0) {
                warning() << "Step 3: Non-finite or negative maxCapacity for path " << i
                          << ": " << maxCapacity << ". Sanitizing to 0.";
                maxCapacity = 0.0;
            }
            auto* flowVar = solver->MakeNumVar(0.0, maxCapacity,
                                               "flow_path_" + std::to_string(i));
            if (flowVar == nullptr) {
                error() << "LP variable allocation failed for path " << i;
                return result;
            }
            pathFlowVars.push_back(flowVar);
        }

        // Step 5: Add constraints
        info() << "Step 5: Add constraints";
        // Sender balance constraints per equivalent
        for (const auto& equiv : senderEquivalents) {
            auto senderBalance = getSenderBalance(equiv, senderID);
            if (senderBalance == TrustLineAmount(0)) {
                continue;
            }

            auto* balanceConstraint = solver->MakeRowConstraint(
                0.0, static_cast<double>(senderBalance),
                "balance_" + std::to_string(equiv));

            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                if (feasiblePaths[i].startsWithEquivalent(equiv)) {
                    balanceConstraint->SetCoefficient(pathFlowVars[i], 1.0);
                }
            }
        }

        // Capacity constraints per path
        for (size_t i = 0; i < feasiblePaths.size(); i++) {
            const auto &path = feasiblePaths[i];
            double cap = static_cast<double>(path.calculateMaxCapacity());

            auto* capConstraint = solver->MakeRowConstraint(0.0, cap, "cap_path_" + std::to_string(i));
            capConstraint->SetCoefficient(pathFlowVars[i], 1.0);
        }

        struct EdgeContribution {
            size_t pathIndex;
            double multiplier;
        };

        struct EdgeAggregate {
            double capacity = 0.0;
            bool capacityInitialized = false;
            double constantSum = 0.0;
            std::vector<EdgeContribution> contributions;
        };

        std::map<EdgeKey, EdgeAggregate> edgeUsage;

        for (size_t i = 0; i < feasiblePaths.size(); ++i) {
            const auto &path = feasiblePaths[i];
            if (path.ids.size() < 2) {
                continue;
            }

            std::set<ContractorID> exchangeNodesInPath;
            for (const auto &ex : path.exchangeSteps) {
                exchangeNodesInPath.insert(ex.nodeID);
            }

            double multiplier = 1.0;
            double constantTerm = 0.0;

            auto applyExchange = [&](const ExchangeStep &ex) {
                double r = ex.exchangeRate.convert_to<double>();
                int16_t sh = ex.exchangeRateShift;
                if (sh > 0) for (int t = 0; t < sh; ++t) r *= 10.0;
                if (sh < 0) for (int t = 0; t < -sh; ++t) r /= 10.0;
                multiplier *= r;
                constantTerm *= r;
            };

            for (size_t k = 0; k + 1 < path.ids.size(); ++k) {
                ContractorID fromNode = path.ids[k];
                ContractorID toNode = path.ids[k + 1];

                if (fromNode == toNode && path.equivalents[k] != path.equivalents[k + 1]) {
                    for (const auto &ex : path.exchangeSteps) {
                        if (ex.nodeID == fromNode && ex.fromEquivalent == path.equivalents[k]
                            && ex.toEquivalent == path.equivalents[k + 1]) {
                            applyExchange(ex);
                            break;
                        }
                    }
                    continue;
                }

                SerializedEquivalent edgeEq = path.equivalents[k];
                EdgeKey key{fromNode, toNode, edgeEq};
                auto &edge = edgeUsage[key];

                if (!edge.capacityInitialized) {
                    double edgeCapD = 0.0;
                    try {
                        auto tlm = mRouter->topologyTrustLineManager(edgeEq);
                        for (auto tlPtr : tlm->trustLinePtrsSet(fromNode)) {
                            auto tl = tlPtr->topologyTrustLine();
                            if (tl->targetID() == toNode) {
                                edgeCapD = tl->freeAmount()->convert_to<double>();
                                break;
                            }
                        }
                    } catch (...) {
                        edgeCapD = 0.0;
                    }
                    edge.capacity = edgeCapD;
                    edge.capacityInitialized = true;
                }

                edge.contributions.push_back({i, multiplier});
                edge.constantSum += constantTerm;

                size_t arrivalIndex = k + 1;
                if (arrivalIndex > 0 && arrivalIndex < path.ids.size() - 1) {
                    ContractorID arrivalNode = path.ids[arrivalIndex];
                    SerializedEquivalent arrivalEq = path.equivalents[arrivalIndex];
                    if (exchangeNodesInPath.find(arrivalNode) == exchangeNodesInPath.end()) {
                        try {
                            auto tlm = mRouter->topologyTrustLineManager(arrivalEq);
                            auto commission = tlm->getCommission(arrivalNode, arrivalEq);
                            if (commission) {
                                constantTerm += static_cast<double>(commission->amount());
                            }
                        } catch (...) {}
                    }
                }
            }
        }

        for (const auto &entry : edgeUsage) {
            const auto &key = entry.first;
            const auto &data = entry.second;
            double rhs = data.capacity + data.constantSum;
            if (!std::isfinite(rhs)) {
                continue;
            }
            auto constraintName = std::string("edge_cap_") + std::to_string(key.from) + "_" + std::to_string(key.to) + "_" + std::to_string(key.equivalent);
            auto *edgeConstraint = solver->MakeRowConstraint(0.0, rhs, constraintName);
            for (const auto &contrib : data.contributions) {
                edgeConstraint->SetCoefficient(pathFlowVars[contrib.pathIndex], contrib.multiplier);
            }
        }

        // Global commission constraint
        std::set<std::pair<ContractorID, SerializedEquivalent>> allUniqueCommissionNodes;
        double totalUniqueCommissions = 0.0;

        for (size_t i = 0; i < feasiblePaths.size(); i++) {
            const auto &path = feasiblePaths[i];
            for (size_t j = 1; j < path.ids.size() - 1; ++j) {
                allUniqueCommissionNodes.insert({path.ids[j], path.equivalents[j]});
            }
        }

        for (const auto &nodeEquiv : allUniqueCommissionNodes) {
            try {
                auto tlm = mRouter->topologyTrustLineManager(nodeEquiv.second);
                auto commission = tlm->getCommission(nodeEquiv.first, nodeEquiv.second);
                if (commission) {
                    totalUniqueCommissions += static_cast<double>(commission->amount());
                }
            } catch (...) {
            }
        }

        if (totalUniqueCommissions > 0.0) {
            auto* globalCommissionConstraint = solver->MakeRowConstraint(
                totalUniqueCommissions, solver->infinity(), "global_unique_commissions");
            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                double rate = feasiblePaths[i].calculateEffectiveExchangeRate();
                globalCommissionConstraint->SetCoefficient(pathFlowVars[i], rate);
            }
        }

        // Exchange min/max limits per step
        for (size_t i = 0; i < feasiblePaths.size(); i++) {
            const auto &p = feasiblePaths[i];
            if (p.exchangeSteps.empty()) {
                continue;
            }

            double cumRate = 1.0;
            double lowerBound = 0.0;
            double upperBound = solver->infinity();

            for (const auto &ex : p.exchangeSteps) {
                double r = ex.exchangeRate.convert_to<double>();
                int16_t sh = ex.exchangeRateShift;
                if (sh > 0) for (int j = 0; j < sh; ++j) r *= 10.0;
                if (sh < 0) for (int j = 0; j < -sh; ++j) r /= 10.0;

                double minEx = ex.minExchangeAmount.convert_to<double>();
                double maxEx = ex.maxExchangeAmount.convert_to<double>();

                if (minEx > 0.0) {
                    lowerBound = std::max(lowerBound, minEx / cumRate);
                }
                if (maxEx > 0.0) {
                    upperBound = std::min(upperBound, maxEx / cumRate);
                }

                cumRate *= r;
            }

            if (lowerBound > 0.0) {
                auto *lb = solver->MakeRowConstraint(lowerBound, solver->infinity(),
                    "ex_lb_path_" + std::to_string(i));
                lb->SetCoefficient(pathFlowVars[i], 1.0);
            }
            if (upperBound < solver->infinity()) {
                auto *ub = solver->MakeRowConstraint(0.0, upperBound,
                    "ex_ub_path_" + std::to_string(i));
                ub->SetCoefficient(pathFlowVars[i], 1.0);
            }
        }

        // Node balance constraints
        std::set<ContractorID> pathNodes;
        for (const auto &p : feasiblePaths) {
            for (auto nid : p.ids) pathNodes.insert(nid);
        }

        for (ContractorID node : pathNodes) {
            if (node == senderID || node == targetContractor) continue;

            std::set<SerializedEquivalent> nodeEqs;
            for (const auto &p : feasiblePaths) {
                for (size_t idx = 0; idx < p.ids.size(); ++idx) {
                    if (p.ids[idx] == node) nodeEqs.insert(p.equivalents[idx]);
                }
            }

            for (SerializedEquivalent eq : nodeEqs) {
                auto *balanceEq = solver->MakeRowConstraint(0.0, 0.0,
                    std::string("node_bal_") + std::to_string(node) + "_eq_" + std::to_string(eq));

                for (size_t pi = 0; pi < feasiblePaths.size(); ++pi) {
                    const auto &p = feasiblePaths[pi];

                    auto cumulativeRateAt = [&](size_t idx) -> double {
                        double cr = 1.0;
                        for (size_t j = 1; j <= idx && j < p.ids.size(); ++j) {
                            if (p.ids[j-1] == p.ids[j] && p.equivalents[j-1] != p.equivalents[j]) {
                                for (const auto &ex : p.exchangeSteps) {
                                    if (ex.nodeID == p.ids[j] && ex.fromEquivalent == p.equivalents[j-1]
                                        && ex.toEquivalent == p.equivalents[j]) {
                                        double r = ex.exchangeRate.convert_to<double>();
                                        int16_t sh = ex.exchangeRateShift;
                                        if (sh > 0) for (int t=0; t<sh; ++t) r *= 10.0;
                                        if (sh < 0) for (int t=0; t<-sh; ++t) r /= 10.0;
                                        cr *= r;
                                        break;
                                    }
                                }
                            }
                        }
                        return cr;
                    };

                    double coeff = 0.0;
                    for (size_t idx = 1; idx < p.ids.size(); ++idx) {
                        if (p.ids[idx] == node && p.equivalents[idx] == eq
                            && p.ids[idx-1] != node && p.equivalents[idx-1] == eq) {
                            coeff += cumulativeRateAt(idx);
                        }
                    }
                    for (size_t idx = 0; idx + 1 < p.ids.size(); ++idx) {
                        if (p.ids[idx] == node && p.equivalents[idx] == eq
                            && p.ids[idx+1] != node && p.equivalents[idx+1] == eq) {
                            coeff -= cumulativeRateAt(idx);
                        }
                    }
                    for (size_t idx = 0; idx + 1 < p.ids.size(); ++idx) {
                        if (p.ids[idx] == node && p.ids[idx+1] == node
                            && p.equivalents[idx] != p.equivalents[idx+1]) {
                            for (const auto &ex : p.exchangeSteps) {
                                if (ex.nodeID == node && ex.fromEquivalent == p.equivalents[idx]
                                    && ex.toEquivalent == p.equivalents[idx+1]) {
                                    double cr = cumulativeRateAt(idx);
                                    double r = ex.exchangeRate.convert_to<double>();
                                    int16_t sh = ex.exchangeRateShift;
                                    if (sh > 0) for (int t=0; t<sh; ++t) r *= 10.0;
                                    if (sh < 0) for (int t=0; t<-sh; ++t) r /= 10.0;
                                    if (p.equivalents[idx] == eq) {
                                        coeff -= cr;
                                    }
                                    if (p.equivalents[idx+1] == eq) {
                                        coeff += cr * r;
                                    }
                                }
                            }
                        }
                    }

                    if (coeff != 0.0) {
                        balanceEq->SetCoefficient(pathFlowVars[pi], coeff);
                    }
                }
            }
        }

        // Step 4: Set up objective (maximize total received amount)
        info() << "Step 4: Set up objective (maximize total received amount)";
        {
            double recvUb = 0.0;
            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                double cap = static_cast<double>(feasiblePaths[i].calculateMaxCapacity());
                double rate = feasiblePaths[i].calculateEffectiveExchangeRate();
                if (!std::isfinite(cap) || cap < 0.0) cap = 0.0;
                if (!std::isfinite(rate) || rate < 0.0) rate = 0.0;
                recvUb += cap * rate;
            }
            if (!std::isfinite(recvUb) || recvUb < 0.0) recvUb = 0.0;

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define VTCPD_ASAN_BUILD 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define VTCPD_ASAN_BUILD 1
#endif

#ifdef VTCPD_ASAN_BUILD
            info() << "ASan fallback: using UB estimate as result";
            result.maxFlow = toAmountSafe(recvUb);
            return result;
#endif

            MPVariable* recvVar = solver->MakeNumVar(0.0, recvUb, "recv_total");
            if (recvVar == nullptr) {
                error() << "Failed to create recv_total objective variable";
                return result;
            }

            MPConstraint* objDef = solver->MakeRowConstraint(0.0, 0.0, "obj_definition");
            objDef->SetCoefficient(recvVar, 1.0);

            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                if (pathFlowVars[i] == nullptr) {
                    error() << "LP variable is null before obj_definition for path " << i;
                    return result;
                }
                const auto &path = feasiblePaths[i];
                double rate = path.calculateEffectiveExchangeRate();

                objDef->SetCoefficient(pathFlowVars[i], -rate);
            }

            MPObjective* objective = solver->MutableObjective();
            if (objective == nullptr) {
                error() << "Objective is null (MutableObjective failed)";
                return result;
            }

#ifndef VTCPD_SKIP_OBJECTIVE_CLEAR
            try {
                objective->Clear();
            } catch (const std::exception& e) {
                error() << "Exception in objective->Clear(): " << e.what();
                return result;
            } catch (...) {
                error() << "Unknown exception in objective->Clear()";
                return result;
            }
#else
            info() << "Skipping objective->Clear() under ASan";
#endif
            objective->SetCoefficient(recvVar, 1.0);
            objective->SetMaximization();
        }

        // Step 6: Solve the optimization problem
        info() << "Step 6: Solve the optimization problem";
        MPSolver::ResultStatus status = solver->Solve();

        // Step 7: Handle solver results
        info() << "Step 7: Handle solver results";
        switch (status) {
            case MPSolver::OPTIMAL: {
                MPObjective* objectiveRes = solver->MutableObjective();
                double maxReceivable = objectiveRes ? objectiveRes->Value() : 0.0;
                std::set<ContractorID> exchangeNodes = collectExchangeNodes(feasiblePaths);

                vector<OptimalPathResult> optimalPaths;
                for (size_t i = 0; i < feasiblePaths.size(); i++) {
                    double optimalFlow = pathFlowVars[i]->solution_value();
                    if (optimalFlow > 1e-6) {
                        const auto &path = feasiblePaths[i];
                        OptimalPathResult pathResult;
                        pathResult.mPath = path;
                        pathResult.optimal_flow = toAmountSafe(optimalFlow);
                        pathResult.received_amount = TrustLineAmount(0);
                        pathResult.effective_exchange_rate = path.calculateEffectiveExchangeRate();
                        pathResult.path_efficiency = pathResult.effective_exchange_rate;
                        optimalPaths.push_back(pathResult);
                    }
                }

                std::sort(optimalPaths.begin(), optimalPaths.end(),
                    [](const OptimalPathResult &a, const OptimalPathResult &b) {
                        const double eps = 1e-9;
                        double diff = a.effective_exchange_rate - b.effective_exchange_rate;
                        if (std::fabs(diff) > eps) {
                            return diff > 0.0;
                        }
                        auto aNodes = a.path().ids.size();
                        auto bNodes = b.path().ids.size();
                        if (aNodes != bNodes) {
                            return aNodes < bNodes;
                        }
                        return false;
                    });

                std::set<std::pair<ContractorID, SerializedEquivalent>> appliedCommissionsGlobal;
                std::set<std::pair<ContractorID, SerializedEquivalent>> processedForLogging;

                std::map<EdgeKey, double> edgeRemaining;
                auto ensureEdgeCapacity = [&](ContractorID from, ContractorID to, SerializedEquivalent eq) {
                    EdgeKey key{from, to, eq};
                    if (edgeRemaining.find(key) == edgeRemaining.end()) {
                        edgeRemaining[key] = fetchEdgeCapacity(mRouter, from, to, eq);
                    }
                };

                for (const auto &pathResult : optimalPaths) {
                    const auto &path = pathResult.path();
                    for (size_t idx = 0; idx + 1 < path.ids.size(); ++idx) {
                        if (path.ids[idx] == path.ids[idx + 1]) {
                            continue;
                        }
                        ensureEdgeCapacity(path.ids[idx], path.ids[idx + 1], path.equivalents[idx]);
                    }
                }

                double totalNetReceivable = 0.0;

                for (auto &pathResult : optimalPaths) {
                    double solverRaw = pathResult.optimal_flow.convert_to<double>();
                    std::vector<std::pair<ContractorID, SerializedEquivalent>> appliedNow;
                    simulatePathNetAmount(
                        pathResult.path(),
                        solverRaw,
                        mRouter,
                        exchangeNodes,
                        &appliedCommissionsGlobal,
                        &appliedNow);

                    double maxRawAllowed = computeMaxRawAllowed(
                        pathResult.path(),
                        appliedNow,
                        edgeRemaining,
                        mRouter,
                        solverRaw);
                    double adjustedRaw = maxRawAllowed;
                    if (!std::isfinite(adjustedRaw) || adjustedRaw < 0.0) {
                        adjustedRaw = 0.0;
                    }

                    pathResult.optimal_flow = toAmountSafe(adjustedRaw);

                    double deliveredActual = 0.0;
                    std::string formattedPath = formatDetailedPathWithRouter(
                        pathResult,
                        mRouter,
                        exchangeNodes,
                        &processedForLogging,
                        &edgeRemaining,
                        &deliveredActual);

                    info() << "Optimal path: " << formattedPath;

                    pathResult.received_amount = toAmountSafe(deliveredActual);
                    pathResult.path_efficiency = (adjustedRaw > 1e-9)
                        ? (deliveredActual / adjustedRaw)
                        : 0.0;

                    totalNetReceivable += deliveredActual;
                }

                double netReceivable = totalNetReceivable;
                double totalCommissionReduction = std::max(0.0, maxReceivable - netReceivable);

                result.maxFlow = toAmountSafe(netReceivable);
                result.optimalPaths = optimalPaths;

                info() << "Optimal receivable amount for contractor " << targetContractor
                       << ": gross=" << maxReceivable << ", commission_reduction=" << totalCommissionReduction
                       << ", net=" << netReceivable << " in equivalent " << receiverEquivalent
                       << " achieved through " << optimalPaths.size() << " paths";
                break;
            }
            case MPSolver::INFEASIBLE:
                info() << "No feasible solution exists to contractor " << targetContractor;
                warning() << "No feasible solution exists to contractor " << targetContractor;
                break;
            case MPSolver::UNBOUNDED:
                error() << "LP problem is unbounded - check constraints";
                break;
            case MPSolver::ABNORMAL:
                error() << "LP solver encountered numerical issues";
                break;
            default:
                error() << "LP solver failed with status: " << status;
        }

    } catch (const std::exception &e) {
        error() << "OR-Tools optimization failed: " << e.what();
    }

    return result;
}
