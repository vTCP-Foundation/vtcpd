#include "InitiateMaxFlowExchangeCalculationTransaction.h"

InitiateMaxFlowExchangeCalculationTransaction::InitiateMaxFlowExchangeCalculationTransaction(
    InitiateMaxFlowExchangeCalculationCommand::Shared command,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
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
    mHopsCnt(hopsCount),
    mCommand(command),
    mExchangeEquivalents(command->exchangeEquivalents()),
    mResultStep(1),
    mGatewayResponseProcessed(false),
    mShortMaxFlowsCalculated(false)
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

    auto ownAddress = mContractorsManager->selfContractor()->mainAddress();
    for (const auto &contractorAddress : mCommand->contractorAddresses()) {
        if (contractorAddress == ownAddress) {
            warning() << "Attempt to initialise operation against itself was prevented. Canceled.";
            return resultProtocolError();
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
    
    // TODO: Implement OR-Tools Linear Programming optimization
    // For now, use simplified calculation similar to existing implementation
    mCurrentGlobalContractorIdx = 0;
    mMaxPathLength = kMaxPathLength;

    for (const auto &contractor : mContractorIDs) {
        mCurrentContractor = contractor.first;
        mCurrentMaxFlow = calculateMaxFlow(mCurrentContractor);
        mMaxFlows[mCurrentContractor] = mCurrentMaxFlow;

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "Contractor: " << mCurrentContractor << " max flow: " << mCurrentMaxFlow;
#endif
    }

    return resultOk();
}

TrustLineAmount InitiateMaxFlowExchangeCalculationTransaction::calculateMaxFlow(
    ContractorID contractorID)
{
    // Simplified calculation - in a full implementation, this would use OR-Tools
    // to solve the multi-equivalent linear programming optimization problem
    
    TrustLineAmount maxFlow = TrustLineAmount(0);
    
    // Basic topology traversal to estimate max flow
    // TODO: Replace with proper OR-Tools LP optimization
    
    return maxFlow;
}

void InitiateMaxFlowExchangeCalculationTransaction::calculateMaxFlowOnOneLevel()
{
    // TODO: Implement multi-equivalent flow calculation on one level
}

TrustLineAmount InitiateMaxFlowExchangeCalculationTransaction::calculateOneNode(
    ContractorID nodeID,
    const TrustLineAmount &currentFlow,
    byte_t level)
{
    // TODO: Implement single node calculation for exchange-aware flow
    return TrustLineAmount(0);
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
    s << "[InitiateMaxFlowExchangeCalculationTransactionTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}