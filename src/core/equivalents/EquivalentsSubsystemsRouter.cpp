#include "EquivalentsSubsystemsRouter.h"

#include <unordered_set>

EquivalentsSubsystemsRouter::EquivalentsSubsystemsRouter(
    StorageHandler *storageHandler,
    Keystore *keystore,
    ContractorsManager *contractorsManager,
    EventsInterfaceManager *eventsInterfaceManager,
    as::io_context &ioCtx,
    vector<SerializedEquivalent> &equivalentsIAmGateway,
    Logger &logger):

    mStorageHandler(storageHandler),
    mKeysStore(keystore),
    mContractorsManager(contractorsManager),
    mEventsInterfaceManager(eventsInterfaceManager),
    mEquivalentsIAmGateway(equivalentsIAmGateway),
    mIOCtx(ioCtx),
    mLogger(logger),
    mHigherFreeID(1)
{
    // Validate input parameters
    if (storageHandler == nullptr) {
        throw ValueError("EquivalentsSubsystemsRouter::constructor: Storage handler cannot be null.");
    }

    if (keystore == nullptr) {
        throw ValueError("EquivalentsSubsystemsRouter::constructor: Keystore cannot be null.");
    }

    if (contractorsManager == nullptr) {
        throw ValueError("EquivalentsSubsystemsRouter::constructor: Contractors manager cannot be null.");
    }

    if (eventsInterfaceManager == nullptr) {
        throw ValueError("EquivalentsSubsystemsRouter::constructor: Events interface manager cannot be null.");
    }

    // Initialize unified participant ID management with current node as ID 0
    mParticipantsAddresses.emplace_back(
        contractorsManager->selfContractor()->mainAddress(), 
        0);

    try {
        // Load equivalents from storage with proper transaction handling
        {
            auto ioTransaction = storageHandler->beginTransaction();
            if (!ioTransaction) {
                throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to begin storage transaction.");
            }
            mEquivalents = ioTransaction->trustLinesHandler()->equivalents();
        }

        // Initialize subsystems for each equivalent
        for (const auto &equivalent : mEquivalents) {
            info() << "Initializing subsystems for equivalent: " << equivalent;

            // Validate equivalent value
            if (equivalent < 0) {
                throw ValueError("EquivalentsSubsystemsRouter::constructor: Invalid equivalent value: " + to_string(equivalent));
            }

            try {
                // Check if this equivalent is a gateway
                bool isGateway = find(
                                     equivalentsIAmGateway.begin(),
                                     equivalentsIAmGateway.end(),
                                     equivalent) != equivalentsIAmGateway.end();

                mIAmGateways.insert(make_pair(equivalent, isGateway));
                debug() << "Gateway status for equivalent " << equivalent << ": " << (isGateway ? "true" : "false");

                // Initialize TrustLinesManager
                auto trustLinesManager = make_unique<TrustLinesManager>(
                                             equivalent,
                                             mStorageHandler,
                                             mKeysStore,
                                             mContractorsManager,
                                             mLogger);
                if (!trustLinesManager) {
                    throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create TrustLinesManager for equivalent " + to_string(equivalent));
                }
                mTrustLinesManagers.insert(make_pair(equivalent, std::move(trustLinesManager)));
                info() << "TrustLinesManager successfully initialized for equivalent: " << equivalent;

                // Initialize TopologyTrustLinesManager
                auto topologyTrustLinesManager = make_unique<TopologyTrustLinesManager>(
                                                     equivalent,
                                                     contractorsManager->selfContractor()->mainAddress(),
                                                     mIAmGateways[equivalent],
                                                     mIOCtx,
                                                     mLogger);
                if (!topologyTrustLinesManager) {
                    throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create TopologyTrustLinesManager for equivalent " + to_string(equivalent));
                }
                mTopologyTrustLinesManagers.insert(make_pair(equivalent, std::move(topologyTrustLinesManager)));
                info() << "TopologyTrustLinesManager successfully initialized for equivalent: " << equivalent;

                // Initialize TopologyCacheManager
                auto topologyCacheManager = make_unique<TopologyCacheManager>(
                                                equivalent,
                                                mLogger);
                if (!topologyCacheManager) {
                    throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create TopologyCacheManager for equivalent " + to_string(equivalent));
                }
                mTopologyCacheManagers.insert(make_pair(equivalent, std::move(topologyCacheManager)));
                info() << "TopologyCacheManager successfully initialized for equivalent: " << equivalent;

                // Initialize MaxFlowCacheManager
                auto maxFlowCacheManager = make_unique<MaxFlowCacheManager>(
                                               equivalent,
                                               mLogger);
                if (!maxFlowCacheManager) {
                    throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create MaxFlowCacheManager for equivalent " + to_string(equivalent));
                }
                mMaxFlowCacheManagers.insert(make_pair(equivalent, std::move(maxFlowCacheManager)));
                info() << "MaxFlowCacheManager successfully initialized for equivalent: " << equivalent;

                // Initialize TopologyCacheUpdateDelayedTask
                auto topologyCacheUpdateTask = make_unique<TopologyCacheUpdateDelayedTask>(
                                                   equivalent,
                                                   mIOCtx,
                                                   mTopologyCacheManagers[equivalent].get(),
                                                   mTopologyTrustLinesManagers[equivalent].get(),
                                                   mMaxFlowCacheManagers[equivalent].get(),
                                                   mLogger);
                if (!topologyCacheUpdateTask) {
                    throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create TopologyCacheUpdateDelayedTask for equivalent " + to_string(equivalent));
                }
                mTopologyCacheUpdateDelayedTasks.insert(make_pair(equivalent, std::move(topologyCacheUpdateTask)));
                info() << "TopologyCacheUpdateDelayedTask successfully initialized for equivalent: " << equivalent;

                // Initialize PathsManager
                auto pathsManager = make_unique<PathsManager>(
                                        equivalent,
                                        mTrustLinesManagers[equivalent].get(),
                                        mTopologyTrustLinesManagers[equivalent].get(),
                                        this,
                                        mLogger);
                if (!pathsManager) {
                    throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create PathsManager for equivalent " + to_string(equivalent));
                }
                mPathsManagers.insert(make_pair(equivalent, std::move(pathsManager)));
                info() << "PathsManager successfully initialized for equivalent: " << equivalent;

            } catch (const std::exception &e) {
                throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to initialize subsystems for equivalent " +
                              to_string(equivalent) + ". Details: " + e.what());
            }
        }

        // Collect contractors that should be pinged
        for (const auto &trustLinesManagerPair : mTrustLinesManagers) {
            try {
                for (const auto &contractorID : trustLinesManagerPair.second->contractorsShouldBePinged()) {
                    mContractorsShouldBePinged.insert(contractorID);
                }
                trustLinesManagerPair.second->clearContractorsShouldBePinged();
            } catch (const std::exception &e) {
                warning() << "Failed to collect contractors to ping for equivalent " << trustLinesManagerPair.first
                          << ". Details: " << e.what();
            }
        }

        // Initialize GatewayNotificationAndRoutingTablesDelayedTask
        mGatewayNotificationAndRoutingTablesDelayedTask = make_unique<GatewayNotificationAndRoutingTablesDelayedTask>(
                true, // enabled
                1,    // updatingTimerPeriodDays
                mIOCtx,
                mLogger);
        if (!mGatewayNotificationAndRoutingTablesDelayedTask) {
            throw IOError("EquivalentsSubsystemsRouter::constructor: Failed to create GatewayNotificationAndRoutingTablesDelayedTask.");
        }
        info() << "GatewayNotificationAndRoutingTablesDelayedTask successfully initialized";

        info() << "EquivalentsSubsystemsRouter successfully initialized with " << mEquivalents.size() << " equivalents";

    } catch (const std::bad_alloc &e) {
        throw IOError("EquivalentsSubsystemsRouter::constructor: Memory allocation failed. Details: " + string(e.what()));
    } catch (const ValueError &e) {
        throw; // Re-throw ValueError as-is
    } catch (const IOError &e) {
        throw; // Re-throw IOError as-is
    } catch (const std::exception &e) {
        throw IOError("EquivalentsSubsystemsRouter::constructor: Unexpected error during initialization. Details: " + string(e.what()));
    }
}

