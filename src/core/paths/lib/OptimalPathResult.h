#ifndef VTCPD_OPTIMALPATHRESULT_H
#define VTCPD_OPTIMALPATHRESULT_H

#include "ExchangePath.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"

#include <optional>
#include <utility>
#include <set>
#include <vector>

using std::pair;
using std::make_pair;
using std::optional;
using std::set;
using std::vector;

using CommissionKey = pair<ContractorID, SerializedEquivalent>;

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

    // Maximum sender-side flow that can be injected into this path (sender's equivalent).
    // Represents the upper bound on what the coordinator can pay on this path.
    // May be reduced during reservation if intermediate nodes return lower amounts.
    TrustLineAmount mMaxPathFlow;

    // Maximum receiver-side amount that can be delivered through this path (receiver's equivalent).
    // Represents the upper bound on what the receiver can obtain on this path.
    // Used for recalculating optimal_flow when conditions (exchange rates/commissions) change.
    TrustLineAmount mMaxPathReceivedAmount;

    bool mIsValid;
    vector<NodeState> mIntermediateNodesStates;

    // Flow calculation fields (Task 06-12)
    TrustLineAmount paymentFlow;
    vector<pair<TrustLineAmount, SerializedEquivalent>> flows;

    // Constructor
    OptimalPathResult() : mMaxPathFlow(0), mMaxPathReceivedAmount(0), mIsValid(true), paymentFlow(0) {}

    // Methods from PathStats
    void setNodeState(const SerializedPositionInPath positionInPath, const NodeState state);
    const TrustLineAmount& maxFlow() const;
    ExchangePath& path();
    const ExchangePath& path() const;
    bool containsIntermediateNodes() const;
    const pair<BaseAddress::Shared, SerializedPositionInPath> currentIntermediateNodeAndPos() const;
    const pair<BaseAddress::Shared, SerializedPositionInPath> nextIntermediateNodeAndPos() const;
    const SerializedEquivalent firstPathEquivalent() const;
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

    const ExchangeStep* findExchangeStep(
        ContractorID nodeID,
        SerializedEquivalent fromEquivalent,
        SerializedEquivalent toEquivalent) const;

    // Flow calculation method (Task 06-12)
    void calculateFlows(const TrustLineAmount& paymentAmount);

    /**
     * Calculates the expected receiver-side amount after applying the path with
     * optionally updated exchange rate / commission at a specific position.
     *
     * Forward-simulates the payment beginning with the coordinator input,
     * applying exchanges and commissions hop-by-hop. At the affected position
     * the provided overrides (if any) are used instead of cached values.
     *
     * @param inputFlow amount injected at the coordinator (sender-side equivalent)
     * @param affectedPositionInPath node position in `mPath.ids` where new conditions apply
     * @param updatedRate optional pair (mantissa, exponent) for the exchange rate override
     * @param updatedCommission optional commission override to subtract at the affected node
     * @return amount received at the destination after applying all exchanges/commissions
     * @throws ValueError when required exchange/commission step is missing or commission exhausts the amount
     */
    TrustLineAmount calculateReceivedAmountWithUpdatedConditions(
        const TrustLineAmount &inputFlow,
        const SerializedPositionInPath affectedPositionInPath,
        const optional<pair<TrustLineAmount, int16_t>> &updatedRate,
        const optional<TrustLineAmount> &updatedCommission) const;

    /**
     * Calculates the sender-side flow required to deliver a target receiver
     * amount when conditions may have changed at a particular position.
     *
     * Backward-simulates from receiver to sender, inverting exchanges and
     * adding back commissions, using the updated values (if supplied) at the
     * affected step.
     *
     * @param desiredReceivedAmount target amount to deliver at the receiver
     * @param affectedNodePosition position in `mPath.ids` where updated rate/commission should apply
     * @param updatedRate optional replacement rate (mantissa, exponent) for the affected exchange
     * @param updatedCommission optional replacement commission for the affected node
     * @return minimal sender-side amount required to satisfy the receiver target
     * @throws ValueError on missing exchange step, invalid path structure, or arithmetic overflow when adding commission
     */
    TrustLineAmount calculateOptimalFlowWithUpdatedConditions(
        const TrustLineAmount &desiredReceivedAmount,
        const SerializedPositionInPath affectedNodePosition,
        const optional<pair<TrustLineAmount, int16_t>> &updatedRate,
        const optional<TrustLineAmount> &updatedCommission) const;

    TrustLineAmount calculateRequiredInputForPath(
        const TrustLineAmount &desiredOutputAmount) const;

    void adjustCommissionsForEstimation(
        const set<CommissionKey> &alreadyConsumed,
        vector<CommissionKey> &pendingForPath);
};

#endif // VTCPD_OPTIMALPATHRESULT_H
