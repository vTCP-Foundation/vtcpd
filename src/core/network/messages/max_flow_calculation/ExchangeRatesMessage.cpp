#include "ExchangeRatesMessage.h"

ExchangeRatesMessage::ExchangeRatesMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    vector<ExchangeRate::Shared> exchangeRates) :

    SenderMessage(
        equivalent,
        senderAddresses),

    mExchangeRates(exchangeRates)
{
}

ExchangeRatesMessage::ExchangeRatesMessage(
    BytesShared buffer) : SenderMessage(buffer)
{
    auto bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();

    uint16_t exchangeRatesCnt;
    memcpy(
        &exchangeRatesCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(uint16_t));
    bytesBufferOffset += sizeof(uint16_t);

    for (uint16_t idx = 0; idx < exchangeRatesCnt; idx++) {
        SerializedEquivalent equivalentFrom;
        memcpy(
            &equivalentFrom,
            buffer.get() + bytesBufferOffset,
            sizeof(SerializedEquivalent));
        bytesBufferOffset += sizeof(SerializedEquivalent);

        SerializedEquivalent equivalentTo;
        memcpy(
            &equivalentTo,
            buffer.get() + bytesBufferOffset,
            sizeof(SerializedEquivalent));
        bytesBufferOffset += sizeof(SerializedEquivalent);

        vector<byte_t> rateBytes(kTrustLineAmountBytesCount);
        memcpy(
            rateBytes.data(),
            buffer.get() + bytesBufferOffset,
            kTrustLineAmountBytesCount);
        TrustLineAmount exchangeRate = bytesToTrustLineAmount(rateBytes);
        bytesBufferOffset += kTrustLineAmountBytesCount;

        int16_t exchangeRateShift;
        memcpy(
            &exchangeRateShift,
            buffer.get() + bytesBufferOffset,
            sizeof(int16_t));
        bytesBufferOffset += sizeof(int16_t);

        GEOEpochTimestamp expiresAtTimestamp;
        memcpy(
            &expiresAtTimestamp,
            buffer.get() + bytesBufferOffset,
            sizeof(GEOEpochTimestamp));
        DateTime expiresAt = dateTimeFromGEOEpochTimestamp(expiresAtTimestamp);
        bytesBufferOffset += sizeof(GEOEpochTimestamp);

        vector<byte_t> minAmountBytes(kTrustLineAmountBytesCount);
        memcpy(
            minAmountBytes.data(),
            buffer.get() + bytesBufferOffset,
            kTrustLineAmountBytesCount);
        TrustLineAmount minExchangeAmount = bytesToTrustLineAmount(minAmountBytes);
        bytesBufferOffset += kTrustLineAmountBytesCount;

        vector<byte_t> maxAmountBytes(kTrustLineAmountBytesCount);
        memcpy(
            maxAmountBytes.data(),
            buffer.get() + bytesBufferOffset,
            kTrustLineAmountBytesCount);
        TrustLineAmount maxExchangeAmount = bytesToTrustLineAmount(maxAmountBytes);
        bytesBufferOffset += kTrustLineAmountBytesCount;

        auto rate = make_shared<ExchangeRate>(
            equivalentFrom,
            equivalentTo,
            exchangeRate,
            exchangeRateShift,
            expiresAt,
            minExchangeAmount,
            maxExchangeAmount);

        mExchangeRates.push_back(rate);
    }
}

vector<ExchangeRate::Shared> ExchangeRatesMessage::exchangeRates() const
{
    return mExchangeRates;
}

const Message::MessageType ExchangeRatesMessage::typeID() const
{
    return Message::MaxFlow_ExchangeRates;
}

pair<BytesShared, size_t> ExchangeRatesMessage::serializeToBytes() const
{
    auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount = parentBytesAndCount.second + sizeof(uint16_t);

    for (const auto &rate : mExchangeRates) {
        bytesCount += sizeof(SerializedEquivalent); // equivalentFrom
        bytesCount += sizeof(SerializedEquivalent); // equivalentTo
        bytesCount += kTrustLineAmountBytesCount; // exchangeRate
        bytesCount += sizeof(int16_t); // exchangeRateShift
        bytesCount += sizeof(GEOEpochTimestamp); // expiresAt
        bytesCount += kTrustLineAmountBytesCount; // minExchangeAmount
        bytesCount += kTrustLineAmountBytesCount; // maxExchangeAmount
    }

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    auto exchangeRatesCnt = (uint16_t)mExchangeRates.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &exchangeRatesCnt,
        sizeof(uint16_t));
    dataBytesOffset += sizeof(uint16_t);

    for (const auto &rate : mExchangeRates) {
        SerializedEquivalent equivalentFrom = rate->equivalentFrom();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &equivalentFrom,
            sizeof(SerializedEquivalent));
        dataBytesOffset += sizeof(SerializedEquivalent);

        SerializedEquivalent equivalentTo = rate->equivalentTo();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &equivalentTo,
            sizeof(SerializedEquivalent));
        dataBytesOffset += sizeof(SerializedEquivalent);

        vector<byte_t> rateBytes = trustLineAmountToBytes(rate->exchangeRate());
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            rateBytes.data(),
            kTrustLineAmountBytesCount);
        dataBytesOffset += kTrustLineAmountBytesCount;

        int16_t exchangeRateShift = rate->exchangeRateShift();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &exchangeRateShift,
            sizeof(int16_t));
        dataBytesOffset += sizeof(int16_t);

        GEOEpochTimestamp expiresAtTimestamp = microsecondsSinceGEOEpoch(rate->expiresAt());
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &expiresAtTimestamp,
            sizeof(GEOEpochTimestamp));
        dataBytesOffset += sizeof(GEOEpochTimestamp);

        vector<byte_t> minAmountBytes = trustLineAmountToBytes(rate->minExchangeAmount());
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            minAmountBytes.data(),
            kTrustLineAmountBytesCount);
        dataBytesOffset += kTrustLineAmountBytesCount;

        vector<byte_t> maxAmountBytes = trustLineAmountToBytes(rate->maxExchangeAmount());
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            maxAmountBytes.data(),
            kTrustLineAmountBytesCount);
        dataBytesOffset += kTrustLineAmountBytesCount;
    }

    return make_pair(
        dataBytesShared,
        bytesCount);
}

const size_t ExchangeRatesMessage::kOffsetToInheritedBytes() const
{
    size_t kOffset = SenderMessage::kOffsetToInheritedBytes() + sizeof(uint16_t);
    
    for (const auto &rate : mExchangeRates) {
        kOffset += sizeof(SerializedEquivalent); // equivalentFrom
        kOffset += sizeof(SerializedEquivalent); // equivalentTo
        kOffset += kTrustLineAmountBytesCount; // exchangeRate
        kOffset += sizeof(int16_t); // exchangeRateShift
        kOffset += sizeof(GEOEpochTimestamp); // expiresAt
        kOffset += kTrustLineAmountBytesCount; // minExchangeAmount
        kOffset += kTrustLineAmountBytesCount; // maxExchangeAmount
    }
    
    return kOffset;
}