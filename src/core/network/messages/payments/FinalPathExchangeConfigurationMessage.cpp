#include "FinalPathExchangeConfigurationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

FinalPathExchangeConfigurationMessage::FinalPathExchangeConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PathID &pathID,
    const TrustLineAmount &incomingAmount,
    const SerializedEquivalent &incomingEquivalent,
    const TrustLineAmount &outgoingAmount,
    const SerializedEquivalent &outgoingEquivalent) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPathID(pathID),
    mIncomingAmount(incomingAmount),
    mIncomingEquivalent(incomingEquivalent),
    mOutgoingAmount(outgoingAmount),
    mOutgoingEquivalent(outgoingEquivalent)
{
}

FinalPathExchangeConfigurationMessage::FinalPathExchangeConfigurationMessage(
    BytesShared buffer) :

    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);

    deserializer.copyInto(&mPathID);
    currentOffset += BytesSerializer::kSerializedPathIDSize;

    deserializer = BytesDeserializer(buffer, currentOffset);
    deserializer.copyInto(&mIncomingAmount);
    currentOffset += BytesSerializer::kSerializedTrustLineAmountSize;

    deserializer = BytesDeserializer(buffer, currentOffset);
    deserializer.copyInto(&mIncomingEquivalent);
    currentOffset += sizeof(SerializedEquivalent);

    deserializer = BytesDeserializer(buffer, currentOffset);
    deserializer.copyInto(&mOutgoingAmount);
    currentOffset += BytesSerializer::kSerializedTrustLineAmountSize;

    deserializer = BytesDeserializer(buffer, currentOffset);
    deserializer.copyInto(&mOutgoingEquivalent);
}

const PathID& FinalPathExchangeConfigurationMessage::pathID() const
{
    return mPathID;
}

const TrustLineAmount& FinalPathExchangeConfigurationMessage::incomingAmount() const
{
    return mIncomingAmount;
}

const SerializedEquivalent& FinalPathExchangeConfigurationMessage::incomingEquivalent() const
{
    return mIncomingEquivalent;
}

const TrustLineAmount& FinalPathExchangeConfigurationMessage::outgoingAmount() const
{
    return mOutgoingAmount;
}

const SerializedEquivalent& FinalPathExchangeConfigurationMessage::outgoingEquivalent() const
{
    return mOutgoingEquivalent;
}

const Message::MessageType FinalPathExchangeConfigurationMessage::typeID() const
{
    return Message::Payments_FinalPathExchangeConfiguration;
}

pair<BytesShared, size_t> FinalPathExchangeConfigurationMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy(mPathID);
    serializer.copy(mIncomingAmount);
    serializer.copy(mIncomingEquivalent);
    serializer.copy(mOutgoingAmount);
    serializer.copy(mOutgoingEquivalent);

    return serializer.collect();
}

const size_t FinalPathExchangeConfigurationMessage::kOffsetToInheritedBytes() const
{
    const size_t offset =
        TransactionMessage::kOffsetToInheritedBytes() +
        BytesSerializer::kSerializedPathIDSize +
        BytesSerializer::kSerializedTrustLineAmountSize +
        sizeof(SerializedEquivalent) +
        BytesSerializer::kSerializedTrustLineAmountSize +
        sizeof(SerializedEquivalent);

    return offset;
}
