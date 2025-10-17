#ifndef VTCPD_EXCHANGEPATH_H
#define VTCPD_EXCHANGEPATH_H

#include "../../common/Types.h"
#include "../../contractors/addresses/BaseAddress.h"
#include <vector>
#include <sstream>

using namespace std;

// Describes a single exchange operation performed in-place at a node
//
// Semantics:
//   output_amount(toEquivalent) = input_amount(fromEquivalent) * effectiveRate
// where
//   effectiveRate = exchangeRate * 10^(exchangeRateShift)
//
// Limits (if set > 0) apply to the INPUT side (fromEquivalent) before the
// conversion. If input is below minExchangeAmount or above maxExchangeAmount,
// the step is considered infeasible.
struct ExchangeStep {
    // Node at which the currency conversion occurs.
    // Representation detail: in the path this is encoded as a self-edge
    // (ids[i] == ids[i+1]) where equivalents[i] != equivalents[i+1].
    ContractorID nodeID;

    // Input and output equivalents (currencies) for the conversion
    // at this node. Units of amounts are in 'fromEquivalent' before
    // the step and in 'toEquivalent' after the step.
    SerializedEquivalent fromEquivalent;
    SerializedEquivalent toEquivalent;

    // Raw integer part of the exchange rate.
    // Effective rate = exchangeRate * 10^(exchangeRateShift)
    // Using the shift allows precise decimal rates without floating point.
    // Examples:
    //   1.00  => exchangeRate=1,   exchangeRateShift=0
    //   0.5   => exchangeRate=5,   exchangeRateShift=-1
    //   12.5  => exchangeRate=125, exchangeRateShift=-1
    TrustLineAmount exchangeRate;
    int16_t exchangeRateShift{0};

    // Optional limits for the INPUT amount (before conversion), expressed
    // in the 'fromEquivalent'. Zero means "not set".
    TrustLineAmount minExchangeAmount{0};
    TrustLineAmount maxExchangeAmount{0};

    // Reserved for per-step fixed commission if needed. The current flow
    // simulation retrieves intermediate-node commissions dynamically from
    // trust lines and applies them at arrival nodes (in the arrival
    // equivalent). This field is present for potential future use and is
    // not relied upon by the current algorithms.
    TrustLineAmount commission;
};

// Represents a single feasible payment path through the network
struct ExchangePath {
    // Ordered node identifiers along the path. An in-place exchange is
    // represented as consecutive identical IDs with a changed equivalent.
    vector<ContractorID> ids;

    // Optional resolved addresses aligned with 'ids' for human-readable
    // logging and address-based operations.
    vector<BaseAddress::Shared> nodes;

    // Equivalent (currency) at each position in the path. Must be the same
    // length as 'ids'.
    vector<SerializedEquivalent> equivalents;

    // Explicit list of exchange steps (rate, limits) and commissions.
    // Exchange steps: occur where ids[i] == ids[i+1] and fromEquivalent != toEquivalent
    // Commission steps: occur where ids[i] == ids[i+1] and fromEquivalent == toEquivalent
    vector<ExchangeStep> exchangeSteps;

    // Path capacity on the source side (sender equivalent): the maximum
    // injectible flow that respects edge capacities and commission-aware
    // constraints discovered during enumeration.
    TrustLineAmount minCapacity;

    // Theoretical product of all exchange rates along the path (ignores
    // commissions/capacity). Used for scoring and constraints.
    double effectiveExchangeRate;

    // Sum of fixed intermediate-node commissions accounted in the target
    // equivalent (excluding sender and receiver nodes).
    TrustLineAmount totalCommissions;

    // Original methods
    bool isValid() const {
        return !ids.empty() && ids.size() == equivalents.size() &&
               (nodes.empty() || nodes.size() == ids.size());
    }
    TrustLineAmount calculateMaxCapacity() const { return minCapacity; }
    double calculateEffectiveExchangeRate() const { return effectiveExchangeRate; }
    TrustLineAmount sumFixedCommissions() const { return totalCommissions; }
    bool startsWithEquivalent(SerializedEquivalent equiv) const {
        return !equivalents.empty() && equivalents[0] == equiv;
    }

    // Methods from Path class
    vector<BaseAddress::Shared> intermediates() const {
        return nodes;
    }

    int positionOfNode(BaseAddress::Shared nodeAddress) const {
        for (size_t nodeIdx = 0; nodeIdx < nodes.size(); nodeIdx++) {
            if (nodes.at(nodeIdx) == nodeAddress) {
                return (int)nodeIdx;
            }
        }
        return -1;
    }

    void addReceiver(BaseAddress::Shared receiverAddress) {
        nodes.push_back(receiverAddress);
    }

    bool containsTrustLine(
        BaseAddress::Shared source,
        BaseAddress::Shared destination) const {
        for (size_t nodeIdx = 0; nodeIdx < nodes.size() - 1; nodeIdx++) {
            if (nodes.at(nodeIdx) == source && nodes.at(nodeIdx + 1) == destination) {
                return true;
            }
        }
        return false;
    }

    const size_t length() const {
        return nodes.size();
    }

    const string toString() const {
        if (nodes.empty()) {
            return "direct exchange path";
        }
        stringstream s;
        s << "(" << nodes.cbegin()->get()->fullAddress() << ")";
        for (auto it=(++nodes.cbegin()); it != (nodes.cend()); ++it) {
            s << "-(" << it->get()->fullAddress() << ")";
        }
        return s.str();
    }

    friend bool operator== (
        const ExchangePath &p1,
        const ExchangePath &p2);
};

#endif // VTCPD_EXCHANGEPATH_H
