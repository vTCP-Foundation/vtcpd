#ifndef VTCPD_PATHRESERVATION_H
#define VTCPD_PATHRESERVATION_H

#include "../../../../../common/Types.h"

struct PathReservation {
    enum Direction {
        Incoming = 0,
        Outgoing = 1
    };

    PathID pathID;
    ConstSharedTrustLineAmount amount;
    SerializedEquivalent equivalent;
    Direction direction;

    PathReservation(
        const PathID &id,
        const ConstSharedTrustLineAmount &amt,
        const SerializedEquivalent &equiv,
        const Direction &dir)
        : pathID(id), amount(amt), equivalent(equiv), direction(dir) {}
};

#endif //VTCPD_PATHRESERVATION_H
