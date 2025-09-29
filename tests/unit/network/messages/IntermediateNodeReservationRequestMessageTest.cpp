#include <gtest/gtest.h>

#include "core/network/messages/payments/IntermediateNodeReservationRequestMessage.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/common/Types.h"

#include "core/contractors/Contractor.h"
#include "core/crypto/MsgEncryptor.h"

// This test verifies that serialization followed by deserialization preserves
// all fields of IntermediateNodeReservationRequestMessage, including TransactionUUID
// and the final amounts configuration list.
TEST(IntermediateNodeReservationRequestMessageTest, SerializeDeserializeRoundTrip)
{
	// Arrange: set up deterministic data
	const SerializedEquivalent equivalent = 2;
	std::vector<BaseAddress::Shared> senderAddresses;
	senderAddresses.push_back(std::make_shared<IPv4WithPortAddress>(std::string("10.0.0.5"), static_cast<uint16_t>(8080)));

	const TransactionUUID expectedTxnUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

	// Build a final amounts configuration with multiple entries
	std::vector<std::pair<PathID, ConstSharedTrustLineAmount>> finalCfg;
	// Entry 1: small value
	{
		auto amount = std::make_shared<const TrustLineAmount>(TrustLineAmount(123456u));
		finalCfg.emplace_back(static_cast<PathID>(1), amount);
	}
	// Entry 2: large value to test 256-bit boundary and endianness
	{
		TrustLineAmount big = 0;
		big += 1;        // 1
		big <<= 200;     // 1 << 200
		big += 0xBADC0DEu;
		auto amount = std::make_shared<const TrustLineAmount>(big);
		finalCfg.emplace_back(static_cast<PathID>(6553 % std::numeric_limits<PathID>::max()), amount);
	}

	// Act: build, serialize via base Message API, then deserialize the message
	IntermediateNodeReservationRequestMessage original(
		equivalent,
		senderAddresses,
		expectedTxnUUID,
		finalCfg);

	const Message &baseOriginal = original;
	auto bytesAndSize = baseOriginal.serializeToBytes();
	ASSERT_NE(bytesAndSize.first, nullptr);
	ASSERT_GT(bytesAndSize.second, 0u);

	IntermediateNodeReservationRequestMessage parsed(bytesAndSize.first);

	// Assert: verify all fields are equal after round-trip
	const Message &baseParsed = parsed;
	EXPECT_EQ(baseParsed.typeID(), Message::Payments_IntermediateNodeReservationRequest);
	EXPECT_EQ(parsed.equivalent(), equivalent);
	EXPECT_EQ(parsed.transactionUUID(), expectedTxnUUID);
	EXPECT_EQ(parsed.transactionUUID().stringUUID(), expectedTxnUUID.stringUUID());

	// Sender side fields (idOnReceiverSide + addresses)
	EXPECT_EQ(parsed.idOnReceiverSide, 0u);
	ASSERT_EQ(parsed.senderAddresses.size(), senderAddresses.size());
	for (size_t i = 0; i < senderAddresses.size(); ++i) {
		EXPECT_TRUE(parsed.senderAddresses[i] == senderAddresses[i])
			<< "Sender address at index " << i << " mismatch";
	}

	// Final amounts configuration content check
	const auto &parsedCfg = parsed.finalAmountsConfiguration();
	ASSERT_EQ(parsedCfg.size(), finalCfg.size());
	for (size_t i = 0; i < finalCfg.size(); ++i) {
		EXPECT_EQ(parsedCfg[i].first, finalCfg[i].first);
		ASSERT_NE(finalCfg[i].second, nullptr);
		ASSERT_NE(parsedCfg[i].second, nullptr);
		EXPECT_EQ(*parsedCfg[i].second.get(), *finalCfg[i].second.get());
	}
}

struct ReqMsgParam
{
	// number of addresses: 1 IPv4, or 2 (IPv4 + GNS)
	int addressVariant; // 1 or 2
	bool encrypted;     // whether to simulate encryption path boundary
};

class IntermediateNodeReservationRequestMessageParamTest : public ::testing::TestWithParam<ReqMsgParam> {};

static std::vector<BaseAddress::Shared> buildAddresses(int variant)
{
	std::vector<BaseAddress::Shared> addrs;
	addrs.push_back(std::make_shared<IPv4WithPortAddress>(std::string("10.0.0.5"), static_cast<uint16_t>(8080)));
	if (variant == 2) {
		// GNSAddress expects name@provider with optional :port
		addrs.push_back(std::make_shared<GNSAddress>(std::string("alice@gns.example:1234")));
	}
	return addrs;
}

