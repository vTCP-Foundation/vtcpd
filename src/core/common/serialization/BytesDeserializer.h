#ifndef VTCPD_BYTESDESERIALIZER_H
#define VTCPD_BYTESDESERIALIZER_H

#include "../Types.h"
#include "../NodeUUID.h"
#include "../memory/MemoryUtils.h"

using namespace std;

class BytesDeserializer
{
public:
    BytesShared buffer;

public:
    BytesDeserializer(
        BytesShared buffer,
        size_t initialOffset = 0) noexcept;

    void copyInto(
        byte_t* b) noexcept;

    void copyIntoDespiteConst(
        const byte_t* b) noexcept;

    void copyInto(
        uint16_t* v) noexcept;

    void copyIntoDespiteConst(
        const uint16_t* v) noexcept;

    void copyInto(
        uint32_t* v) noexcept;

    void copyIntoDespiteConst(
        const uint32_t* v) noexcept;

    void copyInto(
        size_t* v) noexcept;

    void copyIntoDespiteConst(
        const size_t* v) noexcept;

    void copyInto(
        bool* v) noexcept;

    void copyIntoDespiteConst(
        const bool* v) noexcept;

    void copyInto(
        NodeUUID* nodeUUID) noexcept;

    void copyInto(
        TrustLineAmount* amount) noexcept;

    void copyIntoDespiteConst(
        const NodeUUID* nodeUUID) noexcept;

    void copyInto(
        void* destination,
        const size_t bytesCount) noexcept;

    // Utility methods for centralized offset management
    void skipBytes(
        const size_t bytesCount) noexcept;

    BytesShared copyIntoBuffer(
        const size_t bytesCount) noexcept(false);

    // Get current offset for external operations that need raw pointer access
    size_t getCurrentOffset() const noexcept;

protected:
    size_t mCurrentOffset;
};

#endif // VTCPD_BYTESDESERIALIZER_H
