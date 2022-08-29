#include "CollectTopologyTransaction.h"

CollectTopologyTransaction::CollectTopologyTransaction(
    const SerializedEquivalent equivalent,
    const vector<BaseAddress::Shared> &contractorAddresses,
    ContractorsManager *contractorsManager,
    TrustLinesManager *manager,
    TopologyTrustLinesManager *topologyTrustLineManager,
    TopologyCacheManager *topologyCacheManager,
    MaxFlowCacheManager *maxFlowCacheManager,
    bool iAmGateway,
    Logger &logger,
	HopsCount_t hopsCount) :

    BaseTransaction(
        BaseTransaction::CollectTopologyTransactionType,
        equivalent,
        logger),
    mContractorAddresses(contractorAddresses),
    mContractorsManager(contractorsManager),
    mTrustLinesManager(manager),
    mTopologyTrustLineManager(topologyTrustLineManager),
    mTopologyCacheManager(topologyCacheManager),
    mMaxFlowCacheManager(maxFlowCacheManager),
    mIamGateway(iAmGateway),
    mHopsCnt(hopsCount)
{}

TransactionResult::SharedConst CollectTopologyTransaction::run()
{
    debug() << "Collect topology to " << mContractorAddresses.size() << " contractors";
    // Check if Node does not have outgoing FlowAmount;
    if(mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow().first.empty()){
        return resultDone();
    }
    sendMessagesToContractors();

    if (mIamGateway) {
        for (auto const &nodeAddressAndOutgoingFlow : mTrustLinesManager->outgoingFlows()) {
            auto targetID = mTopologyTrustLineManager->getID(nodeAddressAndOutgoingFlow.first);
            auto trustLineAmountShared = nodeAddressAndOutgoingFlow.second;
            if (mTrustLinesManager->isContractorGateway(targetID)) {
                mTopologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        //TopologyTrustLinesManager::kCurrentNodeID,
						0,
                        targetID,
                        trustLineAmountShared));
                continue;
            }
            if (isNodeListedInTransactionContractors(nodeAddressAndOutgoingFlow.first)) {
                mTopologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        //TopologyTrustLinesManager::kCurrentNodeID,
						0,
                        targetID,
                        trustLineAmountShared));
            }
        }
    } else {
        for (auto const &nodeAddressAndOutgoingFlow : mTrustLinesManager->outgoingFlows()) {
            auto targetID = mTopologyTrustLineManager->getID(nodeAddressAndOutgoingFlow.first);
            auto trustLineAmountShared = nodeAddressAndOutgoingFlow.second;
            mTopologyTrustLineManager->addTrustLine(
                make_shared<TopologyTrustLine>(
                    //TopologyTrustLinesManager::kCurrentNodeID,
					0,
                    targetID,
                    trustLineAmountShared));
        }
    }

    if (mHopsCnt > 0) {
        if (!mTopologyCacheManager->isInitiatorCached()) {
            debug() << "CollectTopologyTransaction: sendMessagesOnFirstLevel";
            sendMessagesOnFirstLevel();
            mTopologyCacheManager->setInitiatorCache();
        }
    }
    return resultDone();
}

void CollectTopologyTransaction::sendMessagesToContractors()
{
	// calc hops count for target;
	//===============================================================
	// [Hops count |  A  |  B   => calc B ]
	// [ 0         |  0  |  0   => (mHopsCnt - 1) = -1 ~> 0 ]
	// [ 1         |  1  |  0   => (mHopsCnt - 1)/2 = 0     ]
	// [ 2         |  1  |  0   => (mHopsCnt - 1)/2 = 0     ]
	// [ 3         |  1  |  1   => (mHopsCnt - 1)/2 = 1     ]
	// [ 4         |  2  |  1   => (mHopsCnt - 1)/2 = 1     ]
	// [ 5         |  2  |  2   => (mHopsCnt - 1)/2 = 2     ]
	//===============================================================

	HopsCount_t target_hops_count = (this->mHopsCnt > 1 ? ((this->mHopsCnt - 1) / 2) : 0);


    for (const auto &contractorAddress : mContractorAddresses)
        sendMessage<InitiateMaxFlowCalculationMessage>(
            contractorAddress,
            mEquivalent,
            mContractorsManager->ownAddresses(),
            mIamGateway,
            target_hops_count);
}

