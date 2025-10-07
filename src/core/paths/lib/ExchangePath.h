#ifndef VTCPD_EXCHANGEPATH_H
#define VTCPD_EXCHANGEPATH_H

#include "../../common/Types.h"
#include "../../contractors/addresses/BaseAddress.h"
#include <vector>
#include <sstream>

using namespace std;

// Describes a single exchange operation at a node
struct ExchangeStep {
    ContractorID nodeID;
    SerializedEquivalent fromEquivalent;
    SerializedEquivalent toEquivalent;
    TrustLineAmount exchangeRate; // raw integer rate without shift
    int16_t exchangeRateShift{0};
    TrustLineAmount minExchangeAmount{0};
    TrustLineAmount maxExchangeAmount{0};
    TrustLineAmount commission;
};

// Represents a single feasible payment path through the network
struct ExchangePath {
    vector<ContractorID> ids;  // renamed from 'nodes'
    vector<BaseAddress::Shared> nodes;  // new field for addresses
    vector<SerializedEquivalent> equivalents;
    vector<ExchangeStep> exchangeSteps;
    TrustLineAmount minCapacity;
    double effectiveExchangeRate;
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
};

#endif // VTCPD_EXCHANGEPATH_H
