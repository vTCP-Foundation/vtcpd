#include "MaxFlowCalculationTargetFstLevelTransaction.h"

MaxFlowCalculationTargetFstLevelTransaction::MaxFlowCalculationTargetFstLevelTransaction(
    MaxFlowCalculationTargetFstLevelMessage::Shared message,
    ContractorsManager *contractorsManager,
    TrustLinesManager *manager,
    TopologyCacheManager *topologyCacheManager,
    Logger &logger,
    bool iAmGateway) :

    BaseTransaction(
        BaseTransaction::MaxFlowCalculationTargetFstLevelTransactionType,
        message->equivalent(),
        logger),
    mMessage(message),
    mContractorsManager (contractorsManager),
    mTrustLinesManager(manager),
    mTopologyCacheManager(topologyCacheManager),
    mIAmGateway(iAmGateway)
{}

TransactionResult::SharedConst MaxFlowCalculationTargetFstLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "run\t" << "sender: " << mMessage->idOnReceiverSide;
    info() << "run\t" << "target: " << mMessage->targetAddresses().at(0)->fullAddress();
    info() << "run\t" << "i am is gateway: " << mIAmGateway;
    info() << "run\t" << "OutgoingFlows: " << mTrustLinesManager->outgoingFlows().size();
    info() << "run\t" << "IncomingFlows: " << mTrustLinesManager->incomingFlows().size();
#endif

	if(mMessage->getHopsCount() < 1 || mMessage->getHopsCount() > 3) {
		return resultDone();
	}

	if(mMessage->getHopsCount() == 1) {
		if(mIAmGateway) {
			sendGatewayResultToInitiator();
		}
		else {
			sendResultToInitiator();
		}
		return resultDone();
	}

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
				incomingFlowIDs = mTrustLinesManager->firstLevelGatewayNeighborsWithIncomingFlow();
			}
			else {
				incomingFlowIDs = mTrustLinesManager->firstLevelNeighborsWithIncomingFlow();
			}
		}
		else {
			incomingFlowIDs = mTrustLinesManager->firstLevelNonGatewayNeighborsWithIncomingFlow();
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
				mMessage->isTargetGateway());
			mTopologyCacheManager->addIntoFirstLevelCache(
				nodeIDWithIncomingFlow);
		}
		for(auto const &nodeIDWithIncomingFlow : incomingFlowIDs.second) {
			if(nodeIDWithIncomingFlow == mMessage->idOnReceiverSide or
				nodeIDWithIncomingFlow == targetContractorID) {
				continue;
			}
			if(!mTopologyCacheManager->isInFirstLevelCache(nodeIDWithIncomingFlow)) {
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
				mMessage->isTargetGateway());
		}

    return resultDone();
}

const string MaxFlowCalculationTargetFstLevelTransaction::logHeader() const
{
    stringstream s;
    s << "[MaxFlowCalculationTargetFstLevelTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}

void MaxFlowCalculationTargetFstLevelTransaction::sendResultToInitiator() {
	#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendResultToInitiator";
#endif
    TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
        mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedResultToInitiator(
            maxFlowCalculationCachePtr);
        return;
    }
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    auto const outgoingFlow = mTrustLinesManager->outgoingFlow(
        mMessage->idOnReceiverSide);
    if (*outgoingFlow.second.get() > TrustLine::kZeroAmount()) {
        outgoingFlows.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
    for (auto const &incomingFlow : mTrustLinesManager->incomingFlows()) {
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

void MaxFlowCalculationTargetFstLevelTransaction::sendCachedResultToInitiator(TopologyCache::Shared maxFlowCalculationCachePtr) {
	#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    auto const outgoingFlow = mTrustLinesManager->outgoingFlow(
        mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
        outgoingFlowsForSending.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
    for (auto const &incomingFlow : mTrustLinesManager->incomingFlowsFromNonGateways()) {
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
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}

void MaxFlowCalculationTargetFstLevelTransaction::sendGatewayResultToInitiator() {
	#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendGatewayResultToInitiator";
#endif
    TopologyCache::Shared maxFlowCalculationCachePtr = mTopologyCacheManager->cacheByAddress(
        mMessage->targetAddresses().at(0));
    if (maxFlowCalculationCachePtr != nullptr) {
        sendCachedGatewayResultToInitiator(
            maxFlowCalculationCachePtr);
        return;
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
    auto const outgoingFlow = mTrustLinesManager->outgoingFlow(
        mMessage->idOnReceiverSide);
    if (*outgoingFlow.second.get() > TrustLine::kZeroAmount()) {
        outgoingFlows.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
    if (mMessage->isTargetGateway()) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mTrustLinesManager->incomingFlows()) {
            if (*incomingFlow.second.get() > TrustLine::kZeroAmount() &&
                    incomingFlow.first != senderMainAddress &&
                    incomingFlow.first != mMessage->targetAddresses().at(0)) {
                incomingFlows.push_back(
                    incomingFlow);
            }
        }
    } else {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mTrustLinesManager->incomingFlows()) {
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

void MaxFlowCalculationTargetFstLevelTransaction::sendCachedGatewayResultToInitiator(TopologyCache::Shared maxFlowCalculationCachePtr) {
	#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "sendCachedGatewayResultToInitiator";
#endif
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlowsForSending;
    auto const outgoingFlow = mTrustLinesManager->outgoingFlow(
        mMessage->idOnReceiverSide);
    if (!maxFlowCalculationCachePtr->containsOutgoingFlow(outgoingFlow.first, outgoingFlow.second)) {
        outgoingFlowsForSending.push_back(
            outgoingFlow);
    }

    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlowsForSending;
    if (mMessage->isTargetGateway()) {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mTrustLinesManager->incomingFlowsFromGateways()) {
            if (incomingFlow.first != senderMainAddress &&
                    incomingFlow.first != mMessage->targetAddresses().at(0) &&
                    !maxFlowCalculationCachePtr->containsIncomingFlow(incomingFlow.first, incomingFlow.second)) {
                incomingFlowsForSending.push_back(
                    incomingFlow);
            }
        }
    } else {
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &incomingFlow : mTrustLinesManager->incomingFlows()) {
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
            mEquivalent,
            mContractorsManager->ownAddresses(),
            outgoingFlowsForSending,
            incomingFlowsForSending);
    }
}
