#include "OptimalPathResult.h"

#include <algorithm>

/**
 * Increases node state to the "state".
 *
 * @param positionInPath - position of the node, which state must be increased.
 * @param state - new state of the node that should be remembered.
 */
void OptimalPathResult::setNodeState(
    const SerializedPositionInPath positionInPath,
    const OptimalPathResult::NodeState state)
{
#ifdef INTERNAL_ARGUMENTS_VALIDATION
    assert(positionInPath >= 0);
    assert(positionInPath < mIntermediateNodesStates.size());
#endif

    mIntermediateNodesStates[positionInPath] = state;
}

/**
 * @returns current max flow of the path,
 * that was calculated due to amount reservation process.
 */
const TrustLineAmount& OptimalPathResult::maxFlow() const
{
    return mMaxPathFlow;
}

ExchangePath& OptimalPathResult::path()
{
    return mPath;
}

const ExchangePath& OptimalPathResult::path() const
{
    return mPath;
}

bool OptimalPathResult::containsIntermediateNodes() const
{
    return !mIntermediateNodesStates.empty();
}

/**
 * @returns node uuid (and it's position in the path),
 * from which reservation response must be received.
 *
 * @throws NotFoundError in case if no currently processed node,
 * or last in the path is already processed.
 */
const pair<BaseAddress::Shared, SerializedPositionInPath> OptimalPathResult::currentIntermediateNodeAndPos() const
{
    for (SerializedPositionInPath idx = 0; idx < mIntermediateNodesStates.size(); ++idx)
        if (mIntermediateNodesStates[idx] != OptimalPathResult::ReservationApproved &&
                mIntermediateNodesStates[idx] != OptimalPathResult::ReservationRejected)
            return make_pair(mPath.intermediates()[idx], idx);

    throw NotFoundError(
        "OptimalPathResult::currentIntermediateNodeAndPos: "
        "no unprocessed nodes are left.");
}

/**
 * @returns node to which amount reservation request wasn't set yet.
 * Amount reservation request may be both "CoordinatorReservationRequest" and "IntermediateNodeReservationRequest".
 * (this method ensures both requests are sent to the node)
 *
 * Also, node position (relative to the source node) would be returned.
 *
 * @throws NotFoundError - in case if all nodes of this path are already processed.
 */
const pair<BaseAddress::Shared, SerializedPositionInPath> OptimalPathResult::nextIntermediateNodeAndPos() const
{
    for (SerializedPositionInPath idx = 0; idx < mIntermediateNodesStates.size(); ++idx) {
        if (0 == idx &&
                mIntermediateNodesStates[idx] == OptimalPathResult::NeighbourReservationApproved) {
            return make_pair(mPath.intermediates()[idx], idx);
        }

        if (mIntermediateNodesStates[idx] == OptimalPathResult::ReservationRequestDoesntSent) {
            return make_pair(mPath.intermediates()[idx], idx);
        }
    }

    throw NotFoundError(
        "OptimalPathResult::nextIntermediateNodeAndPos: "
        "no unprocessed nodes are left.");
}

const SerializedEquivalent OptimalPathResult::firstPathEquivalent() const
{
    if (flows.empty()) {
        throw NotFoundError(
            "OptimalPathResult::firstPathEquivalent: "
            "no flows are present.");
    }
    return flows.front().second;
}

const SerializedEquivalent OptimalPathResult::currentPathEquivalent() const
{
    if (isLastIntermediateNodeProcessed()) {
        return flows.back().second;
    }
    return currentPathFlow().second;
}

const pair<TrustLineAmount, SerializedEquivalent> OptimalPathResult::currentPathFlow() const
{
    for (SerializedPositionInPath idx = 0; idx < mIntermediateNodesStates.size(); ++idx)
        if (mIntermediateNodesStates[idx] != OptimalPathResult::ReservationApproved &&
                mIntermediateNodesStates[idx] != OptimalPathResult::ReservationRejected)
            return flows[idx+1];

    throw NotFoundError(
        "OptimalPathResult::currentPathFlow: no unprocessed nodes are left.");
}

const pair<TrustLineAmount, SerializedEquivalent> OptimalPathResult::previousPathFlow() const
{
    for (SerializedPositionInPath idx = 0; idx < mIntermediateNodesStates.size(); ++idx)
        if (mIntermediateNodesStates[idx] != OptimalPathResult::ReservationApproved &&
                mIntermediateNodesStates[idx] != OptimalPathResult::ReservationRejected)
            return flows[idx];

    return flows[mIntermediateNodesStates.size()];
}

