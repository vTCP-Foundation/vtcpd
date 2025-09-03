#include "MaxFlowCalculationSourceFstLevelTransaction.h"

MaxFlowCalculationSourceFstLevelTransaction::MaxFlowCalculationSourceFstLevelTransaction(
    MaxFlowCalculationSourceFstLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationSourceFstLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mExchangeRatesManager(exchangeRatesManager)
{}

TransactionResult::SharedConst MaxFlowCalculationSourceFstLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sender: " << mMessage->idOnReceiverSide;
    info() << "i am is gateway: " << mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent);
    info() << "OutgoingFlows: " << mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->outgoingFlows().size();
    info() << "IncomingFlows: " << mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->incomingFlows().size();
    info() << "Hops count: " << mMessage->getHopsCount();
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

    // Send topology to initiator in case you don't need to extend the request to the 2nd level
    if (this->mMessage->getHopsCount() == 1) {
		if(mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent))
			sendGatewayResultToInitiator(mEquivalent);
		else
			this->sendResultToInitiator(mEquivalent);
        
        return resultDone();
    }

    // Send topology request to 2nd level. in this case it is not required to send topology to initiator, 
    // because we will send topology to initiator in 2nd level transaction.
    pair<vector<ContractorID>, vector<ContractorID>> outgoingFlowIDs;
    if (mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent)) {
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
        // inform that I am is gateway
        // todo : it is not required inform about gateway, because this info initiator can obtain on it side
        auto contractorsAddress = mContractorsManager->contractorMainAddress(
                                      mMessage->idOnReceiverSide);
        sendMessage<ResultMaxFlowCalculationGatewayMessage>(
            contractorsAddress,
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlows,
            incomingFlows);
        outgoingFlowIDs = mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->firstLevelGatewayNeighborsWithOutgoingFlow();
    } 
	else {
        outgoingFlowIDs = mEquivalentsSubsystemsRouter->trustLinesManager(mEquivalent)->firstLevelNeighborsWithOutgoingFlow();
    }

	for(auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs.first) {
		if(nodeIDWithOutgoingFlow == mMessage->idOnReceiverSide) {
			continue;
		}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
		info() << "sendFirst: " << nodeIDWithOutgoingFlow;
#endif
		sendMessage<MaxFlowCalculationSourceSndLevelMessage>(
			mContractorsManager->contractorMainAddress(nodeIDWithOutgoingFlow),
			mEquivalent,
			mContractorsManager->idOnContractorSide(nodeIDWithOutgoingFlow),
			mContractorsManager->contractorAddresses(mMessage->idOnReceiverSide),
			mMessage->exchangeEquivalents());
		mEquivalentsSubsystemsRouter->topologyCacheManager(mEquivalent)->addIntoFirstLevelCache(
			nodeIDWithOutgoingFlow);
	}
	for(auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs.second) {
		if(nodeIDWithOutgoingFlow == mMessage->idOnReceiverSide) {
			continue;
		}
		if(!mEquivalentsSubsystemsRouter->topologyCacheManager(mEquivalent)->isInFirstLevelCache(nodeIDWithOutgoingFlow)) {
			continue;
		}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
		info() << "sendFirst zero: " << nodeIDWithOutgoingFlow;
#endif
		sendMessage<MaxFlowCalculationSourceSndLevelMessage>(
			mContractorsManager->contractorMainAddress(nodeIDWithOutgoingFlow),
			mEquivalent,
			mContractorsManager->idOnContractorSide(nodeIDWithOutgoingFlow),
			mContractorsManager->contractorAddresses(mMessage->idOnReceiverSide),
			mMessage->exchangeEquivalents());
	}
   
    return resultDone();
}

const string MaxFlowCalculationSourceFstLevelTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationSourceFstLevelTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}

void MaxFlowCalculationSourceFstLevelTransaction::sendResultToInitiator(SerializedEquivalent equivalent) {
	
	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

	TopologyCache::Shared maxFlowCalculationCachePtr = mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->cacheByAddress(
        targetAddresses.at(0));
    
	if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr, equivalent);
        return;
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendResultToInitiator";
#endif
    
	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;

	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlows()) {
		if(*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
			outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0)) {
			outgoingFlows.push_back(
				outgoingFlow);
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
        sendMessage<ResultMaxFlowCalculationMessage>(
            targetAddresses.at(0),
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

void MaxFlowCalculationSourceFstLevelTransaction::sendCachedResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr,
    SerializedEquivalent equivalent) {

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedResultToInitiator";
#endif

	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
   
	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlows()) {
		if(outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0) &&
			!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
			outgoingFlowsForSending.push_back(
				outgoingFlow);
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
        sendMessage<ResultMaxFlowCalculationMessage>(
            targetAddresses.at(0),
            equivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}

void MaxFlowCalculationSourceFstLevelTransaction::sendGatewayResultToInitiator(SerializedEquivalent equivalent) {

	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

	TopologyCache::Shared maxFlowCalculationCachePtr = mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent)->cacheByAddress(
		targetAddresses.at(0));
	if(maxFlowCalculationCachePtr != nullptr) {
		sendCachedGatewayResultToInitiator(
			maxFlowCalculationCachePtr, equivalent);
		return;
	}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
	info() << "sendGatewayResultToInitiator";
#endif
	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;

	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlowsToGateways()) {
		if(*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
			outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0)) {
			outgoingFlows.push_back(
				outgoingFlow);
		}
	}

	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
	const auto incomingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(mMessage->idOnReceiverSide);
	if(*incomingFlow.second.get() > TrustLine::kZeroAmount()) {
		incomingFlows.push_back(
			incomingFlow);
	}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
	info() << "OutgoingFlows: " << outgoingFlows.size();
	info() << "IncomingFlows: " << incomingFlows.size();
#endif
	if(!outgoingFlows.empty() || !incomingFlows.empty()) {
		sendMessage<ResultMaxFlowCalculationGatewayMessage>(
			targetAddresses.at(0),
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

void MaxFlowCalculationSourceFstLevelTransaction::sendCachedGatewayResultToInitiator(
    TopologyCache::Shared maxFlowCalculationCachePtr,
    SerializedEquivalent equivalent) {

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
	info() << "sendCachedGatewayResultToInitiator";
#endif
	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;

	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->outgoingFlowsToGateways()) {
		if(outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0) &&
			!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
			outgoingFlowsForSending.push_back(
				outgoingFlow);
		}
	}

	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
	auto const incomingFlow = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent)->incomingFlow(mMessage->idOnReceiverSide);
	if(!maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
		incomingFlowsForSending.push_back(
			incomingFlow);
	}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
	info() << "OutgoingFlows: " << outgoingFlowsForSending.size();
	info() << "IncomingFlows: " << incomingFlowsForSending.size();
#endif
	if(!outgoingFlowsForSending.empty() || !incomingFlowsForSending.empty()) {
		sendMessage<ResultMaxFlowCalculationGatewayMessage>(
			targetAddresses.at(0),
			equivalent,
			mContractorsManager->ownAddresses(),
			outgoingFlowsForSending,
			incomingFlowsForSending);
	}

}

void MaxFlowCalculationSourceFstLevelTransaction::sendExchangeRatesIfNeeded()
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
        vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
            mMessage->idOnReceiverSide);
        
        sendMessage<ExchangeRatesMessage>(
            targetAddresses.at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            ratesToSend);
            
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "Sent " << ratesToSend.size() << " exchange rates to initiator";
#endif
    }
}