void CollectTopologyTransaction::sendMessagesOnFirstLevel()
{
	// calc hops count for source;
	//===============================================================
	// [Hops count |  A  |  B   => calc B ]
	// [ 0         |  0  |  0   => (mHopsCnt - 1) = -1 ~> 0 ]
	// [ 1         |  1  |  0   => (mHopsCnt - 1)/2 = 0     ]
	// [ 2         |  1  |  0   => (mHopsCnt - 1)/2 = 0     ]
	// [ 3         |  1  |  1   => (mHopsCnt - 1)/2 = 1     ]
	// [ 4         |  2  |  1   => (mHopsCnt - 1)/2 = 1     ]
	// [ 5         |  2  |  2   => (mHopsCnt - 1)/2 = 2     ]
	//===============================================================

	HopsCount_t target_hops_count = (this->mHopsCnt > 1 ? ((this->mHopsCnt - 1) / 2) : 0);
	HopsCount_t source_hops_count = (this->mHopsCnt == 1 ? 1 : (this->mHopsCnt - 1 - target_hops_count));

    if (mIamGateway) {
        auto outgoingFlowIDs = mTrustLinesManager->firstLevelGatewayNeighborsWithOutgoingFlow().first;
        for (auto const &nodeIDOutgoingFlow : outgoingFlowIDs) {
            auto contractorAddress = mContractorsManager->contractorMainAddress(nodeIDOutgoingFlow);
            if (isNodeListedInTransactionContractors(contractorAddress)) {
                continue;
            }
            sendMessage<MaxFlowCalculationSourceFstLevelMessage>(
                nodeIDOutgoingFlow,
                mEquivalent,
                mContractorsManager->idOnContractorSide(nodeIDOutgoingFlow),
				source_hops_count);
        }
    } else {
        auto outgoingFlowIDs = mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow().first;
        auto outgoingFlowIDIt = outgoingFlowIDs.begin();
        while (outgoingFlowIDIt != outgoingFlowIDs.end()) {
            // firstly send message to gateways
            if (mTrustLinesManager->isContractorGateway(*outgoingFlowIDIt)) {
                sendMessage<MaxFlowCalculationSourceFstLevelMessage>(
                    *outgoingFlowIDIt,
                    mEquivalent,
                    mContractorsManager->idOnContractorSide(*outgoingFlowIDIt),
					source_hops_count);
                outgoingFlowIDs.erase(outgoingFlowIDIt);
            } else {
                outgoingFlowIDIt++;
            }
        }
        for (auto const &nodeIDWithOutgoingFlow : outgoingFlowIDs) {
            auto contractorAddress = mContractorsManager->contractorMainAddress(
                nodeIDWithOutgoingFlow);
            if (isNodeListedInTransactionContractors(contractorAddress)) {
                continue;
            }
            sendMessage<MaxFlowCalculationSourceFstLevelMessage>(
                nodeIDWithOutgoingFlow,
                mEquivalent,
                mContractorsManager->idOnContractorSide(nodeIDWithOutgoingFlow),
				source_hops_count);
        }
    }
}

bool CollectTopologyTransaction::isNodeListedInTransactionContractors(
    BaseAddress::Shared nodeAddress) const
{
    for (const auto &contractor : mContractorAddresses) {
        if (nodeAddress == contractor) {
            return true;
        }
    }
    return false;
}

const string CollectTopologyTransaction::logHeader() const
{
    stringstream s;
    s << "[CollectTopologyTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}
