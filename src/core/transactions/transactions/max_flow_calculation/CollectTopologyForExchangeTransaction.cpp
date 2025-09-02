#include "CollectTopologyForExchangeTransaction.h"

CollectTopologyForExchangeTransaction::CollectTopologyForExchangeTransaction(
    const SerializedEquivalent equivalent,
    const vector<SerializedEquivalent> &exchangeEquivalents,
    const vector<BaseAddress::Shared> &contractorAddresses,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger,
    HopsCount_t hopsCount) :

    BaseTransaction(
        BaseTransaction::CollectTopologyForExchangeTransactionType,
        equivalent,
        logger),
    mContractorAddresses(contractorAddresses),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mExchangeRatesManager(exchangeRatesManager),
    mHopsCnt(hopsCount)
{
    // Store sender's payment equivalents
    mExchangeEquivalents = exchangeEquivalents;
}

TransactionResult::SharedConst CollectTopologyForExchangeTransaction::run()
{
    debug() << "Collect topology for exchange to " << mContractorAddresses.size() << " contractors";
    // Check if Node does not have outgoing FlowAmount;
    // TODO: remove this check or make chcking for all exchangeEquivalents
    // if(mTrustLinesManager->firstLevelNeighborsWithOutgoingFlow().first.empty()) {
    //     return resultDone();
    // }
    if (mHopsCnt > 0) {
        sendMessagesToContractors();
    }

    for (const auto& exchangeEquivalent : mExchangeEquivalents) {
        bool iAmGatewayForEquivalent = mEquivalentsSubsystemsRouter->iAmGateway(exchangeEquivalent);
        auto trustLinesManager = mEquivalentsSubsystemsRouter->trustLinesManager(exchangeEquivalent);
        auto topologyTrustLineManager = mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent);
        if (iAmGatewayForEquivalent) {    
            for (auto const &nodeAddressAndOutgoingFlow : trustLinesManager->outgoingFlows()) {
                auto targetID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(nodeAddressAndOutgoingFlow.first);
                auto trustLineAmountShared = nodeAddressAndOutgoingFlow.second;
                if (trustLinesManager->isContractorGateway(targetID)) {
                    topologyTrustLineManager->addTrustLine(
                        make_shared<TopologyTrustLine>(
                            0,
                            targetID,
                            trustLineAmountShared));
                    continue;
                }
                if (isNodeListedInTransactionContractors(nodeAddressAndOutgoingFlow.first)) {
                    topologyTrustLineManager->addTrustLine(
                        make_shared<TopologyTrustLine>(
                            0,
                            targetID,
                            trustLineAmountShared));
                }
            }
        } else {
            for (auto const &nodeAddressAndOutgoingFlow : trustLinesManager->outgoingFlows()) {
                auto targetID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(nodeAddressAndOutgoingFlow.first);
                auto trustLineAmountShared = nodeAddressAndOutgoingFlow.second;
                topologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        0,
                        targetID,
                        trustLineAmountShared));
            }
        }
    }

    sendMessagesOnFirstLevel();

    debug() << "topology sent";
    return resultDone();
}

void CollectTopologyForExchangeTransaction::sendMessagesToContractors()
{
    debug() << "Send messages to contractors";

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

    // Send InitiateMaxFlowForExchangeCalculationMessage to contractors according to PRD semantics
    auto senderAddresses = mContractorsManager->ownAddresses();
    //bool iAmGatewayForReceiver = mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent);
    for (const auto& contractorAddress : mContractorAddresses) {
        sendMessage<InitiateMaxFlowForExchangeCalculationMessage>(
            contractorAddress,
            mEquivalent, // Receiver's target equivalent
            senderAddresses,
            //iAmGatewayForReceiver,
            false,
            target_hops_count,
            mExchangeEquivalents); // Sender's payment equivalents
    }
}

void CollectTopologyForExchangeTransaction::sendMessagesOnFirstLevel()
{
    debug() << "Send messages on first level";

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

    // According to PRD: iterate over exchangeEquivalents and send messages per sender equivalent
    for (const auto& exchangeEquivalent : mExchangeEquivalents) {
        auto exchangeEquivTrustLinesManager = mEquivalentsSubsystemsRouter->trustLinesManager(exchangeEquivalent);
        auto neighborsWithFlows = exchangeEquivTrustLinesManager->firstLevelNeighborsWithOutgoingFlow();
        
        for (auto const &nodeIDWithOutgoingFlow : neighborsWithFlows.first) {
            auto nodeAddress = mContractorsManager->contractorMainAddress(nodeIDWithOutgoingFlow);
            if (!isNodeListedInTransactionContractors(nodeAddress)) {
                // PRD: equivalent = exchangeEquivalents[i], exchangeEquivalents = { mEquivalent }
                vector<SerializedEquivalent> receiverEquivalents = { mEquivalent };
                sendMessage<MaxFlowCalculationSourceFstLevelMessage>(
                    nodeAddress,
                    exchangeEquivalent, // Current exchange equivalent
                    mContractorsManager->idOnContractorSide(nodeIDWithOutgoingFlow),
                    source_hops_count,
                    receiverEquivalents); // Single receiver equivalent
            }
        }
    }
}

bool CollectTopologyForExchangeTransaction::isNodeListedInTransactionContractors(
    BaseAddress::Shared nodeAddress) const
{
    for (const auto& contractorAddress : mContractorAddresses) {
        if (nodeAddress == contractorAddress) {
            return true;
        }
    }
    return false;
}

const string CollectTopologyForExchangeTransaction::logHeader() const
{
    stringstream s;
    s << "[CollectTopologyForExchangeTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}