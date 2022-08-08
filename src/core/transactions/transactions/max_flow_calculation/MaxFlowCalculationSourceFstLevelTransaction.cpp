#include "MaxFlowCalculationSourceFstLevelTransaction.h"

MaxFlowCalculationSourceFstLevelTransaction::MaxFlowCalculationSourceFstLevelTransaction(
    MaxFlowCalculationSourceFstLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    TrustLinesManager *trustLinesManager,
    TopologyCacheManager *topologyCacheManager,
    Logger &logger,
    bool iAmGateway) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationSourceFstLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager(contractorsManager),
    mTrustLinesManager(trustLinesManager),
    mTopologyCacheManager(topologyCacheManager),
    mIAmGateway(iAmGateway)
{}

TransactionResult::SharedConst MaxFlowCalculationSourceFstLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "run\t" << "sender: " << mMessage->idOnReceiverSide;
    info() << "run\t" << "i am is gateway: " << mIAmGateway;
    info() << "run\t" << "OutgoingFlows: " << mTrustLinesManager->outgoingFlows().size();
    info() << "run\t" << "IncomingFlows: " << mTrustLinesManager->incomingFlows().size();
#endif
    if (this->mMessage->getHopsCount() == 1) {
		if(mIAmGateway)
			sendGatewayResultToInitiator();
		else
			this->sendResultToInitiator();
        return resultDone();
    }

    pair<vector<ContractorID>, vector<ContractorID>> outgoingFlowIDs;
    if (mIAmGateway) {
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
        outgoingFlowIDs = mTrustLinesManager->firstLevelGatewayNeighborsWithOutgoingFlow();
    } 
	else {
        outgoingFlowIDs = mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow();
    }

	for(auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs.first) {
		if(nodeIDWithOutgoingFlow == mMessage->idOnReceiverSide) {
			continue;
		}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
		info() << "sendFirst: " << nodeIDWithOutgoingFlow;
#endif
		sendMessage<MaxFlowCalculationSourceSndLevelMessage>(
			nodeIDWithOutgoingFlow,
			mEquivalent,
			mContractorsManager->idOnContractorSide(
			nodeIDWithOutgoingFlow),
			mContractorsManager->contractorAddresses(
			mMessage->idOnReceiverSide));
		mTopologyCacheManager->addIntoFirstLevelCache(
			nodeIDWithOutgoingFlow);
	}
	for(auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs.second) {
		if(nodeIDWithOutgoingFlow == mMessage->idOnReceiverSide) {
			continue;
		}
		if(!mTopologyCacheManager->isInFirstLevelCache(nodeIDWithOutgoingFlow)) {
			continue;
		}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
		info() << "sendFirst zero: " << nodeIDWithOutgoingFlow;
#endif
		sendMessage<MaxFlowCalculationSourceSndLevelMessage>(
			nodeIDWithOutgoingFlow,
			mEquivalent,
			mContractorsManager->idOnContractorSide(
			nodeIDWithOutgoingFlow),
			mContractorsManager->contractorAddresses(
			mMessage->idOnReceiverSide));
	}
   
    return resultDone();
}

const string MaxFlowCalculationSourceFstLevelTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationSourceFstLevelTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}

void MaxFlowCalculationSourceFstLevelTransaction::sendResultToInitiator() {
	
	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

	TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
        targetAddresses.at(0));
    
	if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr);
        return;
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendResultToInitiator";
#endif
    
	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;

	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mTrustLinesManager->outgoingFlows()) {
		if(*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
			outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0)) {
			outgoingFlows.push_back(
				outgoingFlow);
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
            targetAddresses.at(0),
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

void MaxFlowCalculationSourceFstLevelTransaction::sendCachedResultToInitiator(TopologyCache::Shared maxFlowCalculationCachePtr) {

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedResultToInitiator";
#endif

	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
   
	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mTrustLinesManager->outgoingFlows()) {
		if(outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0) &&
			!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
			outgoingFlowsForSending.push_back(
				outgoingFlow);
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
            targetAddresses.at(0),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}

void MaxFlowCalculationSourceFstLevelTransaction::sendGatewayResultToInitiator() {

	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

	TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
		targetAddresses.at(0));
	if(maxFlowCalculationCachePtr != nullptr) {
		sendCachedGatewayResultToInitiator(
			maxFlowCalculationCachePtr);
		return;
	}
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
	info() << "sendGatewayResultToInitiator";
#endif
	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;

	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mTrustLinesManager->outgoingFlowsToGateways()) {
		if(*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
			outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0)) {
			outgoingFlows.push_back(
				outgoingFlow);
		}
	}

	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
	const auto incomingFlow = mTrustLinesManager->incomingFlow(mMessage->idOnReceiverSide);
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

void MaxFlowCalculationSourceFstLevelTransaction::sendCachedGatewayResultToInitiator(TopologyCache::Shared maxFlowCalculationCachePtr) {

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
	info() << "sendCachedGatewayResultToInitiator";
#endif
	vector<BaseAddress::Shared> targetAddresses = mContractorsManager->contractorAddresses(
                mMessage->idOnReceiverSide);

	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;

	auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
	for(auto const &outgoingFlow : mTrustLinesManager->outgoingFlowsToGateways()) {
		if(outgoingFlow.first != senderMainAddress &&
			outgoingFlow.first != targetAddresses.at(0) &&
			!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
			outgoingFlowsForSending.push_back(
				outgoingFlow);
		}
	}

	vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
	auto const incomingFlow = mTrustLinesManager->incomingFlow(mMessage->idOnReceiverSide);
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
			mEquivalent,
			mContractorsManager->ownAddresses(),
			outgoingFlowsForSending,
			incomingFlowsForSending);
	}

}
