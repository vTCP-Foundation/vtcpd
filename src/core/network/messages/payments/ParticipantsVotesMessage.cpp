#include "ParticipantsVotesMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

ParticipantsVotesMessage::ParticipantsVotesMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    map<PaymentNodeID, lamport::Signature::Shared> &participantsSignatures) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mParticipantsSignatures(participantsSignatures)
{}

ParticipantsVotesMessage::ParticipantsVotesMessage(
    BytesShared buffer) :
    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();

    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedRecordsCount kRecordsCount;
    deserializer.copyInto(&kRecordsCount);

    for (SerializedRecordNumber i = 0; i < kRecordsCount; ++i) {
        PaymentNodeID paymentNodeID;
        deserializer.copyInto(&paymentNodeID);

        // Read signature data through deserializer to maintain offset consistency
        BytesShared signatureBytes = tryMalloc(lamport::Signature::signatureSize());
        deserializer.copyInto(
            signatureBytes.get(),
            lamport::Signature::signatureSize());

        auto signature = make_shared<lamport::Signature>(signatureBytes.get());

        mParticipantsSignatures.insert(
            make_pair(
                paymentNodeID,
                signature));
    }
}

const Message::MessageType ParticipantsVotesMessage::typeID() const
{
    return Message::Payments_ParticipantsVotes;
}

/**
 * Serializes the message into shared bytes sequence.
 *
 * Message format:
 *  16B - Transaction UUID,
 *  4B  - Total participants count,
 *
 *  { Participant record
 *      2B - Participant 1 PaymentID,
 *      16KB  - Participant 1 signature,
 *  }
 *
 *  ...
 *
 *  { Participant record
 *      2B - Participant N payment ID
 *      16KB  - Participant N signature
 *  }
 */
pair<BytesShared, size_t> ParticipantsVotesMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());

    const auto kTotalParticipantsCount = mParticipantsSignatures.size();
    // Records count
    serializer.copy((SerializedRecordsCount)kTotalParticipantsCount);

    // Payment Node IDs and signatures
    for (const auto &paymentNodeIDAndVote : mParticipantsSignatures) {
        serializer.copy(paymentNodeIDAndVote.first);
        serializer.copy(
            paymentNodeIDAndVote.second->data(),
            lamport::Signature::signatureSize());
    }

    return serializer.collect();
}

const map<PaymentNodeID, lamport::Signature::Shared>& ParticipantsVotesMessage::participantsSignatures() const
{
    return mParticipantsSignatures;
}
