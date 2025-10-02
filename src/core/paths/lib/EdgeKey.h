#ifndef VTCPD_EDGEKEY_H
#define VTCPD_EDGEKEY_H

#include "../../common/Types.h"

// Unique identifier for a directed edge in the network topology
struct EdgeKey {
    ContractorID from;
    ContractorID to;
    SerializedEquivalent equivalent;

    bool operator<(const EdgeKey &other) const noexcept
    {
        if (from != other.from) return from < other.from;
        if (to != other.to) return to < other.to;
        return equivalent < other.equivalent;
    }
};

#endif // VTCPD_EDGEKEY_H
