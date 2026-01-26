#include <gtest/gtest.h>
#include <cstring>

#include "core/network/messages/trust_line_channels/InitChannelMessage.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/crypto/MsgEncryptor.h"

namespace {
const ContractorID kTestContractorId = 10;
const uint16_t kTestPort = 2000;
const char kTestAddress[] = "127.0.0.1";
const size_t kPaymentKeySize = crypto::sphincs::PublicKey::keySize();

vector<BaseAddress::Shared> createTestAddresses()
{
    vector<BaseAddress::Shared> addresses;
    addresses.push_back(std::make_shared<IPv4WithPortAddress>(kTestAddress, kTestPort));
    return addresses;
}

Contractor::Shared createTestContractor()
{
    auto keys = MsgEncryptor::generateKeyTrio();
    auto addresses = createTestAddresses();
    return std::make_shared<Contractor>(kTestContractorId, addresses, keys);
}

sphincs::PublicKey::Shared createPaymentPublicKey(byte_t fill)
{
    std::vector<byte_t> keyBytes(kPaymentKeySize, fill);
    return std::make_shared<sphincs::PublicKey>(keyBytes.data());
}
} // namespace

TEST(InitChannelMessageTest, Serialization_WithValidPaymentPublicKey_IncludesRawBytes)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0xAB);

    InitChannelMessage message(
        createTestAddresses(),
        TransactionUUID(),
        contractor,
        paymentKey);

    auto [buffer, size] = message.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, kPaymentKeySize);

    const size_t keyOffset = size - kPaymentKeySize;
    EXPECT_EQ(0, memcmp(buffer.get() + keyOffset, paymentKey->data(), kPaymentKeySize));
}

TEST(InitChannelMessageTest, Deserialization_ValidBytes_ExtractsKeyCorrectly)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0x11);

    InitChannelMessage message(
        createTestAddresses(),
        TransactionUUID(),
        contractor,
        paymentKey);

    auto [buffer, size] = message.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, kPaymentKeySize);

    InitChannelMessage parsed(buffer);
    ASSERT_NE(parsed.paymentPublicKey(), nullptr);
    EXPECT_EQ(*parsed.paymentPublicKey(), *paymentKey);
}

TEST(InitChannelMessageTest, RoundTrip_SerializeDeserialize_PreservesFields)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0x7E);
    TransactionUUID uuid("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

    InitChannelMessage message(
        createTestAddresses(),
        uuid,
        contractor,
        paymentKey);

    auto [buffer, size] = message.serializeToBytes();
    ASSERT_NE(buffer, nullptr);
    ASSERT_GT(size, kPaymentKeySize);

    InitChannelMessage parsed(buffer);
    EXPECT_EQ(parsed.contractorID(), contractor->getID());
    ASSERT_NE(parsed.publicKey(), nullptr);
    EXPECT_EQ(0, memcmp(parsed.publicKey()->key, contractor->cryptoKey()->publicKey->key,
                        MsgEncryptor::PublicKey::kBytesSize));
    ASSERT_NE(parsed.paymentPublicKey(), nullptr);
    EXPECT_EQ(*parsed.paymentPublicKey(), *paymentKey);
    EXPECT_EQ(parsed.transactionUUID(), uuid);
}

TEST(InitChannelMessageTest, Serialization_DifferentKeysProduceDifferentPayloads)
{
    auto contractor = createTestContractor();
    auto paymentKeyA = createPaymentPublicKey(0x00);
    auto paymentKeyB = createPaymentPublicKey(0xFF);

    InitChannelMessage messageA(
        createTestAddresses(),
        TransactionUUID(),
        contractor,
        paymentKeyA);

    InitChannelMessage messageB(
        createTestAddresses(),
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
