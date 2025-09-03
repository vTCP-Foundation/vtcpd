#include "MaxFlowCalculationTargetFstLevelTransaction.h"

MaxFlowCalculationTargetFstLevelTransaction::MaxFlowCalculationTargetFstLevelTransaction(
    MaxFlowCalculationTargetFstLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger,
    bool iAmGateway) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationTargetFstLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager (contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mExchangeRatesManager(exchangeRatesManager),
    mIAmGateway(iAmGateway)
{}

TransactionResult::SharedConst MaxFlowCalculationTargetFstLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() <<  "sender: " << mMessage->idOnReceiverSide;
    info() << "target: " << mMessage->targetAddresses().at(0)->fullAddress();
    info() << "i am is gateway: " << mIAmGateway;
    info() << "OutgoingFlows: " << mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->outgoingFlows().size();
    info() << "IncomingFlows: " << mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->incomingFlows().size();
#endif

    if(mMessage->getHopsCount() < 1 || mMessage->getHopsCount() > 3) {
        return resultDone();
    }

    // Send exchange rates if this is an exchange-aware flow
    sendExchangeRatesIfNeeded();

    // Enhanced topology sending: send additional topology for exchange equivalents where rates exist
    if (!mMessage->exchangeEquivalents().empty()) {
        for (const auto& exchangeEquiv : mMessage->exchangeEquivalents()) {
            try {
                auto rate = mExchangeRatesManager->get(exchangeEquiv, mEquivalent);
                if (rate != nullptr) {
                    if(mIAmGateway)
                        sendGatewayResultToInitiator(exchangeEquiv);
                    else
                        sendResultToInitiator(exchangeEquiv);
                }
            } catch (const NotFoundError&) {
                // No rate found, skip this equivalent
            }
        }
    }

     // Send topology to initiator in case you don't need to extend the request to the 2nd level
    if(mMessage->getHopsCount() == 1) {
        if(mIAmGateway) {
            sendGatewayResultToInitiator(mEquivalent);
        } else {
            sendResultToInitiator(mEquivalent);
        }
        return resultDone();
    }

    // Send topology request to 2nd level. in this case it is not required to send topology to initiator, 
    // because we will send topology to initiator in 2nd level transaction.
    pair<vector<ContractorID>, vector<ContractorID>> incomingFlowIDs;
    if(mIAmGateway) {
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
        // inform that I am is gateway
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlows,
            incomingFlows);
        if(mMessage->isTargetGateway()) {
            incomingFlowIDs = mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->firstLevelGatewayNeighborsWithIncomingFlow();
        } else {
            incomingFlowIDs = mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->firstLevelNeighborsWithIncomingFlow();
        }
    } else {
        incomingFlowIDs = mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->firstLevelNonGatewayNeighborsWithIncomingFlow();
    }

    auto targetContractorID = mContractorsManager->contractorIDByAddress(
                                  mMessage->targetAddresses().at(0));
    for(auto const &nodeIDWithIncomingFlow : incomingFlowIDs.first) {
        if(nodeIDWithIncomingFlow == mMessage->idOnReceiverSide or
                nodeIDWithIncomingFlow == targetContractorID) {
            continue;
        }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "sendFirst: " << nodeIDWithIncomingFlow;
#endif
        sendMessage<MaxFlowCalculationTargetSndLevelMessage>(
            nodeIDWithIncomingFlow,
            mEquivalent,
            mContractorsManager->idOnContractorSide(nodeIDWithIncomingFlow),
            mMessage->targetAddresses(),
            mMessage->isTargetGateway(),
            mMessage->exchangeEquivalents());
        mEquivalentsSubsystemsRouter->topologyCacheManager(mEquivalent)->addIntoFirstLevelCache(
            nodeIDWithIncomingFlow);
    }
    for (auto const &nodeIDWithIncomingFlow : incomingFlowIDs.second) {
        if (nodeIDWithIncomingFlow == mMessage->idOnReceiverSide or
                nodeIDWithIncomingFlow == targetContractorID) {
            continue;
        }
        if (!mEquivalentsSubsystemsRouter->topologyCacheManager(mEquivalent)->isInFirstLevelCache(nodeIDWithIncomingFlow)) {
            continue;
        }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "sendFirst zero: " << nodeIDWithIncomingFlow;
#endif
        sendMessage<MaxFlowCalculationTargetSndLevelMessage>(
            nodeIDWithIncomingFlow,
            mEquivalent,
            mContractorsManager->idOnContractorSide(nodeIDWithIncomingFlow),
            mMessage->targetAddresses(),
            mMessage->isTargetGateway(),
            mMessage->exchangeEquivalents());
    }

    sendExchangeRatesIfNeeded();
    return resultDone();
}

const string MaxFlowCalculationTargetFstLevelTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationTargetFstLevelTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}

void MaxFlowCalculationTargetFstLevelTransaction::sendResultToInitiator(SerializedEquivalent equivalent)
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
    for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlows()) {
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
        sendMessage<ResultMaxFlowCalculationMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
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

void MaxFlowCalculationTargetFstLevelTransaction::sendCachedResultToInitiator(
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
        sendMessage<ResultMaxFlowCalculationMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}

void MaxFlowCalculationTargetFstLevelTransaction::sendGatewayResultToInitiator(SerializedEquivalent equivalent)
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
        for (auto const &incomingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlows()) {
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
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
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

void MaxFlowCalculationTargetFstLevelTransaction::sendExchangeRatesIfNeeded()
{
    if (mMessage->exchangeEquivalents().empty()) {
        return;
    }
    
    vector<ExchangeRate::Shared> ratesToSend;
    
    for (const auto& exchangeEquiv : mMessage->exchangeEquivalents()) {
        try {
            auto rate = mExchangeRatesManager->get(mEquivalent, exchangeEquiv);
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

void MaxFlowCalculationTargetFstLevelTransaction::sendCachedGatewayResultToInitiator(
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
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            mMessage->targetAddresses().at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}
