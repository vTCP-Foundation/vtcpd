/**
 * @file FindPathsByMaxFlowExchangeTransaction.cpp
 * @brief Implementation of automatic exchange path collection transaction
 *
 * This transaction is part of PRD 07 - Exchange Payment Topology Collection Integration.
 * It provides automatic path collection for exchange payments, similar to how single-equivalent
 * payments work, but with multi-equivalent awareness.
 */

#include "FindPathsByMaxFlowExchangeTransaction.h"
#include "../max_flow_calculation/CollectTopologyForExchangeTransaction.h"
#include "../../../topology/manager/TopologyTrustLinesManager.h"

FindPathsByMaxFlowExchangeTransaction::FindPathsByMaxFlowExchangeTransaction(
    BaseAddress::Shared contractorAddress,
    const TransactionUUID &requestedTransactionUUID,
    const SerializedEquivalent receiverEquivalent,
    const vector<SerializedEquivalent> &exchangeEquivalents,
    ContractorsManager *contractorsManager,
    ResourcesManager *resourcesManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    TailManager *tailManager,
    ExchangePathsManager *exchangePathsManager,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
    Logger &logger,
    HopsCount_t hopsCount) :

    BaseCollectTopologyForExchangeTransaction(
        BaseTransaction::FindPathsByMaxFlowExchangeTransaction,
        receiverEquivalent,
        contractorsManager,
        equivalentsSubsystemsRouter,
        exchangeRatesManager,
        tailManager,
        logger),

    mContractorAddress(contractorAddress),
    mRequestedTransactionUUID(requestedTransactionUUID),
    mReceiverEquivalent(receiverEquivalent),
    mExchangeEquivalents(exchangeEquivalents),
    mExchangePathsManager(exchangePathsManager),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager),
    mResourcesManager(resourcesManager),
    mHopsCount(hopsCount)
{
    // IMPORTANT: Use getOrCreateParticipantID instead of contractorIDByAddress
    // to ensure ContractorID consistency between:
    // 1. Topology collection (which uses EquivalentsSubsystemsRouter)
    // 2. Path cache keys (PathCacheKey uses this ContractorID)
    // 3. Coordinator verification (uses same method to get ContractorID)
    //
    // Using contractorIDByAddress would result in mismatched IDs and the coordinator
    // would not find cached paths even after successful path building.
    mContractorID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(contractorAddress);
}

const string FindPathsByMaxFlowExchangeTransaction::logHeader() const
{
    stringstream s;
    s << "[FindPathsByMaxFlowExchangeTransaction: " << currentTransactionUUID() << "] ";
    return s.str();
}

TransactionResult::SharedConst FindPathsByMaxFlowExchangeTransaction::sendRequestForCollectingTopology()
{
    info() << "Requesting topology collection for exchange paths to contractor "
           << mContractorID;

    // Launch subsidiary CollectTopologyForExchangeTransaction to gather network topology.
    // This transaction will:
    // 1. Send MaxFlowCalculationSourceFstLevelMessage to target contractor
    // 2. Collect ResultMaxFlowCalculationMessage responses from network nodes
    // 3. Collect ExchangeRatesMessage for exchange rate information
    // 4. Store all collected data in mContext for processing by fillTopology() and fillRates()
    vector<BaseAddress::Shared> contractorAddresses = {mContractorAddress};

    auto transaction = make_shared<CollectTopologyForExchangeTransaction>(
        mReceiverEquivalent,        // Target equivalent for receiver
        mExchangeEquivalents,       // Sender's available equivalents
        contractorAddresses,        // Target contractor(s)
        mContractorsManager,
        mEquivalentsSubsystemsRouter,
        mExchangeRatesManager,
        mLog,
        mHopsCount);                // Maximum depth of topology traversal

    launchSubsidiaryTransaction(transaction);

    // Initialize retry counter for processCollectingTopology
    mCountProcessCollectingTopologyRun = 0;

    // Wait for initial topology responses (300ms)
    // After this timeout, processCollectingTopology() will be called
    return resultAwakeAfterMilliseconds(kTopologyCollectingMillisecondsTimeout);
}

