#include "BytesDeserializer.h"
#include <cstring>  // for std::memcpy

BytesDeserializer::BytesDeserializer(
    BytesShared buffer,
    size_t initialOffset) noexcept :

    buffer(buffer),
    mCurrentOffset(initialOffset)
{
}

void BytesDeserializer::copyInto(
    byte_t* b) noexcept
{
    copyInto(
        (void*)b,
        sizeof(*b));
}

void BytesDeserializer::copyInto(
    uint16_t* v) noexcept
{
    // Ensure proper alignment for uint16_t (2-byte alignment)
    alignas(uint16_t) uint16_t aligned_value;
    std::memcpy(&aligned_value, buffer.get() + mCurrentOffset, sizeof(uint16_t));
    *v = aligned_value;
    mCurrentOffset += sizeof(uint16_t);
}

void BytesDeserializer::copyInto(
    uint32_t* v) noexcept
{
    // Ensure proper alignment for uint32_t (4-byte alignment)
    alignas(uint32_t) uint32_t aligned_value;
    std::memcpy(&aligned_value, buffer.get() + mCurrentOffset, sizeof(uint32_t));
    *v = aligned_value;
    mCurrentOffset += sizeof(uint32_t);
}

void BytesDeserializer::copyInto(
    NodeUUID *nodeUUID) noexcept
{
    std::copy(
        buffer.get() + mCurrentOffset,
        buffer.get() + mCurrentOffset + NodeUUID::kBytesSize,
        nodeUUID->begin()
    );
    mCurrentOffset += NodeUUID::kBytesSize;
}

void BytesDeserializer::copyInto(
    void* destination,
    const size_t bytesCount) noexcept
{
    // Use std::memcpy for better alignment handling
    std::memcpy(
        destination,
        buffer.get() + mCurrentOffset,
        bytesCount);

    mCurrentOffset += bytesCount;
}

void BytesDeserializer::copyIntoDespiteConst(
    const byte_t* b) noexcept
{
    copyInto(const_cast<byte_t*>(b));
}

void BytesDeserializer::copyIntoDespiteConst(
    const uint16_t* v) noexcept
{
    copyInto(const_cast<uint16_t*>(v));
}

void BytesDeserializer::copyIntoDespiteConst(
    const uint32_t* v) noexcept
{
    copyInto(const_cast<uint32_t*>(v));
}

void BytesDeserializer::copyIntoDespiteConst(
    const NodeUUID *nodeUUID) noexcept
{
    copyInto(const_cast<NodeUUID*>(nodeUUID));
}
