#include "OptimalPathResult.h"

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

/**
 * @param kAmount - new max flow of the path.
 *
 * @throws ValueError in case of attempt to increase path max flow.
 */
void OptimalPathResult::shortageMaxFlow(
    const TrustLineAmount& kAmount)
{
    if (mMaxPathFlow == 0) {
        mMaxPathFlow = kAmount;
    }
    else if (kAmount <= mMaxPathFlow) {
        mMaxPathFlow = kAmount;
    }
    else
        throw ValueError(
            "OptimalPathResult::shortageMaxFlow: "
            "attempt to increase max flow occurred.");
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

const SerializedEquivalent OptimalPathResult::currentPathEquivalent() const
{
    for (SerializedPositionInPath idx = 0; idx < mIntermediateNodesStates.size(); ++idx)
        if (mIntermediateNodesStates[idx] != OptimalPathResult::ReservationApproved &&
                mIntermediateNodesStates[idx] != OptimalPathResult::ReservationRejected)
            return mPath.equivalents[idx];

    throw NotFoundError(
        "OptimalPathResult::currentPathEquivalent: "
        "no unprocessed nodes are left.");
}

const pair<TrustLineAmount, SerializedEquivalent> OptimalPathResult::currentPathFlow() const
{
    for (SerializedPositionInPath idx = 0; idx < mIntermediateNodesStates.size(); ++idx)
        if (mIntermediateNodesStates[idx] != OptimalPathResult::ReservationApproved &&
                mIntermediateNodesStates[idx] != OptimalPathResult::ReservationRejected)
            return flows[idx+1];

    throw NotFoundError(
        "OptimalPathResult::currentPathFlow: "
        "no unprocessed nodes are left.");
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

/**
 * Calculates flow amounts between each pair of nodes along the path
 * for a given payment amount, taking into account exchange rates.
 *
 * @param paymentAmount - input payment amount in the first equivalent
 * @throws ValueError if paymentAmount > optimal_flow or if path structure is invalid
 */
void OptimalPathResult::calculateFlows(const TrustLineAmount& paymentAmount)
{
    // Step 1: Validate paymentAmount <= optimal_flow
    if (paymentAmount > optimal_flow) {
        throw ValueError(
            "OptimalPathResult::calculateFlows: "
            "Payment amount exceeds optimal flow");
    }

    // Step 2: Clear and initialize
    flows.clear();
    paymentFlow = paymentAmount;

    // Step 3: Validate path structure
    if (mPath.ids.empty() || mPath.ids.size() != mPath.equivalents.size()) {
        throw ValueError(
            "OptimalPathResult::calculateFlows: "
            "Invalid path structure");
    }

    // Step 4: Start simulation with paymentAmount
    double currentAmount = paymentAmount.convert_to<double>();

    // Step 5: Process each step in path
    for (size_t k = 0; k + 1 < mPath.ids.size(); ++k) {
        ContractorID fromNode = mPath.ids[k];
        ContractorID toNode = mPath.ids[k + 1];
        SerializedEquivalent currentEquiv = mPath.equivalents[k];
        SerializedEquivalent nextEquiv = mPath.equivalents[k + 1];

        // Step 5a: Check if this is an exchange step
        if (fromNode == toNode && currentEquiv != nextEquiv) {
            // Apply exchange rate
            for (const auto& ex : mPath.exchangeSteps) {
                if (ex.nodeID == fromNode &&
                    ex.fromEquivalent == currentEquiv &&
                    ex.toEquivalent == nextEquiv) {

                    // Calculate effective rate with shift
                    double rate = ex.exchangeRate.convert_to<double>();
                    int16_t shift = ex.exchangeRateShift;
                    double effectiveRate = rate * std::pow(10.0, shift);

                    // Validate exchange limits before exchange
                    if (ex.minExchangeAmount > TrustLineAmount(0)) {
                        if (currentAmount < ex.minExchangeAmount.convert_to<double>()) {
                            throw ValueError(
                                "OptimalPathResult::calculateFlows: "
                                "Amount below minimum exchange limit");
                        }
                    }
                    if (ex.maxExchangeAmount > TrustLineAmount(0)) {
                        if (currentAmount > ex.maxExchangeAmount.convert_to<double>()) {
                            throw ValueError(
                                "OptimalPathResult::calculateFlows: "
                                "Amount exceeds maximum exchange limit");
                        }
                    }

                    // Apply exchange
                    currentAmount *= effectiveRate;
                    break;
                }
            }
            continue;
        }

        // Step 5b: Regular edge - record flow (without commission for now)
        TrustLineAmount flowAmount(static_cast<uint64_t>(currentAmount));
        flows.push_back(make_pair(flowAmount, currentEquiv));
    }
}
