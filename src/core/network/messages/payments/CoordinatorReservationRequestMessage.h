#ifndef COORDINATORRESERVATIONREQUESTMESSAGE_H
#define COORDINATORRESERVATIONREQUESTMESSAGE_H

#include "base/RequestMessageWithReservations.h"
#include <optional>

class CoordinatorReservationRequestMessage:
    public RequestMessageWithReservations
{

public:
    typedef shared_ptr<CoordinatorReservationRequestMessage> Shared;
    typedef shared_ptr<const CoordinatorReservationRequestMessage> ConstShared;

public:
    // Default constructor (no exchange rate, no commission)
    CoordinatorReservationRequestMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID& transactionUUID,
        const vector<PathReservation> &finalAmountsConfig,
        BaseAddress::Shared nextNodeInThePath);

    // Constructor with expected exchange rate
    CoordinatorReservationRequestMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID& transactionUUID,
        const vector<PathReservation> &finalAmountsConfig,
        BaseAddress::Shared nextNodeInThePath,
        const TrustLineAmount &expectedExchangeRate,
        const int16_t expectedExchangeRateShift);

    // Constructor with expected commission
    CoordinatorReservationRequestMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID& transactionUUID,
        const vector<PathReservation> &finalAmountsConfig,
        BaseAddress::Shared nextNodeInThePath,
        const TrustLineAmount &expectedCommission);

    // DEPRECATED: Constructor without equivalents
    CoordinatorReservationRequestMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID& transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
        BaseAddress::Shared nextNodeInThePath);

    // Deserialization constructor
    CoordinatorReservationRequestMessage(
        BytesShared buffer);

    BaseAddress::Shared nextNodeInPath() const;

    // Getters for expected exchange rate and commission
    const optional<pair<TrustLineAmount, int16_t>>& expectedExchangeRate() const;
    const optional<TrustLineAmount>& expectedCommission() const;

    const Message::MessageType typeID() const override;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

protected:
    BaseAddress::Shared mNextPathNode;

    // Optional fields for condition validation
    // Exchange rate stored as pair<rate, shift> - equivalents derived from context
    optional<pair<TrustLineAmount, int16_t>> mExpectedExchangeRate;
    optional<TrustLineAmount> mExpectedCommission;
};

#endif // COORDINATORRESERVATIONREQUESTMESSAGE_H