vector<SerializedEquivalent> EquivalentsSubsystemsRouter::equivalents() const
{
    return mEquivalents;
}

bool EquivalentsSubsystemsRouter::iAmGateway(
    const SerializedEquivalent equivalent) const
{
    if (mIAmGateways.count(equivalent) == 0) {
        throw NotFoundError("EquivalentsSubsystemsRouter::iAmGateway: Equivalent not found. Equivalent=" + to_string(equivalent));
    }
    return mIAmGateways.at(equivalent);
}

TrustLinesManager* EquivalentsSubsystemsRouter::trustLinesManager(
    const SerializedEquivalent equivalent) const
{
    if (mTrustLinesManagers.count(equivalent) == 0) {
        throw NotFoundError("EquivalentsSubsystemsRouter::trustLinesManager: TrustLinesManager not found. Equivalent=" + to_string(equivalent));
    }
    return mTrustLinesManagers.at(equivalent).get();
}

TopologyTrustLinesManager* EquivalentsSubsystemsRouter::topologyTrustLineManager(
    const SerializedEquivalent equivalent) const
{
    if (mTopologyTrustLinesManagers.count(equivalent) == 0) {
        throw NotFoundError("EquivalentsSubsystemsRouter::topologyTrustLineManager: TopologyTrustLinesManager not found. Equivalent=" + to_string(equivalent));
    }
    return mTopologyTrustLinesManagers.at(equivalent).get();
}

