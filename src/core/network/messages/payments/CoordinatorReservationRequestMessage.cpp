#include "CoordinatorReservationRequestMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"


// Default constructor (no exchange rate, no commission)
CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    BaseAddress::Shared nextNodeInThePath) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mNextPathNode(nextNodeInThePath)
{}

// Constructor with expected exchange rate
CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    BaseAddress::Shared nextNodeInThePath,
    const TrustLineAmount &expectedExchangeRate,
    const int16_t expectedExchangeRateShift) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mNextPathNode(nextNodeInThePath),
    mExpectedExchangeRate(make_pair(expectedExchangeRate, expectedExchangeRateShift))
{}

// Constructor with expected commission
CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    BaseAddress::Shared nextNodeInThePath,
    const TrustLineAmount &expectedCommission) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mNextPathNode(nextNodeInThePath),
    mExpectedCommission(expectedCommission)
{}

// DEPRECATED: Constructor without equivalents
CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
    BaseAddress::Shared nextNodeInThePath) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mNextPathNode(nextNodeInThePath)
{}

CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    BytesShared buffer) :

    RequestMessageWithReservations(buffer)
{
    size_t offset = RequestMessageWithReservations::kOffsetToInheritedBytes();

    // Deserialize mNextPathNode (existing field)
    mNextPathNode = deserializeAddress(buffer.get() + offset);
    offset += mNextPathNode->serializedSize();

    // Deserialize expected exchange rate flag
    byte_t hasExchangeRate = buffer.get()[offset];
    offset += sizeof(byte_t);

    if (hasExchangeRate == 1) {
        // Deserialize only rate and shift
        auto amountOffset = buffer.get() + offset;
        auto amountEndOffset = amountOffset + kTrustLineAmountBytesCount;
        vector<byte_t> amountBytes(amountOffset, amountEndOffset);
        TrustLineAmount rate = bytesToTrustLineAmount(amountBytes);
        offset += kTrustLineAmountBytesCount;

        int16_t shift;
        memcpy(&shift, buffer.get() + offset, sizeof(int16_t));
        offset += sizeof(int16_t);

        // Store as pair (rate, shift)
        mExpectedExchangeRate = make_pair(rate, shift);
    }
    // else mExpectedExchangeRate remains nullopt

    // Deserialize expected commission flag
    byte_t hasCommission = buffer.get()[offset];
    offset += sizeof(byte_t);

    if (hasCommission == 1) {
        auto commissionOffset = buffer.get() + offset;
        auto commissionEndOffset = commissionOffset + kTrustLineAmountBytesCount;
        vector<byte_t> commissionBytes(commissionOffset, commissionEndOffset);
        mExpectedCommission = bytesToTrustLineAmount(commissionBytes);
        offset += kTrustLineAmountBytesCount;
    }
    // else mExpectedCommission remains nullopt
}

BaseAddress::Shared CoordinatorReservationRequestMessage::nextNodeInPath() const {
    return mNextPathNode;
}

const optional<pair<TrustLineAmount, int16_t>>&
CoordinatorReservationRequestMessage::expectedExchangeRate() const
{
    return mExpectedExchangeRate;
}

const optional<TrustLineAmount>&
CoordinatorReservationRequestMessage::expectedCommission() const
{
    return mExpectedCommission;
}

const Message::MessageType CoordinatorReservationRequestMessage::typeID() const {
    return Message::Payments_CoordinatorReservationRequest;
}

pair<BytesShared, size_t> CoordinatorReservationRequestMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(RequestMessageWithReservations::serializeToBytes());

    auto serializedAddress = mNextPathNode->serializeToBytes();
    serializer.enqueue(
        serializedAddress,
        mNextPathNode->serializedSize());

    // Serialize expected exchange rate (only rate and shift, NOT equivalents or dates)
    if (mExpectedExchangeRate.has_value()) {
        serializer.copy((byte_t)1);  // Flag: exchange rate present
        serializer.copy(mExpectedExchangeRate->first);  // TrustLineAmount (rate)
        serializer.enqueue(&mExpectedExchangeRate->second, sizeof(int16_t));  // int16_t (shift)
    } else {
        serializer.copy((byte_t)0);  // Flag: no exchange rate
    }

    // Serialize expected commission
    if (mExpectedCommission.has_value()) {
        serializer.copy((byte_t)1);  // Flag: commission present
        serializer.copy(*mExpectedCommission);  // TrustLineAmount
    } else {
        serializer.copy((byte_t)0);  // Flag: no commission
    }

    return serializer.collect();
}
