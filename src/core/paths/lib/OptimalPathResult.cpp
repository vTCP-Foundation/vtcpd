#include "OptimalPathResult.h"

#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>

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

namespace {

using boost::multiprecision::cpp_int;

cpp_int pow10(const size_t exponent)
{
    cpp_int result = 1;
    for (size_t idx = 0; idx < exponent; ++idx) {
        result *= 10;
    }
    return result;
}

const ExchangeStep* findExchangeStep(
    const ExchangePath &path,
    ContractorID nodeID,
    SerializedEquivalent fromEquivalent,
    SerializedEquivalent toEquivalent)
{
    const auto it = std::find_if(
        path.exchangeSteps.begin(),
        path.exchangeSteps.end(),
        [nodeID, fromEquivalent, toEquivalent](const ExchangeStep &step) {
            return step.nodeID == nodeID &&
                   step.fromEquivalent == fromEquivalent &&
                   step.toEquivalent == toEquivalent;
        });

    if (it == path.exchangeSteps.end()) {
        return nullptr;
    }

    return &(*it);
}

TrustLineAmount applyExchangeForward(
    const TrustLineAmount &amount,
    const ExchangeStep &step)
{
    cpp_int result = cpp_int(amount) * cpp_int(step.exchangeRate);

    if (step.exchangeRateShift >= 0) {
        result *= pow10(static_cast<size_t>(step.exchangeRateShift));
    } else {
        const cpp_int divisor = pow10(static_cast<size_t>(-step.exchangeRateShift));
        result /= divisor;
    }

    if (result < 0) {
        throw ValueError(
            "OptimalPathResult::calculateFlows: negative exchange result");
    }

    return result.convert_to<TrustLineAmount>();
}

} // namespace

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
                mPath,
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

            currentAmount = applyExchangeForward(currentAmount, *exchangeStep);
            continue;
        }

        flows.emplace_back(currentAmount, currentEquivalent);

        if (idx + 1 < mPath.ids.size() - 1) {
            const auto *commissionStep = findExchangeStep(
                mPath,
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
