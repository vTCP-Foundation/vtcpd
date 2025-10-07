#include "CoordinatorExchangePaymentTransaction.h"

CoordinatorExchangePaymentTransaction::CoordinatorExchangePaymentTransaction(
    const CreditUsageExchangeCommand::Shared command,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangePathsManager *exchangePathsManager,
    Keystore *keystore,
    bool isPaymentTransactionsAllowedDueToObserving,
    EventsInterfaceManager *eventsInterfaceManager,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        BaseTransaction::CoordinatorPaymentTransaction,
        command->equivalent(),
        contractorsManager,
        equivalentsSubsystemsRouter,
        storageHandler,
        resourcesManager,
        keystore,
        log,
        subsystemsController),
    mExchangePathsManager(exchangePathsManager),
    mCommand(command),
    mAmount(command->amount()),
    mCommandUUID(command->UUID()),
    mContractorAddresses(command->contractorAddresses()),
    mExchangeEquivalents(command->exchangeEquivalents()),
    mEventsInterfaceManager(eventsInterfaceManager),
    mIsPaymentTransactionsAllowedDueToObserving(isPaymentTransactionsAllowedDueToObserving)
{
    // Get ContractorID from first address
    if (!mContractorAddresses.empty()) {
        try {
            mContractorID = mContractorsManager->contractorIDByAddress(mContractorAddresses[0]);
        } catch (...) {
            // Will be set later when contractor is resolved
        }
    }
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::run()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::run not yet implemented");
}

const CommandUUID &CoordinatorExchangePaymentTransaction::commandUUID() const
{
    return mCommandUUID;
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runPaymentInitializationStage()
{
    debug() << "runPaymentInitializationStage";

    // Send ReceiverInitPaymentRequestMessage to receiver
    sendMessage<ReceiverInitPaymentRequestMessage>(
        mContractorAddresses[0],  // receiver address
        mEquivalent,
        mContractorsManager->ownAddresses(),  // sender addresses
        currentTransactionUUID(),
        mAmount,
        mPayload);

    // Wait for receiver response with reduced timeout
    // NO paths resource request here
    return resultWaitForMessageTypes(
        {Message::Payments_ReceiverInitPaymentResponse},
        maxNetworkDelay(4));  // Reduced from 10 to 4
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runPathsResourceProcessingStage()
{
    debug() << "runPathsResourceProcessingStage";

    // Step 1: Initialize total flow counter
    TrustLineAmount totalAddedFlow = TrustLineAmount(0);

    // Step 2: Iterate through each sender equivalent
    for (const auto& senderEquiv : mExchangeEquivalents) {
        // Step 3: Create cache key for this sender-receiver equivalent combination
        PathCacheKey key{
            mContractorID,
            senderEquiv,
            mEquivalent  // receiver equivalent
        };

        // Step 4: Retrieve optimal paths from ExchangePathsManager
        auto optimalPaths = mExchangePathsManager->retrievePaths(key);

        if (!optimalPaths) {
            // No paths available for this equivalent combination
            debug() << "No cached paths for sender equiv " << senderEquiv;
            continue;
        }

        // Step 5: Add paths until total flow >= payment amount
        for (const auto& pathResult : *optimalPaths) {
            if (totalAddedFlow >= mAmount) {
                break;  // Sufficient flow accumulated
            }

            // Step 6: Add path to mPathsStats
            addPathForFurtherProcessing(pathResult);
            totalAddedFlow = totalAddedFlow + pathResult.received_amount;
        }

        // Check if we have enough flow
        if (totalAddedFlow >= mAmount) {
            break;  // No need to check other equivalents
        }
    }

    // Step 7: Validate that we have sufficient paths
    if (totalAddedFlow < mAmount) {
        warning() << "Insufficient total flow: " << totalAddedFlow
                  << " < " << mAmount;
        return transactionResultFromCommand(
            mCommand->responseProtocolError());
    }

    // Step 8: Wait for receiver response
    return resultWaitForMessageTypes(
        {Message::Payments_ReceiverInitPaymentResponse},
        maxNetworkDelay(4));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runReceiverRequestProcessingStage()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runReceiverRequestProcessingStage not yet implemented");
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runReceiverResponseProcessingStage()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runReceiverResponseProcessingStage not yet implemented");
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runAmountReservationStage()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runAmountReservationStage not yet implemented");
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runDirectAmountReservationResponseProcessingStage()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runDirectAmountReservationResponseProcessingStage not yet implemented");
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runFinalAmountsConfigurationConfirmation()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runFinalAmountsConfigurationConfirmation not yet implemented");
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runVotesConsistencyCheckingStage()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runVotesConsistencyCheckingStage not yet implemented");
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runTTLTransactionResponse()
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::runTTLTransactionResponse not yet implemented");
}

void CoordinatorExchangePaymentTransaction::savePaymentOperationIntoHistory(
    IOTransaction::Shared ioTransaction)
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::savePaymentOperationIntoHistory not yet implemented");
}

bool CoordinatorExchangePaymentTransaction::checkReservationsDirections() const
{
    throw RuntimeError("CoordinatorExchangePaymentTransaction::checkReservationsDirections not yet implemented");
}

void CoordinatorExchangePaymentTransaction::addPathForFurtherProcessing(
    const OptimalPathResult& pathResult)
{
    debug() << "addPathForFurtherProcessing";

    // Step 1: Initialize mIntermediateNodesStates
    size_t pathLength = pathResult.mPath.ids.size();

    // Create mutable copy to initialize states
    auto pathCopy = make_unique<OptimalPathResult>(pathResult);

    pathCopy->mIntermediateNodesStates.clear();
    pathCopy->mIntermediateNodesStates.resize(
        pathLength,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent);

    // Step 2: Initialize nodes vector in ExchangePath (ContractorID → BaseAddress conversion)
    pathCopy->mPath.nodes.clear();
    pathCopy->mPath.nodes.reserve(pathLength);

    for (const auto& contractorID : pathResult.mPath.ids) {
        auto contractor = mContractorsManager->contractor(contractorID);
        if (!contractor) {
            throw ValueError(
                "CoordinatorExchangePaymentTransaction::addPathForFurtherProcessing: "
                "Contractor not found for ID: " + to_string(contractorID));
        }
        pathCopy->mPath.nodes.push_back(contractor->mainAddress());
    }

    // Step 3: Generate unique PathID
    PathID pathID = generateNextPathID();

    // Step 4: Add to mPathsStats
    mPathsStats[pathID] = std::move(pathCopy);

    debug() << "Path " << pathID << " added with "
            << pathLength << " nodes, flow: " << pathResult.optimal_flow;
}

PathID CoordinatorExchangePaymentTransaction::generateNextPathID()
{
    // Simple incrementing ID generator
    // If no paths exist yet, start with 1
    if (mPathsStats.empty()) {
        return 1;
    }

    // Find max existing PathID and increment
    PathID maxID = 0;
    for (const auto& [pathID, pathResult] : mPathsStats) {
        if (pathID > maxID) {
            maxID = pathID;
        }
    }

    return maxID + 1;
}
