#ifndef VTCPD_OPTIMALPATHRESULT_H
#define VTCPD_OPTIMALPATHRESULT_H

#include "ExchangePath.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"
#include <utility>

using std::pair;
using std::make_pair;

// Stores result of OR-Tools optimization for a single path
struct OptimalPathResult {
    // Node state enumeration (from PathStats)
    enum NodeState {
        ReservationRequestDoesntSent = 0,
        NeighbourReservationRequestSent,
        NeighbourReservationApproved,
        ReservationRequestSent,
        ReservationApproved,
        ReservationRejected
    };

    // Original fields from OR-Tools optimization
    //
    // Maximum allowed input flow for this path measured in the sender's
    // equivalent. Initially comes from the LP solver solution and may be
    // subsequently reduced to respect edge capacities and already-applied
    // commissions. Acts as an upper bound for what can be injected at source.
    TrustLineAmount optimal_flow;

    // Maximum net amount deliverable to the receiver (in the receiver's
    // equivalent) after applying all exchanges and fixed commissions along
    // the path. Computed via path simulation and used as a cap during
    // inverse (receive-target) estimations.
    TrustLineAmount received_amount;

    // Theoretical exchange rate of the path: the pure product of exchange
    // steps' rates along the path, ignoring commissions and capacity limits.
    // Used for ordering/scoring paths and constraint coefficients.
    double effective_exchange_rate;

    // Actual path efficiency after simulation: ratio of delivered output to
    // input, i.e. received_amount / optimal_flow. Initially set to
    // effective_exchange_rate as a placeholder, then finalized after
    // simulation with capacities and commissions.
    double path_efficiency;

    // Fields from PathStats for reservation management
    //
    // Concrete path selected by optimization: ordered sequence of node IDs /
    // addresses with per-step equivalents and exchange steps. Used for flow
    // simulation (to compute deliverable amounts and efficiency) and for
    // tracking reservation state across intermediate nodes.
    ExchangePath mPath;
    TrustLineAmount mMaxPathFlow;
    bool mIsValid;
    vector<NodeState> mIntermediateNodesStates;

    // Flow calculation fields (Task 06-12)
    TrustLineAmount paymentFlow;
    vector<pair<TrustLineAmount, SerializedEquivalent>> flows;

    // Constructor
    OptimalPathResult() : mMaxPathFlow(0), mIsValid(true), paymentFlow(0) {}

    // Methods from PathStats
    void setNodeState(const SerializedPositionInPath positionInPath, const NodeState state);
    const TrustLineAmount& maxFlow() const;
    void shortageMaxFlow(const TrustLineAmount &kAmount);
    ExchangePath& path();
    const ExchangePath& path() const;
    bool containsIntermediateNodes() const;
    const pair<BaseAddress::Shared, SerializedPositionInPath> currentIntermediateNodeAndPos() const;
    const pair<BaseAddress::Shared, SerializedPositionInPath> nextIntermediateNodeAndPos() const;
    const SerializedEquivalent currentPathEquivalent() const;
    const pair<TrustLineAmount, SerializedEquivalent> currentPathFlow() const;
    const pair<TrustLineAmount, SerializedEquivalent> previousPathFlow() const;
    const bool reservationRequestSentToAllNodes() const;
    const bool isNeighborAmountReserved() const;
    const bool isWaitingForNeighborReservationResponse() const;
    const bool isWaitingForNeighborReservationPropagationResponse() const;
    const bool isWaitingForReservationResponse() const;
    const bool isReadyToSendNextReservationRequest() const;
    const bool isLastIntermediateNodeProcessed() const;
    const bool isLastIntermediateNodeApproved() const;
    const bool isValid() const;
    void setUnusable();

    // Flow calculation method (Task 06-12)
    void calculateFlows(const TrustLineAmount& paymentAmount);
};

#endif // VTCPD_OPTIMALPATHRESULT_H