TopologyCacheManager* EquivalentsSubsystemsRouter::topologyCacheManager(
    const SerializedEquivalent equivalent) const
{
    if (mTopologyCacheManagers.count(equivalent) == 0) {
        throw NotFoundError("EquivalentsSubsystemsRouter::topologyCacheManager: TopologyCacheManager not found. Equivalent=" + to_string(equivalent));
    }
    return mTopologyCacheManagers.at(equivalent).get();
}

MaxFlowCacheManager* EquivalentsSubsystemsRouter::maxFlowCacheManager(
    const SerializedEquivalent equivalent) const
{
    if (mMaxFlowCacheManagers.count(equivalent) == 0) {
        throw NotFoundError("EquivalentsSubsystemsRouter::maxFlowCacheManager: MaxFlowCacheManager not found. Equivalent=" + to_string(equivalent));
    }
    return mMaxFlowCacheManagers.at(equivalent).get();
}

PathsManager* EquivalentsSubsystemsRouter::pathsManager(
    const SerializedEquivalent equivalent) const
{
    if (mPathsManagers.count(equivalent) == 0) {
        throw NotFoundError("EquivalentsSubsystemsRouter::pathsManager: PathsManager not found. Equivalent=" + to_string(equivalent));
    }
    return mPathsManagers.at(equivalent).get();
}

