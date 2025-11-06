#ifndef VTCPD_EQUIVALENTSSUBSYSTEMSROUTER_H
#define VTCPD_EQUIVALENTSSUBSYSTEMSROUTER_H

#include "../trust_lines/manager/TrustLinesManager.h"
#include "../delayed_tasks/TopologyCacheUpdateDelayedTask.h"
#include "../paths/PathsManager.h"
#include "../interface/events_interface/interface/EventsInterfaceManager.h"
#include "../delayed_tasks/GatewayNotificationAndRoutingTablesDelayedTask.h"
#include "../common/exceptions/ValueError.h"
#include "../common/exceptions/IOError.h"
#include "../common/exceptions/NotFoundError.h"

#include <map>
#include <memory>
#include <algorithm>
#include <utility>
#include <optional>

namespace as = boost::asio;
namespace signals = boost::signals2;

using std::optional;

/**
 * Routes and manages subsystems for different equivalents in the network.
 *
 * Provides centralized management of trust lines, topology cache, max flow cache,
 * and paths managers for each equivalent. Ensures proper initialization and
 * resource management for all equivalent-specific subsystems.
 */
class EquivalentsSubsystemsRouter
{

public:
    /**
     * Constructs router and initializes all subsystems for existing equivalents.
     *
     * @param storageHandler Database storage handler (must not be null)
     * @param keystore Cryptographic key storage (must not be null)
     * @param contractorsManager Network contractors manager (must not be null)
     * @param eventsInterfaceManager Event publishing interface (must not be null)
     * @param ioCtx Boost.Asio IO context for async operations
     * @param equivalentsIAmGateway List of equivalents where this node acts as gateway
     * @param logger Logger instance for debugging and error reporting
     *
     * @throws ValueError if any required parameter is null or invalid
     * @throws IOError if database operations or subsystem initialization fails
     */
    EquivalentsSubsystemsRouter(
        StorageHandler *storageHandler,
        Keystore *keystore,
        ContractorsManager *contractorsManager,
        EventsInterfaceManager *eventsInterfaceManager,
        as::io_context &ioCtx,
        vector<SerializedEquivalent> &equivalentsIAmGateway,
        Logger &logger);

    /**
     * Returns list of all managed equivalents.
     */
    vector<SerializedEquivalent> equivalents() const;

    /**
     * Checks if this node acts as gateway for specified equivalent.
     *
     * @param equivalent Target equivalent ID
     * @return true if node is gateway for this equivalent
     * @throws NotFoundError if equivalent not found
     */
    bool iAmGateway(
        const SerializedEquivalent equivalent) const;

    /**
     * Returns trust lines manager for specified equivalent.
     *
     * @param equivalent Target equivalent ID
     * @return Pointer to trust lines manager
     * @throws NotFoundError if equivalent not found
     */
    TrustLinesManager* trustLinesManager(
        const SerializedEquivalent equivalent) const;

    /**
     * Returns topology trust lines manager for specified equivalent.
     *
     * @param equivalent Target equivalent ID
     * @return Pointer to topology trust lines manager
     * @throws NotFoundError if equivalent not found
     */
    TopologyTrustLinesManager* topologyTrustLineManager(
        const SerializedEquivalent equivalent) const;

    /**
     * Returns topology cache manager for specified equivalent.
     *
     * @param equivalent Target equivalent ID
     * @return Pointer to topology cache manager
     * @throws NotFoundError if equivalent not found
     */
    TopologyCacheManager* topologyCacheManager(
        const SerializedEquivalent equivalent) const;

    /**
     * Returns max flow cache manager for specified equivalent.
     *
     * @param equivalent Target equivalent ID
     * @return Pointer to max flow cache manager
     * @throws NotFoundError if equivalent not found
     */
    MaxFlowCacheManager* maxFlowCacheManager(
        const SerializedEquivalent equivalent) const;

    /**
     * Returns paths manager for specified equivalent.
     *
     * @param equivalent Target equivalent ID
     * @return Pointer to paths manager
     * @throws NotFoundError if equivalent not found
     */
    PathsManager* pathsManager(
        const SerializedEquivalent equivalent) const;

    /**
     * Initializes all subsystems for new equivalent with proper error handling.
     *
     * @param equivalent New equivalent ID to initialize
     * @throws ValueError if equivalent already exists or is invalid
     * @throws IOError if subsystem initialization fails
     */
    void initNewEquivalent(
        const SerializedEquivalent equivalent);

