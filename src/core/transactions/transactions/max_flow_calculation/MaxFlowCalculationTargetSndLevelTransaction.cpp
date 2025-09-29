#include "MaxFlowCalculationTargetSndLevelTransaction.h"

MaxFlowCalculationTargetSndLevelTransaction::MaxFlowCalculationTargetSndLevelTransaction(
    MaxFlowCalculationTargetSndLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationTargetSndLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager)
{}

TransactionResult::SharedConst MaxFlowCalculationTargetSndLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sender: " << mMessage->idOnReceiverSide;
    info() << "target: " << mMessage->targetAddresses().at(0)->fullAddress();
    info() << "i am is gateway: " << mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent);
#endif

    if (mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent)) {
        sendGatewayResultToInitiator(mEquivalent);
    } else {
        sendResultToInitiator(mEquivalent);
    }

    // Send exchange rates if this is an exchange-aware flow
    sendExchangeRatesIfNeeded();

    // Enhanced topology sending: send additional topology for exchange equivalents where rates exist// Enhanced topology sending: send additional topology for exchange equivalents where rates exist
    if (!mMessage->exchangeEquivalents().empty()) {
        for (const auto& exchangeEquiv : mMessage->exchangeEquivalents()) {
            try {
                auto rate = mExchangeRatesManager->get(exchangeEquiv, mEquivalent);
                if (rate != nullptr) {
                    if(mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent))
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

void MaxFlowCalculationTargetSndLevelTransaction::sendResultToInitiator(SerializedEquivalent equivalent)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendResultToInitiator";
#endif
    TopologyCache::Shared maxFlowCalculationCachePtr = mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->cacheByAddress(
            mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr, equivalent);
        return;
    }
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    auto const outgoingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlow(
                                  mMessage->idOnReceiverSide);
    if (*outgoingFlow.second.get() > TrustLine::kZeroAmount()) {
        outgoingFlows.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
    for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlowsFromNonGateways()) {
        if (*incomingFlow.second.get() > TrustLine::kZeroAmount() &&
                incomingFlow.first != senderMainAddress &&
                incomingFlow.first != mMessage->targetAddresses().at(0)) {
            incomingFlows.push_back(
                incomingFlow);
        }
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
        /*mTopologyCacheManager->addCache(
            mMessage->targetAddresses().at(0),
            make_shared<TopologyCache>(
                outgoingFlows,
                incomingFlows));*/
    }
}

void MaxFlowCalculationTargetSndLevelTransaction::sendCachedResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr,
    SerializedEquivalent equivalent)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    auto const outgoingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlow(
                                  mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
        outgoingFlowsForSending.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
    for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlowsFromNonGateways()) {
        if (incomingFlow.first != senderMainAddress &&
                incomingFlow.first != mMessage->targetAddresses().at(0) &&
                !maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
            incomingFlowsForSending.push_back(
                incomingFlow);
        }
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

void MaxFlowCalculationTargetSndLevelTransaction::sendGatewayResultToInitiator(SerializedEquivalent equivalent)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendGatewayResultToInitiator";
#endif
    TopologyCache::Shared maxFlowCalculationCachePtr = mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->cacheByAddress(
            mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedGatewayResultToInitiator(
            maxFlowCalculationCachePtr, equivalent);
        return;
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    auto const outgoingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlow(
                                  mMessage->idOnReceiverSide);
    if (*outgoingFlow.second.get() > TrustLine::kZeroAmount()) {
        outgoingFlows.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    if (mMessage->isTargetGateway()) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlowsFromGateways()) {
            if (*incomingFlow.second.get() > TrustLine::kZeroAmount() &&
                    incomingFlow.first != senderMainAddress &&
                    incomingFlow.first != mMessage->targetAddresses().at(0)) {
                incomingFlows.push_back(
                    incomingFlow);
            }
        }
    } else {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlows()) {
            if (*incomingFlow.second.get() > TrustLine::kZeroAmount() &&
                    incomingFlow.first != senderMainAddress &&
                    incomingFlow.first != mMessage->targetAddresses().at(0)) {
                incomingFlows.push_back(
                    incomingFlow);
            }
        }
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
        /*mTopologyCacheManager->addCache(
            mMessage->targetAddresses().at(0),
            make_shared<TopologyCache>(
                outgoingFlows,
                incomingFlows));*/
    }
}

void MaxFlowCalculationTargetSndLevelTransaction::sendCachedGatewayResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr,
    SerializedEquivalent equivalent)
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedGatewayResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    auto const outgoingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlow(
                                  mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
        outgoingFlowsForSending.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    if (mMessage->isTargetGateway()) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlowsFromGateways()) {
            if (incomingFlow.first != senderMainAddress &&
                    incomingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
                incomingFlowsForSending.push_back(
                    incomingFlow);
            }
        }
    } else {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlows()) {
            if (incomingFlow.first != senderMainAddress &&
                    incomingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
                incomingFlowsForSending.push_back(
                    incomingFlow);
            }
        }
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

void MaxFlowCalculationTargetSndLevelTransaction::sendExchangeRatesIfNeeded()
{
    if (mMessage->exchangeEquivalents().empty()) {
        return;
    }
    
    vector<ExchangeRate::Shared> ratesToSend;
    
    for (const auto& exchangeEquiv : mMessage->exchangeEquivalents()) {
        try {
            auto rate = mExchangeRatesManager->get(exchangeEquiv, mEquivalent);
            if (rate != nullptr) {
                ratesToSend.push_back(rate);
            }
        } catch (NotFoundError&) {
        }
    }
    
    if (!ratesToSend.empty()) {
        auto targetAddress = mMessage->targetAddresses().at(0);
        sendMessage<ExchangeRatesMessage>(
            targetAddress,
            mEquivalent,
            mContractorsManager->ownAddresses(),
            ratesToSend);
    }
}

const string MaxFlowCalculationTargetSndLevelTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationTargetSndLevelTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}
