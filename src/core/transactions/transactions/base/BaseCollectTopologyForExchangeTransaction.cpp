#include "BaseCollectTopologyForExchangeTransaction.h"

BaseCollectTopologyForExchangeTransaction::BaseCollectTopologyForExchangeTransaction(
    const TransactionType type,
    const SerializedEquivalent equivalent,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    TailManager *tailManager,
    Logger &logger) :

    BaseTransaction(
        type,
        equivalent,
        logger),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mExchangeRatesManager(exchangeRatesManager),
    mTailManager(tailManager)
{}

TransactionResult::SharedConst BaseCollectTopologyForExchangeTransaction::run()
{
    switch (mStep) {
    case Stages::SendRequestForCollectingTopology: {
        mStep = Stages::ProcessCollectingTopology;
        return sendRequestForCollectingTopology();
    }
    case Stages::ProcessCollectingTopology: {
        return processCollectingTopology();
    }
    case Stages::CustomLogic:
        return applyCustomLogic();
    default:
        throw ValueError(logHeader() + "::run: "
                                       "wrong value of mStep");
    }
}

void BaseCollectTopologyForExchangeTransaction::fillTopology()
{
    /// Take messages from TailManager instead of BaseTransaction's 'mContext'
    auto &mContext = mTailManager->getFlowTail();

    while (!mContext.empty()) {
        if (mContext.front()->typeID() == Message::MaxFlow_ResultMaxFlowCalculation) {
            const auto kMessage = popNextMessage<ResultMaxFlowCalculationMessage>(mContext);
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
            debug() << "Equivalent " << kMessage->equivalent();
            debug() << "Sender " << kMessage->senderAddresses.at(0)->fullAddress() << " common";
            debug() << "Outgoing flows: " << kMessage->outgoingFlows().size();
            debug() << "Incoming flows: " << kMessage->incomingFlows().size();
            debug() << "ConfirmationID " << kMessage->confirmationID();
#endif
            auto topologyTrustLineManager = mEquivalentsSubsystemsRouter->topologyTrustLineManager(
                                                kMessage->equivalent());
            auto senderID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(kMessage->senderAddresses.at(0));
            for (auto const &outgoingFlow : kMessage->outgoingFlows()) {
                auto targetID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(outgoingFlow.first);
                topologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        senderID,
                        targetID,
                        outgoingFlow.second));
            }
            for (auto const &incomingFlow : kMessage->incomingFlows()) {
                auto sourceID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(incomingFlow.first);
                topologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        sourceID,
                        senderID,
                        incomingFlow.second));
            }
            
            // Store commission if present and amount > 0
            if (kMessage->commission() && kMessage->commission()->amount() > 0) {
                topologyTrustLineManager->storeCommission(
                    senderID,
                    kMessage->equivalent(),
                    kMessage->commission());
            }
        } else if (mContext.front()->typeID() == Message::MaxFlow_ResultMaxFlowCalculationFromGateway) {
            const auto kMessage = popNextMessage<ResultMaxFlowCalculationGatewayMessage>(mContext);
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
            debug() << "Equivalent " << kMessage->equivalent();
            debug() << "Sender " << kMessage->senderAddresses.at(0)->fullAddress() << " gateway";
            debug() << "Outgoing flows: " << kMessage->outgoingFlows().size();
            debug() << "Incoming flows: " << kMessage->incomingFlows().size();
            debug() << "ConfirmationID " << kMessage->confirmationID();
#endif
            auto topologyTrustLineManager = mEquivalentsSubsystemsRouter->topologyTrustLineManager(
                                                kMessage->equivalent());
            auto senderID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(kMessage->senderAddresses.at(0));
            topologyTrustLineManager->addGateway(senderID);
            for (auto const &outgoingFlow : kMessage->outgoingFlows()) {
                auto targetID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(outgoingFlow.first);
                topologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        senderID,
                        targetID,
                        outgoingFlow.second));
            }
            for (auto const &incomingFlow : kMessage->incomingFlows()) {
                auto sourceID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(incomingFlow.first);
                topologyTrustLineManager->addTrustLine(
                    make_shared<TopologyTrustLine>(
                        sourceID,
                        senderID,
                        incomingFlow.second));
            }

            // Store commission if present and amount > 0
            if (kMessage->commission() && kMessage->commission()->amount() > 0) {
                topologyTrustLineManager->storeCommission(
                    senderID,
                    kMessage->equivalent(),
                    kMessage->commission());
            }
            
            if (kMessage->equivalent() == mEquivalent) {
                mGateways.insert(
                    senderID);
            }
        } else {
            warning() << "Invalid message type in context during fill topology";
            mContext.pop_front();
        }
    }
}

void BaseCollectTopologyForExchangeTransaction::fillRates()
{
    /// Process exchange rates messages from TailManager
    auto &exchangeRatesContext = mTailManager->getExchangeRatesTail();

    while (!exchangeRatesContext.empty()) {
        if (exchangeRatesContext.front()->typeID() == Message::MaxFlow_ExchangeRates) {
            const auto kMessage = popNextMessage<ExchangeRatesMessage>(exchangeRatesContext);
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
            debug() << "Processing exchange rates message with " << kMessage->exchangeRates().size() << " rates";
#endif
            // Store external exchange rates in ExchangeRatesManager
            auto senderID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(kMessage->senderAddresses.at(0));
            for (const auto& rate : kMessage->exchangeRates()) {
                mExchangeRatesManager->addOrUpdateExternal(senderID, *rate);
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
                debug() << "Added external rate: " << rate->equivFrom() << " -> " << rate->equivTo()
                        << " rate: " << rate->exchangeRate() << " shift: " << rate->exchangeRateShift();
#endif
            }
        } else {
            warning() << "Invalid message type in exchange rates context";
            exchangeRatesContext.pop_front();
        }
    }
}