void EquivalentsSubsystemsRouter::initNewEquivalent(
    const SerializedEquivalent equivalent)
{
    // Validate equivalent value
    if (equivalent < 0) {
        throw ValueError("EquivalentsSubsystemsRouter::initNewEquivalent: Invalid equivalent value: " + to_string(equivalent));
    }

    if (mTrustLinesManagers.count(equivalent) != 0) {
        throw ValueError("EquivalentsSubsystemsRouter::initNewEquivalent: Equivalent already exists. Equivalent=" + to_string(equivalent));
    }

    try {
        info() << "Initializing new equivalent: " << equivalent;

        // Check if this equivalent is a gateway
        bool isGateway = find(
                             mEquivalentsIAmGateway.begin(),
                             mEquivalentsIAmGateway.end(),
                             equivalent) != mEquivalentsIAmGateway.end();

        mIAmGateways.insert(make_pair(equivalent, isGateway));
        debug() << "Gateway status for new equivalent " << equivalent << ": " << (isGateway ? "true" : "false");

        // Initialize TrustLinesManager
        auto trustLinesManager = make_unique<TrustLinesManager>(
                                     equivalent,
                                     mStorageHandler,
                                     mKeysStore,
                                     mContractorsManager,
                                     mLogger);
        if (!trustLinesManager) {
            throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to create TrustLinesManager for equivalent " + to_string(equivalent));
        }
        mTrustLinesManagers.insert(make_pair(equivalent, std::move(trustLinesManager)));
        info() << "TrustLinesManager successfully initialized for new equivalent: " << equivalent;

        // Initialize TopologyTrustLinesManager (note: not gateway for new equivalents)
        auto topologyTrustLinesManager = make_unique<TopologyTrustLinesManager>(
                                             equivalent,
                                             mContractorsManager->selfContractor()->mainAddress(),
                                             false, // New equivalents start as non-gateway
                                             mIOCtx,
                                             mLogger);
        if (!topologyTrustLinesManager) {
            throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to create TopologyTrustLinesManager for equivalent " + to_string(equivalent));
        }
        mTopologyTrustLinesManagers.insert(make_pair(equivalent, std::move(topologyTrustLinesManager)));
        info() << "TopologyTrustLinesManager successfully initialized for new equivalent: " << equivalent;

        // Initialize TopologyCacheManager
        auto topologyCacheManager = make_unique<TopologyCacheManager>(
                                        equivalent,
                                        mLogger);
        if (!topologyCacheManager) {
            throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to create TopologyCacheManager for equivalent " + to_string(equivalent));
        }
        mTopologyCacheManagers.insert(make_pair(equivalent, std::move(topologyCacheManager)));
        info() << "TopologyCacheManager successfully initialized for new equivalent: " << equivalent;

        // Initialize MaxFlowCacheManager
        auto maxFlowCacheManager = make_unique<MaxFlowCacheManager>(
                                       equivalent,
                                       mLogger);
        if (!maxFlowCacheManager) {
            throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to create MaxFlowCacheManager for equivalent " + to_string(equivalent));
        }
        mMaxFlowCacheManagers.insert(make_pair(equivalent, std::move(maxFlowCacheManager)));
        info() << "MaxFlowCacheManager successfully initialized for new equivalent: " << equivalent;

        // Initialize TopologyCacheUpdateDelayedTask
        auto topologyCacheUpdateTask = make_unique<TopologyCacheUpdateDelayedTask>(
                                           equivalent,
                                           mIOCtx,
                                           mTopologyCacheManagers[equivalent].get(),
                                           mTopologyTrustLinesManagers[equivalent].get(),
                                           mMaxFlowCacheManagers[equivalent].get(),
                                           mLogger);
        if (!topologyCacheUpdateTask) {
            throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to create TopologyCacheUpdateDelayedTask for equivalent " + to_string(equivalent));
        }
        mTopologyCacheUpdateDelayedTasks.insert(make_pair(equivalent, std::move(topologyCacheUpdateTask)));
        info() << "TopologyCacheUpdateDelayedTask successfully initialized for new equivalent: " << equivalent;

        // Initialize PathsManager
        auto pathsManager = make_unique<PathsManager>(
                                equivalent,
                                mTrustLinesManagers[equivalent].get(),
                                mTopologyTrustLinesManagers[equivalent].get(),
                                this,
                                mLogger);
        if (!pathsManager) {
            throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to create PathsManager for equivalent " + to_string(equivalent));
        }
        mPathsManagers.insert(make_pair(equivalent, std::move(pathsManager)));
        info() << "PathsManager successfully initialized for new equivalent: " << equivalent;

        // Add to equivalents list
        mEquivalents.push_back(equivalent);
        info() << "New equivalent " << equivalent << " successfully initialized and added to router";

    } catch (const std::bad_alloc &e) {
        // Clean up any partially created objects
        mIAmGateways.erase(equivalent);
        mTrustLinesManagers.erase(equivalent);
        mTopologyTrustLinesManagers.erase(equivalent);
        mTopologyCacheManagers.erase(equivalent);
        mMaxFlowCacheManagers.erase(equivalent);
        mTopologyCacheUpdateDelayedTasks.erase(equivalent);
        mPathsManagers.erase(equivalent);

        throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Memory allocation failed for equivalent " +
                      to_string(equivalent) + ". Details: " + e.what());
    } catch (const ValueError &e) {
        throw; // Re-throw ValueError as-is
    } catch (const IOError &e) {
        throw; // Re-throw IOError as-is
    } catch (const std::exception &e) {
        // Clean up any partially created objects
        mIAmGateways.erase(equivalent);
        mTrustLinesManagers.erase(equivalent);
        mTopologyTrustLinesManagers.erase(equivalent);
        mTopologyCacheManagers.erase(equivalent);
        mMaxFlowCacheManagers.erase(equivalent);
        mTopologyCacheUpdateDelayedTasks.erase(equivalent);
        mPathsManagers.erase(equivalent);

        throw IOError("EquivalentsSubsystemsRouter::initNewEquivalent: Failed to initialize equivalent " +
                      to_string(equivalent) + ". Details: " + e.what());
    }
}

