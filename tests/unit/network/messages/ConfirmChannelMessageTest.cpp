#include <gtest/gtest.h>
#include <cstring>

#include "core/network/messages/trust_line_channels/ConfirmChannelMessage.h"
#include "core/contractors/Contractor.h"
#include "core/crypto/MsgEncryptor.h"

namespace {
const size_t kPaymentKeySize = crypto::sphincs::PublicKey::keySize();

Contractor::Shared createTestContractor()
{
    auto keys = MsgEncryptor::generateKeyTrio();
    return std::make_shared<Contractor>(1, 100, keys, true);
}

sphincs::PublicKey::Shared createPaymentPublicKey(byte_t fill)
{
    std::vector<byte_t> keyBytes(kPaymentKeySize, fill);
    return std::make_shared<sphincs::PublicKey>(keyBytes.data());
}
} // namespace

TEST(ConfirmChannelMessageTest, Serialization_WithValidPaymentPublicKey_IncludesRawBytes)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0xCD);

    ConfirmChannelMessage message(
        TransactionUUID(),
        contractor,
        paymentKey);

    auto [buffer, size] = message.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, kPaymentKeySize);

    const size_t keyOffset = size - kPaymentKeySize;
    EXPECT_EQ(0, memcmp(buffer.get() + keyOffset, paymentKey->data(), kPaymentKeySize));
}

TEST(ConfirmChannelMessageTest, Deserialization_ValidBytes_ExtractsKeyCorrectly)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0x33);

    ConfirmChannelMessage message(
        TransactionUUID(),
        contractor,
        paymentKey);

    auto [buffer, size] = message.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, kPaymentKeySize);

    ConfirmChannelMessage parsed(buffer);
    ASSERT_NE(parsed.paymentPublicKey(), nullptr);
    EXPECT_EQ(*parsed.paymentPublicKey(), *paymentKey);
}

TEST(ConfirmChannelMessageTest, RoundTrip_SerializeDeserialize_PreservesFields)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0x5A);
    TransactionUUID uuid("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

    ConfirmChannelMessage message(
        uuid,
        contractor,
        paymentKey);

    auto [buffer, size] = message.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, kPaymentKeySize);

    ConfirmChannelMessage parsed(buffer);
    ASSERT_NE(parsed.paymentPublicKey(), nullptr);
    EXPECT_EQ(*parsed.paymentPublicKey(), *paymentKey);
    EXPECT_EQ(parsed.transactionUUID(), uuid);
}

TEST(ConfirmChannelMessageTest, Serialization_DifferentKeysProduceDifferentPayloads)
{
    auto contractor = createTestContractor();
    auto paymentKeyA = createPaymentPublicKey(0x00);
    auto paymentKeyB = createPaymentPublicKey(0xFF);

    ConfirmChannelMessage messageA(
        TransactionUUID(),
        contractor,
        paymentKeyA);

    ConfirmChannelMessage messageB(
        TransactionUUID(),
        contractor,
        paymentKeyB);

    auto [bufferA, sizeA] = messageA.serializeToBytes();
    auto [bufferB, sizeB] = messageB.serializeToBytes();
    ASSERT_EQ(sizeA, sizeB);
    ASSERT_GT(sizeA, kPaymentKeySize);

    const size_t keyOffset = sizeA - kPaymentKeySize;
    EXPECT_NE(0, memcmp(bufferA.get() + keyOffset, bufferB.get() + keyOffset, kPaymentKeySize));
}
