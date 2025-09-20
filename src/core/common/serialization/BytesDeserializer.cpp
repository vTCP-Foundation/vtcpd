#include "BytesDeserializer.h"
#include "BytesSerializer.h"
#include "../multiprecision/MultiprecisionUtils.h"
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

    // Use the generic copyInto to maintain offset consistency
    copyInto(&aligned_value, sizeof(uint16_t));
    *v = aligned_value;
}

void BytesDeserializer::copyInto(
    uint32_t* v) noexcept
{
    // Ensure proper alignment for uint32_t (4-byte alignment)
    alignas(uint32_t) uint32_t aligned_value;

    // Use the generic copyInto to maintain offset consistency
    copyInto(&aligned_value, sizeof(uint32_t));
    *v = aligned_value;
}

void BytesDeserializer::copyInto(
    size_t* v) noexcept
{
    // Ensure proper alignment for size_t (8-byte alignment on x64)
    alignas(size_t) size_t aligned_value;

    // Use the generic copyInto to maintain offset consistency
    copyInto(&aligned_value, BytesSerializer::kSerializedSizeTSize);
    *v = aligned_value;
}

void BytesDeserializer::copyInto(
    bool* v) noexcept
{
    // bool is serialized as 1 byte
    byte_t byte_value;

    // Use the generic copyInto to maintain offset consistency
    copyInto(&byte_value, BytesSerializer::kSerializedBoolSize);
    *v = (byte_value != 0);
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
    TrustLineAmount *amount) noexcept
{
    vector<byte_t> amountBytes(
        buffer.get() + mCurrentOffset,
        buffer.get() + mCurrentOffset + BytesSerializer::kSerializedTrustLineAmountSize);

    *amount = bytesToTrustLineAmount(amountBytes);
    mCurrentOffset += BytesSerializer::kSerializedTrustLineAmountSize;
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

void BytesDeserializer::skipBytes(
    const size_t bytesCount) noexcept
{
    mCurrentOffset += bytesCount;
}

BytesShared BytesDeserializer::copyIntoBuffer(
    const size_t bytesCount) noexcept(false)
{
    auto buffer = tryMalloc(bytesCount);
    copyInto(buffer.get(), bytesCount);
    return buffer;
}

size_t BytesDeserializer::getCurrentOffset() const noexcept
{
    return mCurrentOffset;
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
    const size_t* v) noexcept
{
    copyInto(const_cast<size_t*>(v));
}

void BytesDeserializer::copyIntoDespiteConst(
    const bool* v) noexcept
{
    copyInto(const_cast<bool*>(v));
}

void BytesDeserializer::copyIntoDespiteConst(
    const NodeUUID *nodeUUID) noexcept
{
    copyInto(const_cast<NodeUUID*>(nodeUUID));
}
