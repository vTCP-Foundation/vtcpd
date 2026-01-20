#ifndef VTCPD_TYPES_H
#define VTCPD_TYPES_H

#include <boost/multiprecision/cpp_int.hpp>
#include <memory>
#include <cstdint>

using namespace std;

/*
 * Byte
 */
typedef uint8_t byte_t; // todo: switch to std::byte_t (refactoring required)
typedef std::shared_ptr<byte_t> BytesShared;
typedef std::shared_ptr<const byte_t> ConstBytesShared;

/*
 * Trust lines types
 */
namespace multiprecision = boost::multiprecision;
typedef multiprecision::checked_uint256_t TrustLineAmount;
typedef shared_ptr<TrustLineAmount> SharedTrustLineAmount;
typedef shared_ptr<const TrustLineAmount> ConstSharedTrustLineAmount;
typedef uint32_t TrustLineID;
typedef uint32_t KeyNumber;
typedef uint32_t KeysCount;
typedef uint32_t AuditNumber;
// Indicates that a receipt has not been assigned to any audit yet.
static const constexpr AuditNumber kZeroAuditNumber = 0;
typedef uint8_t HopsCount_t;

typedef multiprecision::int256_t TrustLineBalance;
typedef shared_ptr<TrustLineBalance> SharedTrustLineBalance;
typedef shared_ptr<const TrustLineBalance> ConstSharedTrustLineBalance;

typedef uint16_t SerializedRecordsCount;
typedef SerializedRecordsCount SerializedRecordNumber;

typedef uint16_t SerializedResponseCode;

// payments
typedef uint16_t PathID;
typedef uint8_t SerializedPathLengthSize;
typedef SerializedPathLengthSize SerializedPositionInPath;
typedef uint16_t PaymentNodeID;

typedef uint16_t ConfirmationID;

// equivalents
typedef uint32_t SerializedEquivalent;
typedef byte_t SerializedProtocolVersion;

typedef uint32_t ContractorID;

typedef uint64_t BlockNumber;

typedef uint64_t PayloadLength;
typedef uint8_t EquivalentRegisterAddressLength;

typedef uint32_t ProviderParticipantID;

typedef uint16_t SerializedEventType;

#endif // VTCPD_TYPES_H
