#include <gtest/gtest.h>
#include <openssl/sha.h>
#include <algorithm>
#include <cstring>

#include "core/network/messages/trust_lines/AuditMessage.h"
#include "core/contractors/Contractor.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/crypto/MsgEncryptor.h"
#include "core/common/Types.h"

namespace {
const SerializedEquivalent kTestEquivalent = 1;
const AuditNumber kTestAuditNumber = 5;
const TrustLineAmount kIncomingAmount = 100;
const TrustLineAmount kOutgoingAmount = 50;
} // namespace

static Contractor::Shared createTestContractor()
{
    auto keys = MsgEncryptor::generateKeyTrio("");
    std::vector<BaseAddress::Shared> addresses;
    addresses.push_back(std::make_shared<IPv4WithPortAddress>(
        std::string("127.0.0.1"), static_cast<uint16_t>(2000)));
    return std::make_shared<Contractor>(
        static_cast<ContractorID>(10),
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

TEST(AuditMessageTest, SerializeEmptyTransactionList)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0xAB);
    std::vector<TransactionUUID> emptyList;

    AuditMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        kTestAuditNumber,
        kIncomingAmount,
        kOutgoingAmount,
        signature,
        emptyList);

    auto serialized = message.serializeToBytes();
    AuditMessage parsed(serialized.first);

    EXPECT_EQ(parsed.auditNumber(), kTestAuditNumber);
    EXPECT_EQ(parsed.incomingAmount(), kIncomingAmount);
    EXPECT_EQ(parsed.outgoingAmount(), kOutgoingAmount);
    EXPECT_TRUE(parsed.transactionUUIDs().empty());
}

TEST(AuditMessageTest, SerializeSingleTransactionUUID)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0xCD);
    TransactionUUID uuid = makeUUID(0x01, 0xAA);

    AuditMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        kTestAuditNumber,
        kIncomingAmount,
        kOutgoingAmount,
        signature,
        {uuid});

    auto serialized = message.serializeToBytes();
    AuditMessage parsed(serialized.first);

    ASSERT_EQ(parsed.transactionUUIDs().size(), 1);
    EXPECT_EQ(parsed.transactionUUIDs().front(), uuid);
}

TEST(AuditMessageTest, SerializeMultipleTransactionUUIDs)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0xEF);
    TransactionUUID uuid1 = makeUUID(0x01, 0x01);
    TransactionUUID uuid2 = makeUUID(0x02, 0x02);
    TransactionUUID uuid3 = makeUUID(0x03, 0x03);

    AuditMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        kTestAuditNumber,
        kIncomingAmount,
        kOutgoingAmount,
        signature,
        {uuid1, uuid2, uuid3});

    auto serialized = message.serializeToBytes();
    AuditMessage parsed(serialized.first);

    ASSERT_EQ(parsed.transactionUUIDs().size(), 3);
    EXPECT_EQ(parsed.transactionUUIDs()[0], uuid1);
    EXPECT_EQ(parsed.transactionUUIDs()[1], uuid2);
    EXPECT_EQ(parsed.transactionUUIDs()[2], uuid3);
}

TEST(AuditMessageTest, DeserializationReconstructsTransactionList)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0x11);
    TransactionUUID uuid1 = makeUUID(0x05, 0x01);
    TransactionUUID uuid2 = makeUUID(0x06, 0x02);

    AuditMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        kTestAuditNumber,
        kIncomingAmount,
        kOutgoingAmount,
        signature,
        {uuid1, uuid2});

    auto serialized = message.serializeToBytes();
    AuditMessage parsed(serialized.first);

    ASSERT_EQ(parsed.transactionUUIDs().size(), 2);
    EXPECT_EQ(parsed.transactionUUIDs()[0], uuid1);
    EXPECT_EQ(parsed.transactionUUIDs()[1], uuid2);
}

