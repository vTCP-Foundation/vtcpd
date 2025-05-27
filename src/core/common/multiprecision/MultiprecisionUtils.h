#ifndef VTCPD_MULTIPRECISIONUTILS_H
#define VTCPD_MULTIPRECISIONUTILS_H

#include "../../contractors/addresses/GNSAddress.h"
#include "../../contractors/addresses/IPv4WithPortAddress.h"
#include "../Constraints.h"

#include <boost/endian/conversion.hpp>
#include <vector>

using namespace std;

inline TrustLineAmount absoluteBalanceAmount(const TrustLineBalance& balance)
{
    if (balance > TrustLineBalance(0)) {
        return TrustLineAmount(balance);
    } else {
        return TrustLineAmount(-1 * balance);
    }
}

inline vector<byte_t> trustLineAmountToBytes(const TrustLineAmount& amount)
{
    vector<byte_t> rawExportedBytesBuffer, resultBytesBuffer;

    // Exporting bytes of the "amount".
    rawExportedBytesBuffer.reserve(kTrustLineAmountBytesCount);
    export_bits(amount, back_inserter(rawExportedBytesBuffer), 8);

    // Prepending received bytes by zeroes until 32 bytes would be used.
    resultBytesBuffer.reserve(kTrustLineAmountBytesCount);

    size_t unusedBytesCount = kTrustLineAmountBytesCount - rawExportedBytesBuffer.size();
    for (size_t i = 0; i < unusedBytesCount; ++i) {
        resultBytesBuffer.push_back(static_cast<byte_t>(0));
    }

    size_t usedBytesCount = rawExportedBytesBuffer.size();
    for (size_t i = 0; i < usedBytesCount; ++i) {
        resultBytesBuffer.push_back(
            // Casting each byte to big endian makes the deserializer independent
            // from current machine architecture, and, as result - platfrom portable.
            boost::endian::
            conditional_reverse<boost::endian::order::native, boost::endian::order::big>(
                rawExportedBytesBuffer[i]));
    }

    return resultBytesBuffer;
}

inline vector<byte_t> trustLineBalanceToBytes(const TrustLineBalance& balance)
{
    vector<byte_t> rawExportedBytesBuffer, resultBytesBuffer;
    // Exporting bytes of the "balance".
    rawExportedBytesBuffer.reserve(kTrustLineAmountBytesCount);
    export_bits(balance, back_inserter(rawExportedBytesBuffer), 8);

    // Prepending received bytes by zeroes until 32 bytes would be used.
    resultBytesBuffer.reserve(kTrustLineAmountBytesCount + 1);

    size_t unusedBytesCount = kTrustLineAmountBytesCount - rawExportedBytesBuffer.size();
    for (size_t i = 0; i < unusedBytesCount; ++i) {
        resultBytesBuffer.push_back(static_cast<byte_t>(0));
    }

    size_t usedBytesCount = rawExportedBytesBuffer.size();
    for (size_t i = 0; i < usedBytesCount; ++i) {
        resultBytesBuffer.push_back(
            // Casting each byte to big endian makes the deserializer independent
            // from current machine architecture, and, as result - platfrom portable.
            boost::endian::
            conditional_reverse<boost::endian::order::native, boost::endian::order::big>(
                rawExportedBytesBuffer[i]));
    }

    // Process sign
    resultBytesBuffer.insert(
        resultBytesBuffer.begin(),
        boost::endian::conditional_reverse<boost::endian::order::native, boost::endian::order::big>(
            byte_t(balance < 0)));

    return resultBytesBuffer;
}

inline TrustLineAmount bytesToTrustLineAmount(const vector<byte_t> &amountBytes)
{
    vector<byte_t> internalBytesBuffer;
    internalBytesBuffer.reserve(kTrustLineAmountBytesCount);

    for (size_t i = 0; i < kTrustLineAmountBytesCount; ++i) {
        internalBytesBuffer.push_back(boost::endian::conditional_reverse<
                                      boost::endian::order::big,
                                      boost::endian::order::native>(amountBytes[i]));
    }

    TrustLineAmount amount;
    import_bits(amount, internalBytesBuffer.begin(), internalBytesBuffer.end());

    return amount;
}

inline TrustLineBalance bytesToTrustLineBalance(const vector<byte_t> &balanceBytes)
{
    vector<byte_t> internalBytesBuffer;
    internalBytesBuffer.reserve(kTrustLineAmountBytesCount);

    // Note: sign byte must be skipped, so the cycle is starting from 1.
    for (size_t i = 1; i < kTrustLineAmountBytesCount + 1; ++i) {
        internalBytesBuffer.push_back(boost::endian::conditional_reverse<
                                      boost::endian::order::big,
                                      boost::endian::order::native>(balanceBytes[i]));
    }

    TrustLineBalance balance;
    import_bits(balance, internalBytesBuffer.begin(), internalBytesBuffer.end());

    // Sign must be processed only in case if balance != 0.
    // By default, after deserialization, balance is always positive,
    // so it must be only checked for > 0, and not != 0.
    if (balance > 0) {
        byte_t sign = boost::endian::
                      conditional_reverse<boost::endian::order::big, boost::endian::order::native>(
                          balanceBytes[0]);
        if (sign != 0) {
            balance = balance * -1;
        }
    }

    return balance;
}

inline BaseAddress::Shared deserializeAddress(byte_t* offset)
{
    auto addressType = *(reinterpret_cast<BaseAddress::SerializedType*>(offset));

    switch (addressType) {
    case BaseAddress::IPv4_IncludingPort: {
        return make_shared<IPv4WithPortAddress>(offset);
    }
    case BaseAddress::GNS:
        return make_shared<GNSAddress>(offset);
    default: {
        // todo : need correct reaction
        return nullptr;
    }
    }
}

#endif // VTCPD_MULTIPRECISIONUTILS_H
