#include "MaxFlowCalculationSourceSndLevelTransaction.h"

MaxFlowCalculationSourceSndLevelTransaction::MaxFlowCalculationSourceSndLevelTransaction(
    MaxFlowCalculationSourceSndLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    TrustLinesManager *manager,
    TopologyCacheManager *topologyCacheManager,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger,
    bool iAmGateway) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationSourceSndLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager(contractorsManager),
    mTrustLinesManager(manager),
    mTopologyCacheManager(topologyCacheManager),
    mExchangeRatesManager(exchangeRatesManager),
    mIAmGateway(iAmGateway)
{}

TransactionResult::SharedConst MaxFlowCalculationSourceSndLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "i am is gateway: " << mIAmGateway;
    info() << "sender: " << mMessage->idOnReceiverSide;
    info() << "target: " << mMessage->targetAddresses().at(0)->fullAddress();
#endif
    if (mIAmGateway) {
        sendGatewayResultToInitiator();
    } else {
        sendResultToInitiator();
    }
    
    // Send exchange rates if this is an exchange-aware flow
    sendExchangeRatesIfNeeded();
    return resultDone();
}

void MaxFlowCalculationSourceSndLevelTransaction::sendResultToInitiator()
{
    TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
            mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr);
        return;
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mTrustLinesManager->trustLineIsPresent(initiatorID) and
                *mTrustLinesManager->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlows()) {
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                    outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0)) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
    }
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    const auto incomingFlow = mTrustLinesManager->incomingFlow(mMessage->idOnReceiverSide);
    if (*incomingFlow.second.get() > TrustLine::kZeroAmount()) {
        incomingFlows.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlows.size();
    info() << "IncomingFlows: " << incomingFlows.size();
#endif
    if (!outgoingFlows.empty() || !incomingFlows.empty()) {
        sendMessage<ResultMaxFlowCalculationMessage>(
            mMessage->targetAddresses().at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlows,
            incomingFlows);
        // todo : add config if cache need
        /*mTopologyCacheManager->addCache(
            mMessage->targetAddresses().at(0),
            make_shared<TopologyCache>(
                outgoingFlows,
                incomingFlows));*/
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendCachedResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mTrustLinesManager->trustLineIsPresent(initiatorID) and
                *mTrustLinesManager->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlows()) {
            if (outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
                outgoingFlowsForSending.push_back(
                    outgoingFlow);
            }
        }
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    auto const incomingFlow = mTrustLinesManager->incomingFlow(mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
        incomingFlowsForSending.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlowsForSending.size();
    info() << "IncomingFlows: " << incomingFlowsForSending.size();
#endif
    if (!outgoingFlowsForSending.empty() || !incomingFlowsForSending.empty()) {
        sendMessage<ResultMaxFlowCalculationMessage>(
            mMessage->targetAddresses().at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendGatewayResultToInitiator()
{
    TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
            mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedGatewayResultToInitiator(
            maxFlowCalculationCachePtr);
        return;
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendGatewayResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mTrustLinesManager->trustLineIsPresent(initiatorID) and
                *mTrustLinesManager->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlowsToGateways()) {
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                    outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0)) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    const auto incomingFlow = mTrustLinesManager->incomingFlow(mMessage->idOnReceiverSide);
    if (*incomingFlow.second.get() > TrustLine::kZeroAmount()) {
        incomingFlows.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlows.size();
    info() << "IncomingFlows: " << incomingFlows.size();
#endif
    if (!outgoingFlows.empty() || !incomingFlows.empty()) {
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlows,
            incomingFlows);
        // todo : add config if cache need
        /*mTopologyCacheManager->addCache(
            mMessage->targetAddresses().at(0),
            make_shared<TopologyCache>(
                outgoingFlows,
                incomingFlows));*/
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendCachedGatewayResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedGatewayResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mTrustLinesManager->trustLineIsPresent(initiatorID) and
                *mTrustLinesManager->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlowsToGateways()) {
            if (outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
                outgoingFlowsForSending.push_back(
                    outgoingFlow);
            }
        }
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    auto const incomingFlow = mTrustLinesManager->incomingFlow(mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
        incomingFlowsForSending.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlowsForSending.size();
    info() << "IncomingFlows: " << incomingFlowsForSending.size();
#endif
    if (!outgoingFlowsForSending.empty() || !incomingFlowsForSending.empty()) {
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendExchangeRatesIfNeeded()
{
    // According to PRD semantics for Source-side transactions:
    // When exchangeEquivalents non-empty, search local ExchangeRatesManager for rates matching pairs mEquivalent/exchangeEquivalents[i]
    // Send found rates via ExchangeRatesMessage alongside topology data
    
    if (mMessage->exchangeEquivalents().empty()) {
        // Legacy single-equivalent flow, no exchange rates to send
        return;
    }

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "Sending exchange rates for " << mMessage->exchangeEquivalents().size() << " exchange equivalents";
#endif

    vector<ExchangeRate::Shared> ratesToSend;
    
    // Search for exchange rates from mEquivalent to each exchangeEquivalent
    for (const auto& exchangeEquiv : mMessage->exchangeEquivalents()) {
        try {
            auto rate = mExchangeRatesManager->get(mEquivalent, exchangeEquiv);
            if (rate != nullptr) {
                ratesToSend.push_back(rate);
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
                info() << "Found local rate: " << mEquivalent << " -> " << exchangeEquiv;
#endif
            }
        } catch (const NotFoundError&) {
            // No rate found, continue to next equivalent
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
            debug() << "No local rate found for: " << mEquivalent << " -> " << exchangeEquiv;
#endif
        }
    }
    
    if (!ratesToSend.empty()) {
        sendMessage<ExchangeRatesMessage>(
            mMessage->targetAddresses().at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            ratesToSend);
            
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "Sent " << ratesToSend.size() << " exchange rates to initiator";
#endif
    }
}

const string MaxFlowCalculationSourceSndLevelTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationSourceSndLevelTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}