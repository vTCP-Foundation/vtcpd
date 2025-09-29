#include "ReceiveMaxFlowCalculationForExchangeOnTargetTransaction.h"

ReceiveMaxFlowCalculationForExchangeOnTargetTransaction::ReceiveMaxFlowCalculationForExchangeOnTargetTransaction(
    InitiateMaxFlowForExchangeCalculationMessage::Shared message,
    ContractorsManager *contractorsManager,
    TrustLinesManager *trustLinesManager,
    TopologyCacheManager *topologyCacheManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::ReceiveMaxFlowCalculationOnTargetTransactionType, // TODO: Add new transaction type
        message->equivalent(),
        logger),
    mMessage(message),
    mExchangeEquivalents(message->exchangeEquivalents()), // Store payer-provided exchange equivalents set
    mContractorsManager(contractorsManager),
    mTrustLinesManager(trustLinesManager),
    mTopologyCacheManager(topologyCacheManager)
{}

TransactionResult::SharedConst ReceiveMaxFlowCalculationForExchangeOnTargetTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "run\t" << "initiator: " << mMessage->senderAddresses.at(0)->fullAddress();
    info() << "Exchange equivalents count: " << mExchangeEquivalents.size();
#endif
    sendResultToInitiator();
    if (mMessage->getHopsCount() > 0) {
        debug() << "ReceiveMaxFlowCalculationForExchangeOnTargetTransaction: sendMessagesOnFirstLevel";
        sendMessagesOnFirstLevel();
    }
    return resultDone();
}

void ReceiveMaxFlowCalculationForExchangeOnTargetTransaction::sendResultToInitiator()
{
    TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
            mMessage->senderAddresses.at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr);
        return;
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    if (mMessage->isSenderGateway()) {
        for (auto const &incomingFlow : mTrustLinesManager->incomingFlowsFromGateways()) {
            if (*incomingFlow.second.get() > TrustLine::kZeroAmount() &&
                    incomingFlow.first != mMessage->senderAddresses.at(0)) {
                incomingFlows.push_back(
                    incomingFlow);
            }
        }
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlowsToGateways()) {
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                    outgoingFlow.first != mMessage->senderAddresses.at(0)) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
    } else {
        for (auto const &incomingFlow : mTrustLinesManager->incomingFlows()) {
            if (*incomingFlow.second.get() > TrustLine::kZeroAmount() &&
                    incomingFlow.first != mMessage->senderAddresses.at(0)) {
                incomingFlows.push_back(
                    incomingFlow);
            }
        }
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlows()) {
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                    outgoingFlow.first != mMessage->senderAddresses.at(0)) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
    }

    sendMessage<ResultMaxFlowCalculationMessage>(
        mMessage->senderAddresses.at(0),
        mEquivalent,
        mContractorsManager->selfContractor()->addresses(),
        outgoingFlows,
        incomingFlows);

    // TODO: Add cache if needed
    /*mTopologyCacheManager->addCache(
        mMessage->senderAddresses.at(0),
        make_shared<TopologyCache>(
            outgoingFlows,
            incomingFlows));*/
}

void ReceiveMaxFlowCalculationForExchangeOnTargetTransaction::sendMessagesOnFirstLevel()
{
    // According to PRD semantics for ReceiveMaxFlowCalculationForExchangeOnTargetTransaction:
    // Collects topology only in mEquivalent and sends MaxFlowCalculationTargetFstLevelMessage with:
    // - equivalent = mEquivalent
    // - exchangeEquivalents = <payer-provided set>
    
    pair<vector<ContractorID>, vector<ContractorID>> incomingFlowIDs;
    if (mMessage->isSenderGateway()) {
        incomingFlowIDs = mTrustLinesManager->firstLevelGatewayNeighborsWithIncomingFlow();
    } else {
        incomingFlowIDs = mTrustLinesManager->firstLevelNeighborsWithIncomingFlow();
    }
    auto initiatorContractorID = mContractorsManager->contractorIDByAddress(
                                     mMessage->senderAddresses.at(0));

    for (auto const &nodeIDWithIncomingFlow : incomingFlowIDs.first) {
        if (nodeIDWithIncomingFlow == initiatorContractorID) {
            continue;
        }
        sendMessage<MaxFlowCalculationTargetFstLevelMessage>(
            nodeIDWithIncomingFlow,
            mEquivalent, // equivalent = mEquivalent
            mContractorsManager->idOnContractorSide(nodeIDWithIncomingFlow),
            mMessage->senderAddresses,
            mMessage->isSenderGateway(),
            mMessage->getHopsCount(),
            mExchangeEquivalents); // exchangeEquivalents = <payer-provided set>
    }
}

void ReceiveMaxFlowCalculationForExchangeOnTargetTransaction::sendCachedResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr)
{
    // TODO: Implement proper cached result handling
    // For now, fall back to regular result sending
    sendResultToInitiator();
}

const string ReceiveMaxFlowCalculationForExchangeOnTargetTransaction::logHeader() const
{
    stringstream s;
    s << "[ReceiveMaxFlowCalculationForExchangeOnTargetTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}