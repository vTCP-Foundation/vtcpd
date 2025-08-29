#include "CollectTopologyForExchangeTransaction.h"

CollectTopologyForExchangeTransaction::CollectTopologyForExchangeTransaction(
    const SerializedEquivalent equivalent,
    const vector<SerializedEquivalent> &exchangeEquivalents,
    const vector<BaseAddress::Shared> &contractorAddresses,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    TopologyCacheManager *topologyCacheManager,
    MaxFlowCacheManager *maxFlowCacheManager,
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
    mTopologyCacheManager(topologyCacheManager),
    mMaxFlowCacheManager(maxFlowCacheManager),
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
        if (iAmGatewayForEquivalent) {
            auto trustLinesManager = mEquivalentsSubsystemsRouter->trustLinesManager(exchangeEquivalent);
            auto topologyTrustLineManager = mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent);
            
            for (auto const &nodeAddressAndOutgoingFlow : trustLinesManager->outgoingFlows()) {
                auto targetID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(nodeAddressAndOutgoingFlow.first);
                auto trustLineAmountShared = nodeAddressAndOutgoingFlow.second;
                if (trustLinesManager->isContractorGateway(targetID)) {
                    topologyTrustLineManager->addTrustLine(
                        make_shared<TopologyTrustLine>(
                            0,
                            targetID,
                            trustLineAmountShared));
                }
            }
            for (auto const &nodeAddressAndIncomingFlow : trustLinesManager->incomingFlows()) {
                auto sourceID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(nodeAddressAndIncomingFlow.first);
                auto trustLineAmountShared = nodeAddressAndIncomingFlow.second;
                if (trustLinesManager->isContractorGateway(sourceID)) {
                    topologyTrustLineManager->addTrustLine(
                        make_shared<TopologyTrustLine>(
                            sourceID,
                            0,
                            trustLineAmountShared));
                }
            }
        }
    }

    debug() << "topology sent";
    return resultDone();
}

void CollectTopologyForExchangeTransaction::sendMessagesToContractors()
{
    if (mContractorAddresses.empty()) {
        sendMessagesOnFirstLevel();
        return;
    }
    
    // Send InitiateMaxFlowForExchangeCalculationMessage to contractors according to PRD semantics
    auto senderAddresses = mContractorsManager->ownAddresses();
    bool iAmGatewayForReceiver = mEquivalentsSubsystemsRouter->iAmGateway(mEquivalent);
    for (const auto& contractorAddress : mContractorAddresses) {
        if (!isNodeListedInTransactionContractors(contractorAddress)) {
            sendMessage<InitiateMaxFlowForExchangeCalculationMessage>(
                contractorAddress,
                mEquivalent, // Receiver's target equivalent
                senderAddresses,
                iAmGatewayForReceiver,
                mHopsCnt,
                mExchangeEquivalents); // Sender's payment equivalents
        }
    }
    sendMessagesOnFirstLevel();
}

void CollectTopologyForExchangeTransaction::sendMessagesOnFirstLevel()
{
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
                    mHopsCnt,
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