    /**
     * Returns set of contractors that should be pinged.
     */
    set<ContractorID> contractorsShouldBePinged() const;

    /**
     * Clears the list of contractors that should be pinged.
     */
    void clearContractorsShouldBePinged();

    /**
     * Sends topology events for all managed equivalents with error handling.
     */
    void sendTopologyEvent() const;

    /**
     * Returns unified ContractorID for given address, creating new ID if needed.
     * 
     * This provides a single source of truth for ContractorID assignment across
     * all equivalents, ensuring consistent participant identification.
     *
     * @param address Base address to resolve or assign ContractorID for
     * @return ContractorID for the address (existing or newly created)
     */
    /**
     * Returns a snapshot of all known participant IDs without exposing internal containers.
     *
     * The IDs are returned in the deterministic order in which participants were registered,
     * starting with the current node (ID 0). The method never duplicates IDs, even if the
     * backing storage contained redundant entries due to future refactors.
     */
    std::vector<ContractorID> participantsIDs() const;

    ContractorID getOrCreateParticipantID(
        const BaseAddress::Shared &address);

    /**
     * Resolves ContractorID to BaseAddress if known.
     *
     * @param contractorID ContractorID to resolve  
     * @return BaseAddress for the contractor, or nullptr if not found
     */
    BaseAddress::Shared resolveParticipantAddress(
        ContractorID contractorID) const;

    /**
     * Returns BaseAddress for given ContractorID.
     *
     * @param contractorID ContractorID to lookup
     * @return BaseAddress for the contractor
     * @throws NotFoundError if ContractorID not found in participants
     */
    BaseAddress::Shared getParticipantAddress(
        ContractorID contractorID) const;

    /**
     * Finds ContractorID for given address without creating new entry.
     *
     * @param address Base address to lookup
     * @return ContractorID if found, or nullopt if address is unknown
     */
    optional<ContractorID> resolveParticipantID(
        const BaseAddress::Shared &address) const;

#ifdef TESTS
    /**
     * Sets this node as gateway for all equivalents (testing only).
     */
    void setMeAsGateway();
#endif

    /**
     * Prints participants mapping to the log for debugging purposes.
     *
     * Outputs every pair of BaseAddress and ContractorID stored in
     * mParticipantsAddresses using the debug() logger stream.
     */
    void printParticipants() const;

protected:
    string logHeader() const;

    LoggerStream warning() const;

    LoggerStream error() const;

    LoggerStream info() const;

    LoggerStream debug() const;

private:
    static const uint32_t kTopologyEventPortionSize = 100;

private:
    map<SerializedEquivalent, bool> mIAmGateways;
    as::io_context &mIOCtx;
    StorageHandler *mStorageHandler;
    Keystore *mKeysStore;
    ContractorsManager *mContractorsManager;
    EventsInterfaceManager *mEventsInterfaceManager;
    Logger &mLogger;
    vector<SerializedEquivalent> mEquivalents;
    map<SerializedEquivalent, unique_ptr<TrustLinesManager>> mTrustLinesManagers;
    map<SerializedEquivalent, unique_ptr<TopologyTrustLinesManager>> mTopologyTrustLinesManagers;
    map<SerializedEquivalent, unique_ptr<TopologyCacheManager>> mTopologyCacheManagers;
    map<SerializedEquivalent, unique_ptr<TopologyCacheUpdateDelayedTask>> mTopologyCacheUpdateDelayedTasks;
    map<SerializedEquivalent, unique_ptr<MaxFlowCacheManager>> mMaxFlowCacheManagers;
    map<SerializedEquivalent, unique_ptr<PathsManager>> mPathsManagers;
    vector<SerializedEquivalent> mEquivalentsIAmGateway;
    unique_ptr<GatewayNotificationAndRoutingTablesDelayedTask> mGatewayNotificationAndRoutingTablesDelayedTask;

    set<ContractorID> mContractorsShouldBePinged;

    // Unified participant ID management across all equivalents
    vector<pair<BaseAddress::Shared, ContractorID>> mParticipantsAddresses;
    ContractorID mHigherFreeID;
};


#endif //VTCPD_EQUIVALENTSSUBSYSTEMSROUTER_H