TEST(AuditMessageTest, RoundTripSerializationPreservesData)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0x22);
    TransactionUUID transactionUUID = makeUUID(0x99, 0x77);
    TransactionUUID uuid1 = makeUUID(0x10, 0x10);
    TransactionUUID uuid2 = makeUUID(0x11, 0x11);

    AuditMessage message(
        kTestEquivalent,
        contractor,
        transactionUUID,
        kTestAuditNumber,
        kIncomingAmount,
        kOutgoingAmount,
        signature,
        {uuid2, uuid1});

    auto serialized = message.serializeToBytes();
    AuditMessage parsed(serialized.first);

    EXPECT_EQ(parsed.auditNumber(), kTestAuditNumber);
    EXPECT_EQ(parsed.incomingAmount(), kIncomingAmount);
    EXPECT_EQ(parsed.outgoingAmount(), kOutgoingAmount);
    EXPECT_EQ(parsed.transactionUUID(), transactionUUID);
    ASSERT_NE(parsed.signature(), nullptr);
    EXPECT_EQ(0, memcmp(parsed.signature()->data(),
                        signature->data(),
                        signature->signatureSize()));
    ASSERT_EQ(parsed.transactionUUIDs().size(), 2);
    EXPECT_EQ(parsed.transactionUUIDs()[0], uuid1);
    EXPECT_EQ(parsed.transactionUUIDs()[1], uuid2);
}

TEST(AuditMessageTest, TransactionListSortedAfterConstruction)
{
    auto contractor = createTestContractor();
    auto signature = createTestSignature(0x33);
    TransactionUUID uuidA = makeUUID(0x01, 0x01);
    TransactionUUID uuidB = makeUUID(0x00, 0x02);
    TransactionUUID uuidC = makeUUID(0x02, 0x03);

    AuditMessage message(
        kTestEquivalent,
        contractor,
        TransactionUUID(),
        kTestAuditNumber,
        kIncomingAmount,
        kOutgoingAmount,
        signature,
        {uuidA, uuidB, uuidC});

    const auto &list = message.transactionUUIDs();
    ASSERT_EQ(list.size(), 3);
    EXPECT_EQ(list[0], uuidB);
    EXPECT_EQ(list[1], uuidA);
    EXPECT_EQ(list[2], uuidC);
}

TEST(TransactionListSortTest, LexicographicOrderByRawBytes)
{
    TransactionUUID uuidA = makeUUID(0x01, 0x01);
    TransactionUUID uuidB = makeUUID(0x02, 0x02);
    TransactionUUID uuidC = makeUUID(0x03, 0x03);
    std::vector<TransactionUUID> list = {uuidB, uuidC, uuidA};

    AuditMessage::sortTransactionUUIDs(list);

    EXPECT_EQ(list[0], uuidA);
    EXPECT_EQ(list[1], uuidB);
    EXPECT_EQ(list[2], uuidC);
}

TEST(TransactionListSortTest, SortingIsDeterministic)
{
    TransactionUUID uuidA = makeUUID(0x01, 0x01);
    TransactionUUID uuidB = makeUUID(0x02, 0x02);
    std::vector<TransactionUUID> list = {uuidB, uuidA};

    AuditMessage::sortTransactionUUIDs(list);
    auto firstOrder = list;
    AuditMessage::sortTransactionUUIDs(list);

    EXPECT_EQ(firstOrder, list);
}

TEST(TransactionListSortTest, SortingHandlesEmptyList)
{
    std::vector<TransactionUUID> list;
    AuditMessage::sortTransactionUUIDs(list);
    EXPECT_TRUE(list.empty());
}

TEST(TransactionListSortTest, SortingHandlesSingleElement)
{
    std::vector<TransactionUUID> list = {makeUUID(0x01, 0x01)};
    AuditMessage::sortTransactionUUIDs(list);
    EXPECT_EQ(list.size(), 1);
}

TEST(TransactionListSortTest, SortingMatchesCompareTransactionUUIDBehavior)
{
    TransactionUUID uuidA = makeUUID(0x01, 0x01);
    TransactionUUID uuidB = makeUUID(0x00, 0x02);
    TransactionUUID uuidC = makeUUID(0x02, 0x03);
    std::vector<TransactionUUID> list = {uuidA, uuidC, uuidB};

    std::vector<TransactionUUID> expected = list;
    std::sort(expected.begin(), expected.end(), [](const TransactionUUID &left, const TransactionUUID &right) {
        return memcmp(left.data, right.data, TransactionUUID::kBytesSize) < 0;
    });

    AuditMessage::sortTransactionUUIDs(list);
    EXPECT_EQ(list, expected);
}

