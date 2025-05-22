#ifndef VTCPD_MEMORYUTILS_H
#define VTCPD_MEMORYUTILS_H

#include "../Types.h"

#include <cstdint>
#include <memory>

#ifdef MAC_OS
#include <stdlib.h>
#endif

#ifdef LINUX
#include <malloc.h>
#endif

using namespace std;

inline BytesShared tryMalloc(
    size_t bytesCount)
{
    BytesShared bytesShared(
        (byte_t*)malloc(bytesCount),
        free
    );

    if (bytesShared == nullptr) {
        throw std::bad_alloc();
    }
    memset(bytesShared.get(), 0, bytesCount);

    return bytesShared;
}

inline BytesShared tryCalloc(
    size_t bytesCount)
{
    BytesShared bytesShared(
        (byte_t*)calloc(
            bytesCount,
            sizeof(byte_t)
        ),
        free);

    if (bytesShared == nullptr) {
        throw std::bad_alloc();
    }

    return bytesShared;
}
#endif // VTCPD_MEMORYUTILS_H