const bool OptimalPathResult::reservationRequestSentToAllNodes() const
{
    return mIntermediateNodesStates.at(
               mIntermediateNodesStates.size()-1) != ReservationRequestDoesntSent;
}

const bool OptimalPathResult::isNeighborAmountReserved() const
{
    return mIntermediateNodesStates[0] == OptimalPathResult::NeighbourReservationApproved;
}

const bool OptimalPathResult::isWaitingForNeighborReservationResponse() const
{
    return mIntermediateNodesStates[0] == OptimalPathResult::NeighbourReservationRequestSent;
}

const bool OptimalPathResult::isWaitingForNeighborReservationPropagationResponse() const
{
    return mIntermediateNodesStates[0] == OptimalPathResult::ReservationRequestSent;
}

/**
 * @returns true if current path sent amount reservation request and
 * is now waiting for the response to it.
 */
const bool OptimalPathResult::isWaitingForReservationResponse() const
{
    if (mPath.length() == 1) {
        return false;
    }

    for (const auto& it: mIntermediateNodesStates)
        if (it == OptimalPathResult::ReservationRequestSent)
            return true;

    return false;
}

const bool OptimalPathResult::isReadyToSendNextReservationRequest() const
{
    return !isWaitingForReservationResponse() &&
           !isWaitingForNeighborReservationResponse() &&
           !isLastIntermediateNodeProcessed();
}

const bool OptimalPathResult::isLastIntermediateNodeProcessed() const
{
    return
        mIntermediateNodesStates[mIntermediateNodesStates.size()-1] !=
        OptimalPathResult::ReservationRequestDoesntSent &&
        mIntermediateNodesStates[mIntermediateNodesStates.size()-1] !=
        OptimalPathResult::NeighbourReservationRequestSent &&
        mIntermediateNodesStates[mIntermediateNodesStates.size()-1] !=
        OptimalPathResult::NeighbourReservationApproved;
}

const bool OptimalPathResult::isLastIntermediateNodeApproved() const
{
    return mIntermediateNodesStates[mIntermediateNodesStates.size()-1] ==
           OptimalPathResult::NeighbourReservationApproved ||
           mIntermediateNodesStates[mIntermediateNodesStates.size()-1] ==
           OptimalPathResult::ReservationApproved;
}

const bool OptimalPathResult::isValid() const
{
    return mIsValid;
}

void OptimalPathResult::setUnusable()
{
    mIsValid = false;
    mMaxPathFlow = 0;
}


const ExchangeStep* OptimalPathResult::findExchangeStep(
    ContractorID nodeID,
    SerializedEquivalent fromEquivalent,
    SerializedEquivalent toEquivalent) const
{
    const auto it = std::find_if(
        mPath.exchangeSteps.begin(),
        mPath.exchangeSteps.end(),
        [nodeID, fromEquivalent, toEquivalent](const ExchangeStep &step) {
            return step.nodeID == nodeID &&
                   step.fromEquivalent == fromEquivalent &&
                   step.toEquivalent == toEquivalent;
        });

    if (it == mPath.exchangeSteps.end()) {
        return nullptr;
    }

    return &(*it);
}

/**
 * Calculates flow amounts between each pair of nodes along the path
 * for a given payment amount, taking into account exchange rates and commissions.
 *
 * @param paymentAmount - input payment amount in the first equivalent
 * @throws ValueError if paymentAmount > optimal_flow or if path structure is invalid
 */
