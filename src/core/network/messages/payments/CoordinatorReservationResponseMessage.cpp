#include "CoordinatorReservationResponseMessage.h"

// Default constructor (no exchange rate, no commission)
CoordinatorReservationResponseMessage::CoordinatorReservationResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PathID &pathID,
    const ResponseMessage::OperationState state,
    const TrustLineAmount &reservedAmount) :

    ResponseMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        pathID,
        state),
    mAmountReserved(reservedAmount)
{
}

// Constructor with actual exchange rate
CoordinatorReservationResponseMessage::CoordinatorReservationResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PathID &pathID,
    const ResponseMessage::OperationState state,
    const TrustLineAmount &reservedAmount,
    const TrustLineAmount &actualExchangeRate,
    const int16_t actualExchangeRateShift) :

    ResponseMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        pathID,
        state),
    mAmountReserved(reservedAmount),
    mActualExchangeRate(make_pair(actualExchangeRate, actualExchangeRateShift))
{
}

// Constructor with actual commission
CoordinatorReservationResponseMessage::CoordinatorReservationResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PathID &pathID,
    const ResponseMessage::OperationState state,
    const TrustLineAmount &reservedAmount,
    const TrustLineAmount &actualCommission) :

    ResponseMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        pathID,
        state),
    mAmountReserved(reservedAmount),
    mActualCommission(actualCommission)
{
}

CoordinatorReservationResponseMessage::CoordinatorReservationResponseMessage(
    BytesShared buffer) :

    ResponseMessage(
        buffer)
{
    size_t offset = ResponseMessage::kOffsetToInheritedBytes();

    // Deserialize mAmountReserved (existing field)
    auto amountOffset = buffer.get() + offset;
    auto amountEndOffset = amountOffset + kTrustLineAmountBytesCount; // TODO: deserialize only non-zero
    vector<byte_t> amountBytes(amountOffset, amountEndOffset);
    mAmountReserved = bytesToTrustLineAmount(amountBytes);
    offset += kTrustLineAmountBytesCount;

    // Deserialize actual exchange rate flag
    byte_t hasExchangeRate = buffer.get()[offset];
    offset += sizeof(byte_t);

    if (hasExchangeRate == 1) {
        // Deserialize only rate and shift
        auto rateOffset = buffer.get() + offset;
        auto rateEndOffset = rateOffset + kTrustLineAmountBytesCount;
        vector<byte_t> rateBytes(rateOffset, rateEndOffset);
        TrustLineAmount rate = bytesToTrustLineAmount(rateBytes);
        offset += kTrustLineAmountBytesCount;

        int16_t shift;
        memcpy(&shift, buffer.get() + offset, sizeof(int16_t));
        offset += sizeof(int16_t);

        // Store as pair (rate, shift)
        mActualExchangeRate = make_pair(rate, shift);
    }
    // else mActualExchangeRate remains nullopt

    // Deserialize actual commission flag
    byte_t hasCommission = buffer.get()[offset];
    offset += sizeof(byte_t);

    if (hasCommission == 1) {
        auto commissionOffset = buffer.get() + offset;
        auto commissionEndOffset = commissionOffset + kTrustLineAmountBytesCount;
        vector<byte_t> commissionBytes(commissionOffset, commissionEndOffset);
        mActualCommission = bytesToTrustLineAmount(commissionBytes);
        offset += kTrustLineAmountBytesCount;
    }
    // else mActualCommission remains nullopt
}

const TrustLineAmount &CoordinatorReservationResponseMessage::amountReserved() const
{
    return mAmountReserved;
}

const optional<pair<TrustLineAmount, int16_t>>&
CoordinatorReservationResponseMessage::actualExchangeRate() const
{
    return mActualExchangeRate;
}

const optional<TrustLineAmount>&
CoordinatorReservationResponseMessage::actualCommission() const
{
    return mActualCommission;
}

pair<BytesShared, size_t> CoordinatorReservationResponseMessage::serializeToBytes() const
{
    auto parentBytesAndCount = ResponseMessage::serializeToBytes();
    auto serializedAmount = trustLineAmountToBytes(mAmountReserved);

    size_t bytesCount =
        parentBytesAndCount.second + serializedAmount.size();

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mAmountReserved);

    // Serialize actual exchange rate (only rate and shift, NOT equivalents or dates)
    if (mActualExchangeRate.has_value()) {
        serializer.copy((byte_t)1);  // Flag: exchange rate present
        serializer.copy(mActualExchangeRate->first);  // TrustLineAmount (rate)
        serializer.enqueue(&mActualExchangeRate->second, sizeof(int16_t));  // int16_t (shift)
    } else {
        serializer.copy((byte_t)0);  // Flag: no exchange rate
    }

    // Serialize actual commission
    if (mActualCommission.has_value()) {
        serializer.copy((byte_t)1);  // Flag: commission present
        serializer.copy(*mActualCommission);  // TrustLineAmount
    } else {
        serializer.copy((byte_t)0);  // Flag: no commission
    }

    return serializer.collect();
}

const Message::MessageType CoordinatorReservationResponseMessage::typeID() const
{
    return Message::Payments_CoordinatorReservationResponse;
}