TransactionResult::SharedConst FindPathsByMaxFlowExchangeTransaction::processCollectingTopology()
{
    info() << "Building exchange paths for contractor " << mContractorID;

    // Save context size before processing to detect if new messages arrived
    auto const contextSize = mContext.size();

    // Process collected topology and exchange rate messages from mContext
    // fillTopology(): Populates TopologyTrustLinesManager with trust line data
    // fillRates(): Populates ExchangeRatesManager with exchange rate data
    fillTopology();
    fillRates();

    // Retry logic: if new messages arrived (contextSize > 0) and we haven't exceeded
    // the maximum retry count, wait again to collect more topology data.
    // This improves path quality by waiting for slower network responses.
    mCountProcessCollectingTopologyRun++;
    if (contextSize > 0 && mCountProcessCollectingTopologyRun <= kCountRunningProcessCollectingTopologyStage) {
        // Wait 200ms and try again (up to 13 times total)
        return resultAwakeAfterMilliseconds(
                   kTopologyCollectingAgainMillisecondsTimeout);
    }

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    debug() << "Collected topology:";
    debug() << "Topology participants:";
    mEquivalentsSubsystemsRouter->printParticipants();
    debug() << "Receiver equivalent: " << mEquivalent;
    mEquivalentsSubsystemsRouter->topologyTrustLineManager(mEquivalent)->printTrustLines();
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        debug() << "Exchange equivalent: " << exchangeEquivalent;
        mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent)->printTrustLines();
    }
    debug() << "Exchange rates:";
    mExchangeRatesManager->printExternalRates();
    debug() << "Participants commissions in receiver equivalent:";
    mEquivalentsSubsystemsRouter->topologyTrustLineManager(mEquivalent)->printCommissions();
    debug() << "Participants commissions in exchange equivalents:";
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent)->printCommissions();
    }
#endif

    try {
        // Use special constant for current node ID in topology calculations
        // This ID is used by TopologyTrustLinesManager to identify the local node
        ContractorID senderID = TopologyTrustLinesManager::kCurrentNodeID;

        // Calculate optimal paths using OR-Tools linear programming solver.
        // calculateMaxFlow() performs:
        // 1. Path enumeration across all exchange equivalents using DFS
        // 2. Linear programming optimization to maximize receiver amount
        // 3. Commission calculations for each path
        // 4. Path sorting by efficiency
        //
        // NOTE: calculateMaxFlow() only RETURNS paths, it does NOT cache them.
        // We must explicitly call storePaths() below.
        auto result = mExchangePathsManager->calculateMaxFlow(
            mContractorID,          // Target contractor for payment
            mReceiverEquivalent,    // Equivalent receiver expects
            mExchangeEquivalents,   // Equivalents sender can use
            senderID,               // Current node (sender)
            mHopsCount);            // Max path length

        info() << "Exchange path building complete, found " << result.optimalPaths.size()
               << " optimal paths with max flow " << result.maxFlow;

        // CRITICAL: Explicitly cache the paths for each sender equivalent.
        // Coordinator expects paths in cache with keys:
        //   PathCacheKey{contractorID, senderEquivalent, receiverEquivalent}
        //
        // We group paths by senderEquivalent (first equivalent in path) and store
        // each group separately. This matches the pattern from
        // InitiateMaxFlowExchangeCalculationTransaction::applyCustomLogic()
        if (!result.optimalPaths.empty()) {
            // Step 1: Group paths by sender equivalent
            map<SerializedEquivalent, vector<OptimalPathResult>> pathsBySenderEq;

            for (const auto &pathResult : result.optimalPaths) {
                if (!pathResult.path().equivalents.empty()) {
                    // First equivalent in path is the sender's equivalent
                    SerializedEquivalent senderEq = pathResult.path().equivalents.front();
                    pathsBySenderEq[senderEq].push_back(pathResult);
                }
            }

            // Step 2: Store each group in cache with appropriate key
            for (const auto &entry : pathsBySenderEq) {
                SerializedEquivalent senderEq = entry.first;
                const vector<OptimalPathResult> &paths = entry.second;

                // Create cache key that coordinator will use to retrieve these paths
                PathCacheKey key{mContractorID, senderEq, mReceiverEquivalent};
                mExchangePathsManager->storePaths(key, paths);

                info() << "Cached " << paths.size() << " paths for key: "
                       << "contractor=" << mContractorID
                       << ", senderEq=" << senderEq
                       << ", receiverEq=" << mReceiverEquivalent;
            }
        }

    } catch (const exception &e) {
        warning() << "Error building exchange paths: " << e.what();
        // Continue despite error - return resource even if no paths found.
        // The coordinator will detect empty cache and return appropriate error
        // to the user (e.g., resultNoPathsError or resultInsufficientFundsError).
    }

    // Create and return ExchangePathsResource to wake up the waiting coordinator.
    // The resource contains only the requesting transaction UUID - the actual paths
    // are already stored in ExchangePathsManager cache.
    //
    // ResourcesManager will:
    // 1. Emit attachResourceSignal with this resource
    // 2. Coordinator will receive the signal via its resource handler
    // 3. Coordinator transitions to runPathsResourceProcessingStage()
    // 4. Coordinator retrieves paths from cache using PathCacheKey
    auto resource = make_shared<ExchangePathsResource>(mRequestedTransactionUUID);
    mResourcesManager->putResource(resource);

    info() << "Returned ExchangePathsResource for transaction "
           << mRequestedTransactionUUID;

    // Transaction complete - coordinator will continue payment execution
    return resultDone();
}