void OptimalPathResult::calculateFlows(const TrustLineAmount& paymentAmount)
{
    if (paymentAmount > optimal_flow) {
        throw ValueError(
            "OptimalPathResult::calculateFlows: "
            "Payment amount exceeds optimal flow");
    }

    flows.clear();
    paymentFlow = paymentAmount;

    if (mPath.ids.empty() || mPath.ids.size() != mPath.equivalents.size()) {
        throw ValueError(
            "OptimalPathResult::calculateFlows: "
            "Invalid path structure");
    }

    TrustLineAmount currentAmount = paymentAmount;
    flows.reserve(mPath.ids.size() > 1 ? mPath.ids.size() - 1 : 0);

    for (size_t idx = 0; idx + 1 < mPath.ids.size(); ++idx) {
        const ContractorID fromNode = mPath.ids[idx];
        const ContractorID toNode = mPath.ids[idx + 1];
        const SerializedEquivalent currentEquivalent = mPath.equivalents[idx];
        const SerializedEquivalent nextEquivalent = mPath.equivalents[idx + 1];

        if (fromNode == toNode && currentEquivalent != nextEquivalent) {
            const auto *exchangeStep = findExchangeStep(
                fromNode,
                currentEquivalent,
                nextEquivalent);
            if (!exchangeStep) {
                throw ValueError(
                    "OptimalPathResult::calculateFlows: "
                    "Exchange step not found");
            }

            if (exchangeStep->minExchangeAmount > TrustLineAmount(0) &&
                currentAmount < exchangeStep->minExchangeAmount) {
                throw ValueError(
                    "OptimalPathResult::calculateFlows: "
                    "Amount below minimum exchange limit");
            }

            if (exchangeStep->maxExchangeAmount > TrustLineAmount(0) &&
                currentAmount > exchangeStep->maxExchangeAmount) {
                throw ValueError(
                    "OptimalPathResult::calculateFlows: "
                    "Amount exceeds maximum exchange limit");
            }

            currentAmount = exchangeStep->applyExchangeForward(currentAmount);
            continue;
        }

        flows.emplace_back(currentAmount, currentEquivalent);

        if (idx + 1 < mPath.ids.size() - 1) {
            const auto *commissionStep = findExchangeStep(
                toNode,
                nextEquivalent,
                nextEquivalent);
            if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
                if (currentAmount < commissionStep->commission) {
                    throw ValueError(
                        "OptimalPathResult::calculateFlows: "
                        "Flow exhausted by commission at intermediate node");
                }
                currentAmount = currentAmount - commissionStep->commission;
            }
        }
    }
}

/**
 * Performs forward simulation of a payment path using optional overrides for
 * exchange rate / commission at a single path position.
 *
 * Algorithm overview:
 *  1. Start with the coordinator input amount.
 *  2. Iterate pairs of consecutive entries in `mPath.ids` / `mPath.equivalents`.
 *     a. If the pair represents an in-place exchange (same node, different
 *        equivalents) – apply the exchange rate, using the updated value for
 *        the affected position when provided.
 *     b. Otherwise check if the current node charges a commission (same
 *        equivalent on both sides). Subtract the commission, applying the
 *        override when the position matches the affected one.
 *  3. The amount remaining after the loop is what the receiver would obtain.
 *
 * @param inputFlow amount injected at the coordinator (sender-side equivalent)
 * @param affectedPositionInPath node position in `mPath.ids` where new conditions apply
 * @param updatedRate optional pair (mantissa, exponent) for the exchange rate override
 * @param updatedCommission optional commission override to subtract at the affected node
 * @return amount received at the destination after applying all exchanges/commissions
 * @throws ValueError when the relevant exchange/commission step is missing or commission exceeds the available amount
 */
TrustLineAmount OptimalPathResult::calculateReceivedAmountWithUpdatedConditions(
    const TrustLineAmount &inputFlow,
    const SerializedPositionInPath affectedPositionInPath,
    const optional<pair<TrustLineAmount, int16_t>> &updatedRate,
    const optional<TrustLineAmount> &updatedCommission) const
{
    TrustLineAmount currentAmount = inputFlow;

    for (size_t idx = 0; idx + 1 < mPath.ids.size(); ++idx) {
        const ContractorID currentNode = mPath.ids[idx];
        const ContractorID nextNode = mPath.ids[idx + 1];
        const SerializedEquivalent currentEquiv = mPath.equivalents[idx];
        const SerializedEquivalent nextEquiv = mPath.equivalents[idx + 1];

        if (currentNode == nextNode && currentEquiv != nextEquiv) {
            const auto *exchangeStep = findExchangeStep(
                currentNode,
                currentEquiv,
                nextEquiv);

            if (!exchangeStep) {
                throw ValueError(
                    "OptimalPathResult::calculateReceivedAmountWithUpdatedConditions: "
                    "exchange step not found");
            }

            ExchangeStep effectiveStep = *exchangeStep;
            if (updatedRate && idx == affectedPositionInPath) {
                effectiveStep.exchangeRate = updatedRate->first;
                effectiveStep.exchangeRateShift = updatedRate->second;
            }

            currentAmount = effectiveStep.applyExchangeForward(currentAmount);
            continue;
        }

        if (idx > 0 && idx < mPath.ids.size() - 1) {
            const auto *commissionStep = findExchangeStep(
                currentNode,
                currentEquiv,
                currentEquiv);

            if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
                TrustLineAmount commissionToApply = commissionStep->commission;

                if (updatedCommission && idx == affectedPositionInPath) {
                    commissionToApply = *updatedCommission;
                }

                if (currentAmount < commissionToApply) {
                    throw ValueError(
                        "OptimalPathResult::calculateReceivedAmountWithUpdatedConditions: "
                        "amount exhausted by commission");
                }

                currentAmount = currentAmount - commissionToApply;
            }
        }
    }

    return currentAmount;
}

