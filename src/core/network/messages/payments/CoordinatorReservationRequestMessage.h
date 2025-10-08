#ifndef COORDINATORRESERVATIONREQUESTMESSAGE_H
#define COORDINATORRESERVATIONREQUESTMESSAGE_H

#include "base/RequestMessageWithReservations.h"

class CoordinatorReservationRequestMessage:
    public RequestMessageWithReservations
{

public:
    typedef shared_ptr<CoordinatorReservationRequestMessage> Shared;
    typedef shared_ptr<const CoordinatorReservationRequestMessage> ConstShared;

public:
    // NEW: Constructor with equivalents
    CoordinatorReservationRequestMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID& transactionUUID,
        const vector<PathReservation> &finalAmountsConfig,
        BaseAddress::Shared nextNodeInThePath);

    // DEPRECATED: Constructor without equivalents
    CoordinatorReservationRequestMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID& transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
        BaseAddress::Shared nextNodeInThePath);

    CoordinatorReservationRequestMessage(
        BytesShared buffer);

    BaseAddress::Shared nextNodeInPath() const;

    const Message::MessageType typeID() const override;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

protected:
    BaseAddress::Shared mNextPathNode;
};

#endif // COORDINATORRESERVATIONREQUESTMESSAGE_H
