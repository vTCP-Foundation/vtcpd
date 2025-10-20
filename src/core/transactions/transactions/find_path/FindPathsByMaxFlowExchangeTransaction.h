#ifndef VTCPD_FINDPATHSBYMAXFLOWEXCHANGETRANSACTION_H
#define VTCPD_FINDPATHSBYMAXFLOWEXCHANGETRANSACTION_H

#include "../base/BaseCollectTopologyForExchangeTransaction.h"
#include "../../../paths/ExchangePathsManager.h"
#include "../../../rates/manager/ExchangeRatesManager.h"
#include "../../../rates/manager/CommissionsManager.h"
#include "../../../resources/manager/ResourcesManager.h"
#include "../../../resources/resources/ExchangePathsResource.h"

/**
 * @class FindPathsByMaxFlowExchangeTransaction
 * @brief Transaction that collects network topology and builds optimal exchange payment paths on demand.
 *
 * This transaction is triggered by ResourcesManager when CoordinatorExchangePaymentTransaction
 * detects missing or expired exchange paths. Unlike InitiateMaxFlowExchangeCalculationTransaction
 * (which is user-initiated and returns results as command response), this transaction:
 * - Is triggered automatically by the resource management system
 * - Caches built paths in ExchangePathsManager
 * - Returns ExchangePathsResource to notify the requesting coordinator
 * - Does not require paths to be built successfully (returns resource even if no paths found)
 *
 * The transaction reuses topology collection infrastructure from BaseCollectTopologyForExchangeTransaction
 * and OR-Tools path building logic from ExchangePathsManager.
 *
 * Workflow:
 * 1. sendRequestForCollectingTopology(): Launch subsidiary transaction to collect topology
 * 2. processCollectingTopology(): Build paths using OR-Tools and cache them
 * 3. Return ExchangePathsResource to wake up the waiting coordinator
 */
class FindPathsByMaxFlowExchangeTransaction : public BaseCollectTopologyForExchangeTransaction
{
public:
    typedef shared_ptr<FindPathsByMaxFlowExchangeTransaction> Shared;

public:
    /**
     * @brief Constructor for FindPathsByMaxFlowExchangeTransaction
     *
     * @param contractorAddress Target contractor address for path calculation
     * @param requestedTransactionUUID UUID of the coordinator transaction that requested paths
     * @param receiverEquivalent The equivalent in which the receiver expects payment
     * @param exchangeEquivalents List of sender's equivalents to use for payment (max 5)
     * @param contractorsManager Manager for contractor data
     * @param resourcesManager Manager for resource-based communication
     * @param equivalentsSubsystemsRouter Router for accessing equivalent-specific subsystems
     * @param tailManager Manager for message tails
     * @param exchangePathsManager Manager for path calculation and caching
     * @param exchangeRatesManager Manager for exchange rates between equivalents
     * @param commissionsManager Manager for commission calculations
     * @param logger Logger instance
     * @param hopsCount Maximum topology collection depth (number of hops)
     */
    FindPathsByMaxFlowExchangeTransaction(
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
        HopsCount_t hopsCount);

protected:
    /**
     * @brief Returns the log header for this transaction
     * @return Formatted string for log messages
     */
    const string logHeader() const override;

private:
    /**
     * @brief Initiates topology collection for all exchange equivalents
     *
     * Launches CollectTopologyForExchangeTransaction subsidiary transaction to gather
     * network topology data from neighboring nodes. This data is needed for path
     * calculation using OR-Tools.
     *
     * @return Transaction result with timeout to wait for topology responses
     */
    TransactionResult::SharedConst sendRequestForCollectingTopology() override;

    /**
     * @brief Processes collected topology and builds exchange paths
     *
     * This method:
     * 1. Fills topology and exchange rates from collected messages
     * 2. Calls ExchangePathsManager::calculateMaxFlow() to build optimal paths
     * 3. Groups paths by sender equivalent
     * 4. Explicitly caches paths using storePaths() for each PathCacheKey
     * 5. Returns ExchangePathsResource to notify the coordinator
     *
     * The method implements retry logic: if new messages arrive, it waits again
     * to collect more topology data (up to kCountRunningProcessCollectingTopologyStage times).
     *
     * @return Transaction result (done) with ExchangePathsResource
     */
    TransactionResult::SharedConst processCollectingTopology() override;

private:
    // Timeout constants for topology collection

    /// Initial timeout to wait for topology responses (milliseconds)
    static const uint32_t kTopologyCollectingMillisecondsTimeout = 300;

    /// Timeout between retry attempts to collect more topology data (milliseconds)
    static const uint32_t kTopologyCollectingAgainMillisecondsTimeout = 200;

    /// Maximum total time to spend collecting topology (milliseconds)
    static const uint32_t kMaxTopologyCollectingMillisecondsTimeout = 3000;

    /// Maximum number of retry attempts to collect additional topology data
    /// Calculated as: (3000 - 300) / 200 = 13.5 → 13 attempts
    static const uint16_t kCountRunningProcessCollectingTopologyStage =
        (kMaxTopologyCollectingMillisecondsTimeout - kTopologyCollectingMillisecondsTimeout) /
        kTopologyCollectingAgainMillisecondsTimeout;

private:
    // Core transaction data

    /// Target contractor ID (obtained via getOrCreateParticipantID for topology consistency)
    ContractorID mContractorID;

    /// Target contractor address (passed from ResourcesManager request)
    BaseAddress::Shared mContractorAddress;

    /// UUID of the coordinator transaction that requested these paths
    /// Used to route ExchangePathsResource back to the correct coordinator
    TransactionUUID mRequestedTransactionUUID;

    /// Receiver's equivalent (the equivalent in which payment will be received)
    SerializedEquivalent mReceiverEquivalent;

    /// Sender's equivalents (list of equivalents sender can use for payment)
    /// Maximum 5 equivalents as per PRD 06 specification
    vector<SerializedEquivalent> mExchangeEquivalents;

    // Manager dependencies

    /// Manager for calculating and caching exchange paths
    ExchangePathsManager *mExchangePathsManager;

    /// Manager for exchange rates between equivalents
    ExchangeRatesManager *mExchangeRatesManager;

    /// Manager for commission calculations (reserved for future use)
    CommissionsManager *mCommissionsManager;

    /// Manager for returning ExchangePathsResource to coordinator
    ResourcesManager *mResourcesManager;

    // State tracking

    /// Counter for tracking how many times processCollectingTopology has been called
    /// Used for retry logic to collect additional topology data
    size_t mCountProcessCollectingTopologyRun;

    /// Maximum number of hops for topology collection (depth of network traversal)
    HopsCount_t mHopsCount;
};

#endif // VTCPD_FINDPATHSBYMAXFLOWEXCHANGETRANSACTION_H
