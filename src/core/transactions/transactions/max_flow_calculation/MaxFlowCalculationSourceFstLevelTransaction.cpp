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
    mContractorsManager (contractorsManager),
    mTrustLinesManager(trustLinesManager),
    mTopologyCacheManager(topologyCacheManager),
    mIAmGateway(iAmGateway),
    mHopsCnt(0)
{}

TransactionResult::SharedConst MaxFlowCalculationSourceFstLevelTransaction::run()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "run\t" << "sender: " << mMessage->idOnReceiverSide;
    info() << "run\t" << "i am is gateway: " << mIAmGateway;
    info() << "run\t" << "OutgoingFlows: " << mTrustLinesManager->outgoingFlows().size();
    info() << "run\t" << "IncomingFlows: " << mTrustLinesManager->incomingFlows().size();
#endif
    if (mHopsCnt == 0) {
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> outgoingFlows;
        auto senderMainAddress = mContractorsManager->contractorMainAddress(mMessage->idOnReceiverSide);
        for (auto const &outgoingFlow : mTrustLinesManager->outgoingFlowsToGateways()) {
            info() << "outgoingFlow to gateway " << *outgoingFlow.second.get() << " " << outgoingFlow.first->fullAddress();
            if (*outgoingFlow.second.get() > TrustLine::kZeroAmount() &&
                outgoingFlow.first != senderMainAddress) {
                outgoingFlows.push_back(
                    outgoingFlow);
            }
        }
        vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> incomingFlows;
        if (!outgoingFlows.empty() || !incomingFlows.empty()) {
            sendMessage<ResultMaxFlowCalculationGatewayMessage>(
                senderMainAddress,
                mEquivalent,
                mContractorsManager->ownAddresses(),
                outgoingFlows,
                incomingFlows);
            mTopologyCacheManager->addCache(
                senderMainAddress,
                make_shared<TopologyCache>(
                    outgoingFlows,
                    incomingFlows));
        }
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
    } else {
        outgoingFlowIDs = mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow();
    }
    for (auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs.first) {
        if (nodeIDWithOutgoingFlow == mMessage->idOnReceiverSide) {
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
    for (auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs.second) {
        if (nodeIDWithOutgoingFlow == mMessage->idOnReceiverSide) {
            continue;
        }
        if (!mTopologyCacheManager->isInFirstLevelCache(nodeIDWithOutgoingFlow)) {
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
