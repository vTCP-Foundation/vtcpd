#ifndef CoordinatorReservationResponseMessageMESSAGE_H
#define CoordinatorReservationResponseMessageMESSAGE_H


#include "base/ResponseMessage.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include <optional>


class CoordinatorReservationResponseMessage:
    public ResponseMessage
{

public:
    typedef shared_ptr<CoordinatorReservationResponseMessage> Shared;
    typedef shared_ptr<const CoordinatorReservationResponseMessage> ConstShared;

public:
    // Default constructor (no exchange rate, no commission)
    CoordinatorReservationResponseMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PathID &pathID,
        const OperationState state,
        const TrustLineAmount &reservedAmount=0);

    // Constructor with actual exchange rate
    CoordinatorReservationResponseMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PathID &pathID,
        const OperationState state,
        const TrustLineAmount &reservedAmount,
        const TrustLineAmount &actualExchangeRate,
        const int16_t actualExchangeRateShift);

    // Constructor with actual commission
    CoordinatorReservationResponseMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PathID &pathID,
        const OperationState state,
        const TrustLineAmount &reservedAmount,
        const TrustLineAmount &actualCommission);

    // Deserialization constructor
    CoordinatorReservationResponseMessage(
        BytesShared buffer);

    const TrustLineAmount& amountReserved() const;

    // Getters for actual exchange rate and commission
    const optional<pair<TrustLineAmount, int16_t>>& actualExchangeRate() const;
    const optional<TrustLineAmount>& actualCommission() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

    const MessageType typeID() const override;

protected:
    TrustLineAmount mAmountReserved;

    // Optional fields for condition validation response
    // Exchange rate stored as pair<rate, shift> - equivalents derived from context
    optional<pair<TrustLineAmount, int16_t>> mActualExchangeRate;
    optional<TrustLineAmount> mActualCommission;
};

#endif // INTERMEDIATENODERESERVATIONRESPONSE_H