/**
 * Backward simulation that determines how much input at the sender is needed
 * to deliver a target receiver amount when conditions may have changed at one
 * position.
 *
 * Steps:
 *  1. Start from the desired receiver amount.
 *  2. Walk the path in reverse order.
 *     a. For exchange entries, invert the rate (using the updated value when
 *        the current position matches the affected index).
 *     b. For commission entries, add the commission back to the amount,
 *        substituting the provided override when supplied.
 *  3. The resulting amount is the required sender-side flow.
 *
 * @param desiredReceivedAmount target amount to deliver at the receiver
 * @param affectedNodePosition position in `mPath.ids` where updated rate/commission should apply
 * @param updatedRate optional replacement rate (mantissa, exponent) for the affected exchange
 * @param updatedCommission optional replacement commission for the affected node
 * @return minimal sender-side amount required to satisfy the receiver target
 * @throws ValueError if the path structure is invalid, exchange step is missing, or commission addition overflows
 */
TrustLineAmount OptimalPathResult::calculateOptimalFlowWithUpdatedConditions(
    const TrustLineAmount &desiredReceivedAmount,
    const SerializedPositionInPath affectedNodePosition,
    const optional<pair<TrustLineAmount, int16_t>> &updatedRate,
    const optional<TrustLineAmount> &updatedCommission) const
{
    TrustLineAmount requiredAmount = desiredReceivedAmount;

    if (mPath.ids.empty() || mPath.ids.size() != mPath.equivalents.size()) {
        throw ValueError(
            "OptimalPathResult::calculateOptimalFlowWithUpdatedConditions: "
            "invalid path structure");
    }

    for (size_t idx = mPath.ids.size() - 1; idx > 0; --idx) {
        const ContractorID previousNode = mPath.ids[idx - 1];
        const ContractorID currentNode = mPath.ids[idx];
        const SerializedEquivalent previousEquiv = mPath.equivalents[idx - 1];
        const SerializedEquivalent currentEquiv = mPath.equivalents[idx];

        if (previousNode == currentNode && previousEquiv != currentEquiv) {
            const auto *exchangeStep = findExchangeStep(
                currentNode,
                previousEquiv,
                currentEquiv);

            if (!exchangeStep) {
                throw ValueError(
                    "OptimalPathResult::calculateOptimalFlowWithUpdatedConditions: "
                    "exchange step not found");
            }

            ExchangeStep effectiveStep = *exchangeStep;
            if (updatedRate && (idx - 1) == affectedNodePosition) {
                effectiveStep.exchangeRate = updatedRate->first;
                effectiveStep.exchangeRateShift = updatedRate->second;
            }

            requiredAmount = effectiveStep.invertExchangeForRequiredInput(requiredAmount);
            continue;
        }

        if (idx < mPath.ids.size() - 1 && idx > 0) {
            const auto *commissionStep = findExchangeStep(
                currentNode,
                currentEquiv,
                currentEquiv);

            if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
                TrustLineAmount commissionToAdd = commissionStep->commission;

                if (updatedCommission && idx == affectedNodePosition) {
                    commissionToAdd = *updatedCommission;
                }

                try {
                    requiredAmount = requiredAmount + commissionToAdd;
                } catch (const std::exception &e) {
                    throw ValueError(
                        "OptimalPathResult::calculateOptimalFlowWithUpdatedConditions: "
                        "commission addition overflow: " + std::string(e.what()));
                }
            }
        }
    }

    return requiredAmount;
}
