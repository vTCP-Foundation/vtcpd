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
    TrustLineAmount optimal_flow;
    TrustLineAmount received_amount;
    double effective_exchange_rate;
    double path_efficiency;

    // Fields from PathStats for reservation management
    ExchangePath mPath;
    TrustLineAmount mMaxPathFlow;
    bool mIsValid;
    vector<NodeState> mIntermediateNodesStates;

    // Constructor
    OptimalPathResult() : mMaxPathFlow(0), mIsValid(true) {}

    // Methods from PathStats
    void setNodeState(const SerializedPositionInPath positionInPath, const NodeState state);
    const TrustLineAmount& maxFlow() const;
    void shortageMaxFlow(const TrustLineAmount &kAmount);
    ExchangePath& path();
    const ExchangePath& path() const;
    bool containsIntermediateNodes() const;
    const pair<BaseAddress::Shared, SerializedPositionInPath> currentIntermediateNodeAndPos() const;
    const pair<BaseAddress::Shared, SerializedPositionInPath> nextIntermediateNodeAndPos() const;
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
};

#endif // VTCPD_OPTIMALPATHRESULT_H
