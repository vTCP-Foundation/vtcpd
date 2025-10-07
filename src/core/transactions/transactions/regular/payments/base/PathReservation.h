#ifndef VTCPD_PATHRESERVATION_H
#define VTCPD_PATHRESERVATION_H

#include "../../../../../common/Types.h"

struct PathReservation {
    PathID pathID;
    ConstSharedTrustLineAmount amount;
    SerializedEquivalent equivalent;

    PathReservation(
        const PathID &id,
        const ConstSharedTrustLineAmount &amt,
        const SerializedEquivalent &equiv)
        : pathID(id), amount(amt), equivalent(equiv) {}
};

#endif //VTCPD_PATHRESERVATION_H
