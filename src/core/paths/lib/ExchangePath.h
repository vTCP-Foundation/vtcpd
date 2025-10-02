#ifndef VTCPD_EXCHANGEPATH_H
#define VTCPD_EXCHANGEPATH_H

#include "../../common/Types.h"
#include <vector>

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
    vector<ContractorID> nodes;
    vector<SerializedEquivalent> equivalents;
    vector<ExchangeStep> exchangeSteps;
    TrustLineAmount minCapacity;
    double effectiveExchangeRate;
    TrustLineAmount totalCommissions;

    bool isValid() const { return !nodes.empty() && nodes.size() == equivalents.size(); }
    TrustLineAmount calculateMaxCapacity() const { return minCapacity; }
    double calculateEffectiveExchangeRate() const { return effectiveExchangeRate; }
    TrustLineAmount sumFixedCommissions() const { return totalCommissions; }
    bool startsWithEquivalent(SerializedEquivalent equiv) const {
        return !equivalents.empty() && equivalents[0] == equiv;
    }
};

#endif // VTCPD_EXCHANGEPATH_H
