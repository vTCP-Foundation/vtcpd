#include "InitiateMaxFlowExchangeCalculationTransaction.h"
#include "../../../paths/ExchangePathsManager.h"

InitiateMaxFlowExchangeCalculationTransaction::InitiateMaxFlowExchangeCalculationTransaction(
    InitiateMaxFlowExchangeCalculationCommand::Shared command,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    ExchangePathsManager *exchangePathsManager,
    TailManager *tailManager,
    Logger &logger,
    HopsCount_t hopsCount) :

    BaseCollectTopologyForExchangeTransaction(
        BaseTransaction::InitiateMaxFlowCalculationTransactionType, // TODO: Add new transaction type
        command->equivalent(),
        contractorsManager,
        equivalentsSubsystemsRouter,
        exchangeRatesManager,
        tailManager,
        logger),
    mCommand(command),
    mExchangePathsManager(exchangePathsManager),
    mExchangeEquivalents(command->exchangeEquivalents()),
    mHopsCnt(hopsCount)
{
    // Validate exchangeEquivalents limit (maximum 5 elements)
    if (mExchangeEquivalents.size() > 5) {
        throw ValueError(logHeader() + "::constructor: exchangeEquivalents limit exceeded (maximum 5 elements).");
    }
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::sendRequestForCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "targets count: " << mCommand->contractorAddresses().size();
    info() << "SendRequestForCollectingTopology with exchange equivalents: ";
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        info() << exchangeEquivalent;
    }
#endif
    info() << "contractors addresses:";
    for (const auto &contractor : mCommand->contractorAddresses()) {
        info() << contractor->fullAddress();
    }

    // Compare against all own addresses (by value), avoiding dependence on mainAddress()
    std::vector<BaseAddress::Shared> ownAddresses = mContractorsManager->ownAddresses();
    for (const auto &contractorAddress : mCommand->contractorAddresses()) {
        for (const auto &ownAddr : ownAddresses) {
            if (ownAddr && contractorAddress && contractorAddress->fullAddress() == ownAddr->fullAddress()) {
                warning() << "Attempt to initialise operation against itself was prevented. Canceled.";
                return resultProtocolError();
            }
        }
    }
    
    for (const auto &contractorAddress : mCommand->contractorAddresses()) {
        auto contractorID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
                                contractorAddress);
        info() << "ContractorID " << contractorID << " address " << contractorAddress->fullAddress();
        mContractorIDs.emplace_back(
            contractorID,
            contractorAddress);
    }
    
    auto transaction = make_shared<CollectTopologyForExchangeTransaction>(
        mEquivalent, // Receiver's target equivalent
        mExchangeEquivalents, // Sender's payment equivalents
        mCommand->contractorAddresses(),
        mContractorsManager,
        mEquivalentsSubsystemsRouter,
        mExchangeRatesManager,
        mLog,
        mHopsCnt);
    
    launchSubsidiaryTransaction(transaction);

    mCountProcessCollectingTopologyRun = 0;
    return resultAwakeAfterMilliseconds(
        kWaitMillisecondsForCalculatingMaxFlow); 
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::processCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "ProcessCollectingTopology";
#endif

    auto const contextSize = mContext.size();
    
    fillTopology();
    fillRates(); // Process exchange rates messages

    mCountProcessCollectingTopologyRun++;
    if (contextSize > 0 && mCountProcessCollectingTopologyRun <= kCountRunningProcessCollectingTopologyStage) {
        return resultAwakeAfterMilliseconds(
                   kWaitMillisecondsForCalculatingMaxFlowAgain);
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
    mExchangeRatesManager->printExtqrnalRates();
    debug() << "Participants commissions in receiver equivalent:";
    mEquivalentsSubsystemsRouter->topologyTrustLineManager(mEquivalent)->printCommissions();
    debug() << "Participants commissions in exchange equivalents:";
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent)->printCommissions();
    }
#endif

    mStep = CustomLogic;
    return applyCustomLogic();
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::applyCustomLogic()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "applyCustomLogic";
#endif

    if (!mExchangePathsManager) {
        error() << "ExchangePathsManager is null";
        return resultProtocolError();
    }

    try {
        // Get sender ID
        ContractorID senderID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
            mContractorsManager->selfContractor()->mainAddress());

        // Process each target contractor
        for (const auto &contractor : mContractorIDs) {
            ContractorID contractorID = contractor.first;

            // Calculate max flow using ExchangePathsManager
            auto result = mExchangePathsManager->calculateMaxFlow(
                contractorID,
                mEquivalent,
                mExchangeEquivalents,
                senderID,
                mHopsCnt);

            // Store results
            mMaxFlows[contractorID] = result.maxFlow;
            mOptimalPathResults[contractorID] = result.optimalPaths;

            // Cache the paths for future use
            if (!result.optimalPaths.empty()) {
                map<SerializedEquivalent, vector<OptimalPathResult>> pathsBySenderEq;
                for (const auto &pathResult : result.optimalPaths) {
                    if (!pathResult.path.equivalents.empty()) {
                        SerializedEquivalent senderEq = pathResult.path.equivalents.front();
                        pathsBySenderEq[senderEq].push_back(pathResult);
                    }
                }

                for (const auto &entry : pathsBySenderEq) {
                    SerializedEquivalent senderEq = entry.first;
                    const vector<OptimalPathResult> &paths = entry.second;
                    PathCacheKey key{contractorID, senderEq, mEquivalent};
                    mExchangePathsManager->storePaths(key, paths);
                }
            }
        }

        return resultOk();

    } catch (const std::exception &e) {
        error() << "Max flow calculation failed: " << e.what();
        return resultProtocolError();
    }
}
TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::resultOk()
{
    stringstream ss;
    ss << mMaxFlows.size();
    for (const auto &nodeIDAndMaxFlow : mMaxFlows) {
        for (const auto &nodeIDAndAddress : mContractorIDs) {
            if (nodeIDAndAddress.first == nodeIDAndMaxFlow.first) {
                ss << kTokensSeparator << nodeIDAndAddress.second->typeID()
                   << kTokensSeparator << nodeIDAndAddress.second->fullAddress()
                   << kTokensSeparator << nodeIDAndMaxFlow.second;
                break;
            }
        }
    }
    auto kMaxFlowAmountsStr = ss.str();
    return transactionResultFromCommand(
               mCommand->responseOk(
                   kMaxFlowAmountsStr));
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::resultProtocolError()
{
    return transactionResultFromCommand(
        mCommand->responseProtocolError());
}

const string InitiateMaxFlowExchangeCalculationTransaction::logHeader() const
{
    stringstream s;
    s << "[InitiateMaxFlowExchangeCalculationTransactionTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}
