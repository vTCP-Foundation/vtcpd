#include "MaxFlowCalculationSourceSndLevelTransaction.h"

MaxFlowCalculationSourceSndLevelTransaction::MaxFlowCalculationSourceSndLevelTransaction(
    MaxFlowCalculationSourceSndLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationSourceSndLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager)
{}

TransactionResult::SharedConst MaxFlowCalculationSourceSndLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "i am is gateway: " << mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent);
    info() << "sender: " << mMessage->idOnReceiverSide;
    info() << "target: " << mMessage->targetAddresses().at(0)->fullAddress();
#endif
    if (mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent)) {
        sendGatewayResultToInitiator(mEquivalent);
    } else {
        sendResultToInitiator(mEquivalent);
    }
    
    // Send exchange rates if this is an exchange-aware flow
    sendExchangeRatesIfNeeded();

    // Enhanced topology sending: send additional topology for exchange equivalents where rates exist
    if (!mMessage->exchangeEquivalents().empty()) {
        for (const auto& exchangeEquiv : mMessage->exchangeEquivalents()) {
            try {
                auto rate = mExchangeRatesManager->get(mEquivalent, exchangeEquiv);
                if (rate != nullptr) {
                    if(mEquivalentsSubsystemsRouter->iAmGateway(exchangeEquiv))
                        sendGatewayResultToInitiator(exchangeEquiv);
                    else
                        sendResultToInitiator(exchangeEquiv);
                }
            } catch (const NotFoundError&) {
                // No rate found, skip this equivalent
            }
        }
    }
    
    return resultDone();
}

void MaxFlowCalculationSourceSndLevelTransaction::sendResultToInitiator(SerializedEquivalent equivalent)
{
    TopologyCache::Shared maxFlowCalculationCachePtr = mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->cacheByAddress(
            mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr, equivalent);
        return;
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->trustLineIsPresent(initiatorID) and
                *mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlows()) {
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                    outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0)) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
    }
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    const auto incomingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(mMessage->idOnReceiverSide);
    if (*incomingFlow.second.get() > TrustLine::kZeroAmount()) {
        incomingFlows.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlows.size();
    info() << "IncomingFlows: " << incomingFlows.size();
#endif
    if (!outgoingFlows.empty() || !incomingFlows.empty()) {
        Commission::Shared commission = mCommissionsManager->getCommission(equivalent);
        sendMessage<ResultMaxFlowCalculationMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlows,
            incomingFlows,
            commission);
        // todo : add config if cache need
        /*mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->addCache(
            mMessage->targetAddresses().at(0),
            make_shared<TopologyCache>(
                outgoingFlows,
                incomingFlows));*/
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendCachedResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr,
    SerializedEquivalent equivalent)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->trustLineIsPresent(initiatorID) and
                *mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlows()) {
            if (outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
                outgoingFlowsForSending.push_back(
                    outgoingFlow);
            }
        }
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    auto const incomingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
        incomingFlowsForSending.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlowsForSending.size();
    info() << "IncomingFlows: " << incomingFlowsForSending.size();
#endif
    if (!outgoingFlowsForSending.empty() || !incomingFlowsForSending.empty()) {
        Commission::Shared commission = mCommissionsManager->getCommission(equivalent);
        sendMessage<ResultMaxFlowCalculationMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending,
            commission);
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendGatewayResultToInitiator(SerializedEquivalent equivalent)
{
    TopologyCache::Shared maxFlowCalculationCachePtr = mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->cacheByAddress(
            mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedGatewayResultToInitiator(
            maxFlowCalculationCachePtr, equivalent);
        return;
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendGatewayResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->trustLineIsPresent(initiatorID) and
                *mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlowsToGateways()) {
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                    outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0)) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    const auto incomingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(mMessage->idOnReceiverSide);
    if (*incomingFlow.second.get() > TrustLine::kZeroAmount()) {
        incomingFlows.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlows.size();
    info() << "IncomingFlows: " << incomingFlows.size();
#endif
    if (!outgoingFlows.empty() || !incomingFlows.empty()) {
        Commission::Shared commission = mCommissionsManager->getCommission(equivalent);
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlows,
            incomingFlows,
            commission);
        // todo : add config if cache need
        /*mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->addCache(
            mMessage->targetAddresses().at(0),
            make_shared<TopologyCache>(
                outgoingFlows,
                incomingFlows));*/
    }
}

void MaxFlowCalculationSourceSndLevelTransaction::sendCachedGatewayResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr,
    SerializedEquivalent equivalent)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedGatewayResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    bool isSourceFirstLevelNode = false;
    auto initiatorID = mContractorsManager->contractorIDByAddress(mMessage->targetAddresses().at(0));
    if (initiatorID != ContractorsManager::kNotFoundContractorID) {
        if (mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->trustLineIsPresent(initiatorID) and
                *mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(initiatorID).second > TrustLine::kZeroAmount()) {
            isSourceFirstLevelNode = true;
        }
    }
    if (!isSourceFirstLevelNode) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlowsToGateways()) {
            if (outgoingFlow.first != senderMainAddress &&
                    outgoingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
                outgoingFlowsForSending.push_back(
                    outgoingFlow);
            }
        }
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    auto const incomingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
        incomingFlowsForSending.push_back(
            incomingFlow);
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "OutgoingFlows: " << outgoingFlowsForSending.size();
    info() << "IncomingFlows: " << incomingFlowsForSending.size();
#endif
    if (!outgoingFlowsForSending.empty() || !incomingFlowsForSending.empty()) {
        Commission::Shared commission = mCommissionsManager->getCommission(equivalent);
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending,
            commission);
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
    s << "[MaxFlowCalculationSourceSndLevelTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}