TEST(TransactionListHashTest, HashOfEmptyListIsDeterministic)
{
    std::vector<TransactionUUID> empty;
    auto hash1 = AuditMessage::computeTransactionListHash(empty);
    auto hash2 = AuditMessage::computeTransactionListHash(empty);

    ASSERT_NE(hash1, nullptr);
    ASSERT_NE(hash2, nullptr);
    EXPECT_EQ(0, memcmp(hash1.get(), hash2.get(), AuditMessage::kTransactionUUIDsHashSize));
}

TEST(TransactionListHashTest, HashOfSingleUUIDHasExpectedSize)
{
    TransactionUUID uuid = makeUUID(0x10, 0x20);
    auto hash = AuditMessage::computeTransactionListHash({uuid});
    ASSERT_NE(hash, nullptr);
    EXPECT_EQ(AuditMessage::kTransactionUUIDsHashSize, SHA256_DIGEST_LENGTH);

    const uint32_t count = 1;
    const size_t bufferSize = sizeof(count) + TransactionUUID::kBytesSize;
    std::vector<uint8_t> buffer(bufferSize);
    memcpy(buffer.data(), &count, sizeof(count));
    memcpy(buffer.data() + sizeof(count), uuid.data, TransactionUUID::kBytesSize);

    unsigned char expected[SHA256_DIGEST_LENGTH];
    SHA256(buffer.data(), bufferSize, expected);

    EXPECT_EQ(0, memcmp(hash.get(), expected, SHA256_DIGEST_LENGTH));
}

TEST(TransactionListHashTest, HashOfMultipleUUIDsIsDeterministic)
{
    TransactionUUID uuid1 = makeUUID(0x01, 0x01);
    TransactionUUID uuid2 = makeUUID(0x02, 0x02);
    auto hash1 = AuditMessage::computeTransactionListHash({uuid1, uuid2});
    auto hash2 = AuditMessage::computeTransactionListHash({uuid1, uuid2});

    EXPECT_EQ(0, memcmp(hash1.get(), hash2.get(), AuditMessage::kTransactionUUIDsHashSize));
}

TEST(TransactionListHashTest, HashChangesWhenListChanges)
{
    TransactionUUID uuid1 = makeUUID(0x01, 0x01);
    TransactionUUID uuid2 = makeUUID(0x02, 0x02);
    auto hash1 = AuditMessage::computeTransactionListHash({uuid1});
    auto hash2 = AuditMessage::computeTransactionListHash({uuid2});

    EXPECT_NE(0, memcmp(hash1.get(), hash2.get(), AuditMessage::kTransactionUUIDsHashSize));
}

TEST(TransactionListHashTest, HashIsOrderIndependentAfterNormalization)
{
    TransactionUUID uuid1 = makeUUID(0x01, 0x01);
    TransactionUUID uuid2 = makeUUID(0x02, 0x02);
    auto hash1 = AuditMessage::computeTransactionListHash({uuid1, uuid2});
    auto hash2 = AuditMessage::computeTransactionListHash({uuid2, uuid1});

    EXPECT_EQ(0, memcmp(hash1.get(), hash2.get(), AuditMessage::kTransactionUUIDsHashSize));
}

TEST(TransactionListHashTest, HashUsesCountAndConcatenatedUUIDs)
{
    TransactionUUID uuid1 = makeUUID(0x01, 0x01);
    TransactionUUID uuid2 = makeUUID(0x02, 0x02);
    std::vector<TransactionUUID> list = {uuid2, uuid1};

    auto hash = AuditMessage::computeTransactionListHash(list);

    std::vector<TransactionUUID> normalized = {uuid1, uuid2};
    const uint32_t count = static_cast<uint32_t>(normalized.size());
    const size_t bufferSize = sizeof(count) + normalized.size() * TransactionUUID::kBytesSize;
    std::vector<uint8_t> buffer(bufferSize);
    size_t offset = 0;
    memcpy(buffer.data() + offset, &count, sizeof(count));
    offset += sizeof(count);
    for (const auto &uuid : normalized) {
        memcpy(buffer.data() + offset, uuid.data, TransactionUUID::kBytesSize);
        offset += TransactionUUID::kBytesSize;
    }

    unsigned char expected[SHA256_DIGEST_LENGTH];
    SHA256(buffer.data(), bufferSize, expected);

    EXPECT_EQ(0, memcmp(hash.get(), expected, SHA256_DIGEST_LENGTH));
}