set<ContractorID> EquivalentsSubsystemsRouter::contractorsShouldBePinged() const
{
    return mContractorsShouldBePinged;
}

void EquivalentsSubsystemsRouter::clearContractorsShouldBePinged()
{
    mContractorsShouldBePinged.clear();
    debug() << "Contractors should be pinged list cleared";
}

std::vector<ContractorID> EquivalentsSubsystemsRouter::participantsIDs() const
{
    // Pre-size the result container and a guard set to minimise reallocations on large topologies.
    std::vector<ContractorID> ids;
    ids.reserve(mParticipantsAddresses.size());

    std::unordered_set<ContractorID> seen;
    seen.reserve(mParticipantsAddresses.size());

    // Preserve registration order while filtering out potential duplicates defensively.
    for (const auto &entry : mParticipantsAddresses) {
        const auto contractorID = entry.second;
        if (seen.insert(contractorID).second) {
            ids.push_back(contractorID);
        }
    }

    return ids;
}

ContractorID EquivalentsSubsystemsRouter::getOrCreateParticipantID(
    const BaseAddress::Shared &address)
{
    if (!address) {
        throw ValueError("EquivalentsSubsystemsRouter::getOrCreateParticipantID: Address cannot be null");
    }

    // Search for existing address
    for (const auto &participantAddress : mParticipantsAddresses) {
        if (participantAddress.first == address) {
            debug() << "Found existing ContractorID " << participantAddress.second 
                    << " for address " << address->fullAddress();
            return participantAddress.second;
        }
    }

    // Create new entry
    mParticipantsAddresses.emplace_back(address, mHigherFreeID);
    auto result = mHigherFreeID;
    mHigherFreeID++;
    
    info() << "Created new ContractorID " << result 
           << " for address " << address->fullAddress();
    return result;
}

BaseAddress::Shared EquivalentsSubsystemsRouter::resolveParticipantAddress(
    ContractorID contractorID) const
{
    for (const auto &participant : mParticipantsAddresses) {
        if (participant.second == contractorID) {
            return participant.first;
        }
    }
    return nullptr;
}

BaseAddress::Shared EquivalentsSubsystemsRouter::getParticipantAddress(
    ContractorID contractorID) const
{
    for (const auto &participant : mParticipantsAddresses) {
        if (participant.second == contractorID) {
            return participant.first;
        }
    }
    throw NotFoundError(
        "EquivalentsSubsystemsRouter::getParticipantAddress: Participant not found. ContractorID=" + 
        to_string(contractorID));
}

optional<ContractorID> EquivalentsSubsystemsRouter::resolveParticipantID(
    const BaseAddress::Shared &address) const
{
    if (!address) {
        return std::nullopt;
    }

    for (const auto &participantAddress : mParticipantsAddresses) {
        if (participantAddress.first == address) {
            return participantAddress.second;
        }
    }
    return std::nullopt;
}

