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

ExchangePath& OptimalPathResult::path_ref()
{
    return path;
}

const ExchangePath& OptimalPathResult::path_ref() const
{
    return path;
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
            return make_pair(path.intermediates()[idx], idx);

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
            return make_pair(path.intermediates()[idx], idx);
        }

        if (mIntermediateNodesStates[idx] == OptimalPathResult::ReservationRequestDoesntSent) {
            return make_pair(path.intermediates()[idx], idx);
        }
    }

    throw NotFoundError(
        "OptimalPathResult::nextIntermediateNodeAndPos: "
        "no unprocessed nodes are left.");
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
    if (path.length() == 1) {
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
