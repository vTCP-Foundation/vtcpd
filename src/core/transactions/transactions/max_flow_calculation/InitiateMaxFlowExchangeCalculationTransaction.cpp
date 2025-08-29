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
    mShortMaxFlowsCalculated(false),
    mIamGateway(equivalentsSubsystemsRouter->iAmGateway(command->equivalent()))
{
    // Validate exchangeEquivalents limit (maximum 5 elements)
    if (mExchangeEquivalents.size() > 5) {
        throw ValueError(logHeader() + "::constructor: exchangeEquivalents limit exceeded (maximum 5 elements).");
    }
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::sendRequestForCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "run\t" << "targets count: " << mCommand->contractorAddresses().size();
    info() << "SendRequestForCollectingTopology with exchange equivalents: " << mExchangeEquivalents.size();
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

    // According to PRD: just launch CollectTopologyForExchangeTransaction
    // mEquivalent is receiver's target equivalent
    // mExchangeEquivalents are sender's payment equivalents
    auto receiverCacheManager = mEquivalentsSubsystemsRouter->topologyCacheManager(mEquivalent);
    auto receiverMaxFlowCacheManager = mEquivalentsSubsystemsRouter->maxFlowCacheManager(mEquivalent);
    
    auto transaction = make_shared<CollectTopologyForExchangeTransaction>(
        mEquivalent, // Receiver's target equivalent
        mExchangeEquivalents, // Sender's payment equivalents
        mCommand->contractorAddresses(),
        mContractorsManager,
        mEquivalentsSubsystemsRouter,
        mExchangeRatesManager,
        receiverCacheManager,
        receiverMaxFlowCacheManager,
        mLog,
        mHopsCnt);
    
    launchSubsidiaryTransaction(transaction);

    return resultAwakeAfterMilliseconds(10000); // TODO: Use proper timeout mechanism
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::processCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "ProcessCollectingTopology";
#endif
    
    fillTopology();
    fillRates(); // Process exchange rates messages

    if (mContext.empty()) {
        debug() << "No messages received. Waiting for responses.";
        return resultAwakeAfterMilliseconds(5000); // TODO: Use proper timeout mechanism
    }

    mStep = CustomLogic;
    return resultDone();
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::applyCustomLogic()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "applyCustomLogic";
#endif
    
    // TODO: Implement OR-Tools Linear Programming optimization
    // For now, use simplified calculation similar to existing implementation
    mCurrentGlobalContractorIdx = 0;
    mMaxPathLength = kShortMaxPathLength;

    for (const auto &contractor : mContractorIDs) {
        mCurrentContractor = contractor.first;
        mCurrentMaxFlow = calculateMaxFlow(mCurrentContractor);
        mMaxFlows[mCurrentContractor] = mCurrentMaxFlow;

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "Contractor: " << mCurrentContractor << " max flow: " << mCurrentMaxFlow;
#endif
    }

    return resultFinalOk();
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

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::resultFinalOk()
{
    stringstream ss;
    ss << kFinalStep << kTokensSeparator;
    
    for (const auto &contractorResult : mMaxFlows) {
        ss << contractorResult.first << kTokensSeparator;
        ss << contractorResult.second << kTokensSeparator;
    }

    auto result = ss.str();
    return transactionResultFromCommand(
        mCommand->responseOk(result));
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::resultIntermediateOk()
{
    stringstream ss;
    ss << mResultStep << kTokensSeparator;
    
    for (const auto &contractorResult : mMaxFlows) {
        ss << contractorResult.first << kTokensSeparator;
        ss << contractorResult.second << kTokensSeparator;
    }

    auto result = ss.str();
    return transactionResultFromCommand(
        mCommand->responseOk(result));
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