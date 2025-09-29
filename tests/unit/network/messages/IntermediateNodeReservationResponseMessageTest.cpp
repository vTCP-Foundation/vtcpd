#include <gtest/gtest.h>

#include "core/network/messages/payments/IntermediateNodeReservationResponseMessage.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/common/Types.h"

// This test verifies that serialization followed by deserialization preserves
// all fields of IntermediateNodeReservationResponseMessage, including TransactionUUID.
TEST(IntermediateNodeReservationResponseMessageTest, SerializeDeserializeRoundTrip)
{
	// Arrange: set up deterministic data
	const SerializedEquivalent equivalent = 1;
	std::vector<BaseAddress::Shared> senderAddresses;
	senderAddresses.push_back(std::make_shared<IPv4WithPortAddress>(std::string("127.0.0.1"), static_cast<uint16_t>(7777)));

	const TransactionUUID expectedTxnUUID(std::string("123e4567-e89b-12d3-a456-426655440000"));
	const PathID expectedPathId = static_cast<PathID>(7);
	const ResponseMessage::OperationState expectedState = ResponseMessage::Accepted;

	// Choose a non-trivial reserved amount to catch endianness/byte-ordering issues
	// Construct a 256-bit value: (1 << 128) + 0xDEADBEEF
	TrustLineAmount expectedAmount = 0;
	expectedAmount += 1;     // 1
	expectedAmount <<= 128;  // 1 << 128
	expectedAmount += 0xDEADBEEFu; // add lower bits

	// Act: build, serialize, then deserialize the message
	IntermediateNodeReservationResponseMessage original(
		equivalent,
		senderAddresses,
		expectedTxnUUID,
		expectedPathId,
		expectedState,
		expectedAmount);

	auto bytesAndSize = original.serializeToBytes();
	ASSERT_NE(bytesAndSize.first, nullptr);
	ASSERT_GT(bytesAndSize.second, 0u);

	IntermediateNodeReservationResponseMessage parsed(bytesAndSize.first);

	// Assert: verify all fields are equal after round-trip
	EXPECT_EQ(parsed.typeID(), Message::Payments_IntermediateNodeReservationResponse);
	EXPECT_EQ(parsed.equivalent(), equivalent);

	// TransactionUUID equality and string form for clearer diagnostics
	EXPECT_EQ(parsed.transactionUUID(), expectedTxnUUID);
	EXPECT_EQ(parsed.transactionUUID().stringUUID(), expectedTxnUUID.stringUUID());

	EXPECT_EQ(parsed.pathID(), expectedPathId);
	EXPECT_EQ(parsed.state(), expectedState);
	EXPECT_EQ(parsed.amountReserved(), expectedAmount);

	// Sender side fields (idOnReceiverSide + addresses)
	EXPECT_EQ(parsed.idOnReceiverSide, 0u);
	ASSERT_EQ(parsed.senderAddresses.size(), senderAddresses.size());
	for (size_t i = 0; i < senderAddresses.size(); ++i) {
		EXPECT_TRUE(parsed.senderAddresses[i] == senderAddresses[i])
			<< "Sender address at index " << i << " mismatch";
	}
}
