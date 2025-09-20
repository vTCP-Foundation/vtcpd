#include "EquivalentMessage.h"
#include "../../common/serialization/BytesDeserializer.h"

EquivalentMessage::EquivalentMessage(
    const SerializedEquivalent equivalent):
    mEquivalent(equivalent)
{}

EquivalentMessage::EquivalentMessage(
    BytesShared buffer)
{
    BytesDeserializer deserializer(buffer, Message::kOffsetToInheritedBytes());
    deserializer.copyInto(&mEquivalent);
}

const SerializedEquivalent EquivalentMessage::equivalent() const
{
    return mEquivalent;
}

pair<BytesShared, size_t> EquivalentMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(Message::serializeToBytes());
    serializer.copy(mEquivalent);

    return serializer.collect();
}

const size_t EquivalentMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        Message::kOffsetToInheritedBytes()
        + BytesSerializer::kSerializedEquivalentSize;
    return kOffset;
}
