#include <gtest/gtest.h>
#include <cstring>

#include "core/network/messages/trust_lines/AuditResponseMessage.h"
#include "core/contractors/Contractor.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/crypto/MsgEncryptor.h"

namespace {
const SerializedEquivalent kTestEquivalent = 2;
} // namespace

static Contractor::Shared createTestContractor()
{
    auto keys = MsgEncryptor::generateKeyTrio("");
    std::vector<BaseAddress::Shared> addresses;
    addresses.push_back(std::make_shared<IPv4WithPortAddress>(
        std::string("127.0.0.2"), static_cast<uint16_t>(2001)));
    return std::make_shared<Contractor>(
        static_cast<ContractorID>(11),
        addresses,
        keys);
}

static sphincs::Signature::Shared createTestSignature(uint8_t fill)
{
    std::vector<uint8_t> signatureData(sphincs::Signature::signatureSize(), fill);
    return std::make_shared<sphincs::Signature>(signatureData.data());
}

static TransactionUUID makeUUID(uint8_t firstByte, uint8_t lastByte)
{
    uint8_t bytes[TransactionUUID::kBytesSize];
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = firstByte;
    bytes[TransactionUUID::kBytesSize - 1] = lastByte;
    return TransactionUUID(bytes);
}

TEST(AuditResponseMessageTest, SerializeEmptyTransactionList)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0xAA);

    AuditResponseMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        signature,
        {});

    auto serialized = message.serializeToBytes();
    AuditResponseMessage parsed(serialized.first);

    EXPECT_EQ(parsed.state(), ConfirmationMessage::OK);
    EXPECT_TRUE(parsed.transactionUUIDs().empty());
    ASSERT_NE(parsed.signature(), nullptr);
    EXPECT_EQ(0, memcmp(parsed.signature()->data(),
                        signature->data(),
                        signature->signatureSize()));
}

TEST(AuditResponseMessageTest, SerializeTransactionListForUpdateState)
{
    auto contractor = createTestContractor();
    TransactionUUID uuid1 = makeUUID(0x01, 0x01);
    TransactionUUID uuid2 = makeUUID(0x02, 0x02);

    AuditResponseMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        ConfirmationMessage::Audit_UpdateTransactionsList,
        {uuid2, uuid1});

    auto serialized = message.serializeToBytes();
    AuditResponseMessage parsed(serialized.first);

    EXPECT_EQ(parsed.state(), ConfirmationMessage::Audit_UpdateTransactionsList);
    ASSERT_EQ(parsed.transactionUUIDs().size(), 2);
    EXPECT_EQ(parsed.transactionUUIDs()[0], uuid1);
    EXPECT_EQ(parsed.transactionUUIDs()[1], uuid2);
    EXPECT_EQ(parsed.signature(), nullptr);
}

TEST(AuditResponseMessageTest, DeserializationReconstructsTransactionList)
{
    auto contractor = createTestContractor();
    TransactionUUID uuid1 = makeUUID(0x03, 0x03);
    TransactionUUID uuid2 = makeUUID(0x04, 0x04);

    AuditResponseMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        ConfirmationMessage::Audit_UpdateTransactionsList,
        {uuid1, uuid2});

    auto serialized = message.serializeToBytes();
    AuditResponseMessage parsed(serialized.first);

    ASSERT_EQ(parsed.transactionUUIDs().size(), 2);
    EXPECT_EQ(parsed.transactionUUIDs()[0], uuid1);
    EXPECT_EQ(parsed.transactionUUIDs()[1], uuid2);
}

TEST(AuditResponseMessageTest, RoundTripSerializationPreservesData)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0xBB);
    TransactionUUID uuid1 = makeUUID(0x05, 0x05);
    TransactionUUID uuid2 = makeUUID(0x06, 0x06);

    AuditResponseMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        signature,
        {uuid2, uuid1});

    auto serialized = message.serializeToBytes();
    AuditResponseMessage parsed(serialized.first);

    EXPECT_EQ(parsed.state(), ConfirmationMessage::OK);
    ASSERT_EQ(parsed.transactionUUIDs().size(), 2);
    EXPECT_EQ(parsed.transactionUUIDs()[0], uuid1);
    EXPECT_EQ(parsed.transactionUUIDs()[1], uuid2);
    ASSERT_NE(parsed.signature(), nullptr);
    EXPECT_EQ(0, memcmp(parsed.signature()->data(),
                        signature->data(),
                        signature->signatureSize()));
}