// This parameterized test tries combinations of address sets and the encrypted header boundary.
// It serializes to bytes (or encrypts payload while keeping the unencrypted header), then deserializes
// and checks UUID, equivalent, addresses and final amounts config.
TEST_P(IntermediateNodeReservationRequestMessageParamTest, SerializeDeserializeRoundTrip)
{
	const auto p = GetParam();
	const SerializedEquivalent equivalent = 3;
	auto senderAddresses = buildAddresses(p.addressVariant);
	const TransactionUUID expectedTxnUUID(std::string("123e4567-e89b-12d3-a456-426655440000"));

	std::vector<std::pair<PathID, ConstSharedTrustLineAmount>> finalCfg;
	{
		auto a = std::make_shared<const TrustLineAmount>(TrustLineAmount(42u));
		finalCfg.emplace_back(static_cast<PathID>(2), a);
	}
	{
		TrustLineAmount big = 0;
		big += 1;
		big <<= 128;
		big += 0xFEEDFACEu;
		auto a = std::make_shared<const TrustLineAmount>(big);
		finalCfg.emplace_back(static_cast<PathID>(7), a);
	}

	IntermediateNodeReservationRequestMessage original(
		equivalent,
		senderAddresses,
		expectedTxnUUID,
		finalCfg);

	// Optionally simulate encryption boundary, which keeps the first UnencryptedHeaderSize bytes
	// and encrypts the rest. Then decrypt back to bytes buffer form.
	std::pair<BytesShared, size_t> bytesAndSize;
	if (!p.encrypted) {
		const Message &baseOriginal = original;
		bytesAndSize = baseOriginal.serializeToBytes();
	} else {
		// Create dummy contractor and encrypt using MsgEncryptor, then decrypt back
		auto keys = MsgEncryptor::generateKeyTrio("");
		// For the test, set contractorPublicKey to the receiver's public key to form a valid seal/open pair
		keys->contractorPublicKey = keys->publicKey;
		std::vector<BaseAddress::Shared> dummy;
		dummy.push_back(std::make_shared<IPv4WithPortAddress>(std::string("127.0.0.1"), static_cast<uint16_t>(9000)));
		Contractor::Shared contractor = std::make_shared<Contractor>(
			static_cast<ContractorID>(11),
			dummy,
			keys);

		Message::Shared msg = std::make_shared<IntermediateNodeReservationRequestMessage>(original);
		msg->encrypt(contractor);

		auto enc = MsgEncryptor(
			contractor->cryptoKey()->contractorPublicKey,
			contractor->cryptoKey()->secretKey).encrypt(msg);
		// Now decrypt what would be on the wire (simulate receiver side)
		auto dec = MsgEncryptor(
			contractor->cryptoKey()->publicKey,
			contractor->cryptoKey()->secretKey).decrypt(enc.first, enc.second);
		ASSERT_NE(dec.first, nullptr);
		ASSERT_GT(dec.second, 0u);
		bytesAndSize = dec;
	}

	IntermediateNodeReservationRequestMessage parsed(bytesAndSize.first);

	// Base header/type via base interface (typeID protected in derived)
	const Message &baseParsed = parsed;
	EXPECT_EQ(baseParsed.typeID(), Message::Payments_IntermediateNodeReservationRequest);
	EXPECT_EQ(parsed.equivalent(), equivalent);
	EXPECT_EQ(parsed.transactionUUID(), expectedTxnUUID);
	EXPECT_EQ(parsed.transactionUUID().stringUUID(), expectedTxnUUID.stringUUID());

	// Sender side fields
	EXPECT_EQ(parsed.idOnReceiverSide, 0u);
	ASSERT_EQ(parsed.senderAddresses.size(), senderAddresses.size());
	for (size_t i = 0; i < senderAddresses.size(); ++i) {
		EXPECT_TRUE(parsed.senderAddresses[i] == senderAddresses[i])
			<< "Sender address at index " << i << " mismatch";
	}

	// Final amounts configuration content check
	const auto &parsedCfg = parsed.finalAmountsConfiguration();
	ASSERT_EQ(parsedCfg.size(), finalCfg.size());
	for (size_t i = 0; i < finalCfg.size(); ++i) {
		EXPECT_EQ(parsedCfg[i].first, finalCfg[i].first);
		ASSERT_NE(finalCfg[i].second, nullptr);
		ASSERT_NE(parsedCfg[i].second, nullptr);
		EXPECT_EQ(*parsedCfg[i].second.get(), *finalCfg[i].second.get());
	}
}

INSTANTIATE_TEST_SUITE_P(
	Combinations,
	IntermediateNodeReservationRequestMessageParamTest,
	::testing::Values(
		ReqMsgParam{1, false},
		ReqMsgParam{2, false},
		ReqMsgParam{1, true},
		ReqMsgParam{2, true}
	));