void EquivalentsSubsystemsRouter::sendTopologyEvent() const
{
    debug() << "Sending topology events for all equivalents";

    try {
        if (mTrustLinesManagers.empty()) {
            // Send empty topology event when no trust lines managers exist
            try {
                vector<BaseAddress::Shared> emptyVector;
                mEventsInterfaceManager->writeEvent(
                    Event::topologyEvent(
                        mContractorsManager->selfContractor()->mainAddress(),
                        emptyVector,
                        0));
                debug() << "Empty topology event sent successfully";
            } catch (const std::exception &e) {
                warning() << "Failed to send empty topology event. Details: " << e.what();
            }
            return;
        }

        for (const auto &trustLineManagerPair : mTrustLinesManagers) {
            const auto &equivalent = trustLineManagerPair.first;
            const auto &trustLineManager = trustLineManagerPair.second;

            try {
                auto neighbors = trustLineManager->firstLevelNeighborsAddresses();
                debug() << "Processing topology event for equivalent " << equivalent
                        << " with " << neighbors.size() << " neighbors";

                auto neighborIt = neighbors.begin();
                auto previousBegin = neighborIt;

                // Send neighbors in portions to avoid overwhelming the event system
                while (neighborIt != neighbors.end()) {
                    if (neighborIt - previousBegin >= kTopologyEventPortionSize) {
                        vector<BaseAddress::Shared> portionNeighbors;
                        copy(previousBegin, neighborIt, back_inserter(portionNeighbors));

                        try {
                            mEventsInterfaceManager->writeEvent(
                                Event::topologyEvent(
                                    mContractorsManager->selfContractor()->mainAddress(),
                                    portionNeighbors,
                                    equivalent));
                            debug() << "Topology event portion sent for equivalent " << equivalent
                                    << " with " << portionNeighbors.size() << " neighbors";
                        } catch (const std::exception &e) {
                            warning() << "Failed to send topology event portion for equivalent " << equivalent
                                      << ". Details: " << e.what();
                        }
                        previousBegin = neighborIt;
                    } else {
                        neighborIt++;
                    }
                }

                // Send remaining neighbors
                vector<BaseAddress::Shared> remainingNeighbors;
                copy(previousBegin, neighborIt, back_inserter(remainingNeighbors));

                try {
                    mEventsInterfaceManager->writeEvent(
                        Event::topologyEvent(
                            mContractorsManager->selfContractor()->mainAddress(),
                            remainingNeighbors,
                            equivalent));
                    debug() << "Final topology event portion sent for equivalent " << equivalent
                            << " with " << remainingNeighbors.size() << " neighbors";
                } catch (const std::exception &e) {
                    warning() << "Failed to send final topology event portion for equivalent " << equivalent
                              << ". Details: " << e.what();
                }

            } catch (const std::exception &e) {
                warning() << "Failed to process topology event for equivalent " << equivalent
                          << ". Details: " << e.what();
            }
        }

        debug() << "Topology event processing completed for " << mTrustLinesManagers.size() << " equivalents";

    } catch (const std::exception &e) {
        error() << "Critical error in sendTopologyEvent. Details: " << e.what();
    }
}

#ifdef TESTS
void EquivalentsSubsystemsRouter::setMeAsGateway()
{
    for (auto &iAmGatewayPair : mIAmGateways) {
        iAmGatewayPair.second = true;
    }
    debug() << "All equivalents set as gateways for testing";
}
#endif

string EquivalentsSubsystemsRouter::logHeader() const
{
    return "EquivalentsSubsystemsRouter";
}

LoggerStream EquivalentsSubsystemsRouter::error() const
{
    return mLogger.error(logHeader());
}

LoggerStream EquivalentsSubsystemsRouter::warning() const
{
    return mLogger.warning(logHeader());
}

LoggerStream EquivalentsSubsystemsRouter::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream EquivalentsSubsystemsRouter::debug() const
{
    return mLogger.debug(logHeader());
}

void EquivalentsSubsystemsRouter::printParticipants() const
{
    // Print mapping of ContractorID to address for diagnostics
    debug() << "Participants dump begin";
    if (mParticipantsAddresses.empty()) {
        debug() << "No participants registered";
        return;
    }

    for (const auto &entry : mParticipantsAddresses) {
        const auto &address = entry.first;
        const ContractorID contractorID = entry.second;
        // Use fullAddress() to get printable representation of BaseAddress
        debug() << "contractorID=" << contractorID << " address=" << address->fullAddress();
    }

    debug() << "Participants dump end";
}
