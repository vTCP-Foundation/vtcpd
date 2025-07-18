#ifndef VTCPD_POSTGRESQLTESTFIXTURES_H
#define VTCPD_POSTGRESQLTESTFIXTURES_H

#include "../../../../src/core/common/Types.h"
#include "../../../../src/core/contractors/addresses/BaseAddress.h"
#include "../../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../../src/core/contractors/addresses/GNSAddress.h"
#include "../../../../src/core/contractors/Contractor.h"
#include "../../../../src/core/crypto/MsgEncryptor.h"
#include "../../../../src/core/trust_lines/TrustLine.h"
#include "../../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../../src/core/common/memory/MemoryUtils.h"
#include <memory>
#include <vector>

class PostgreSQLTestFixtures
{
public:
    // Address-related test data
    static ContractorID getValidContractorID();
    static ContractorID getInvalidContractorID();
    static BaseAddress::Shared createIPv4Address(const std::string &host, uint16_t port);
    static BaseAddress::Shared createGNSAddress(const std::string &identifier);
    static std::vector<BaseAddress::Shared> createMixedAddresses();
    static std::vector<BaseAddress::Shared> createIPv4OnlyAddresses();
    static std::vector<BaseAddress::Shared> createGNSOnlyAddresses();
    
    // Contractor-related test data
    static ContractorID getValidContractorID2();
    static ContractorID getValidContractorID3();
    static ContractorID getInvalidContractorSideID();
    static MsgEncryptor::KeyTrio::Shared createValidCryptoKey();
    static MsgEncryptor::KeyTrio::Shared createDifferentCryptoKey();
    static std::vector<uint8_t> createValidCryptoKeyBytes();
    static std::vector<uint8_t> createDifferentCryptoKeyBytes();
    
    // Contractor factory methods
    static Contractor::Shared createBasicContractor(ContractorID id);
    static Contractor::Shared createBasicContractorWithAddress(ContractorID id, const std::string &host, uint16_t port);
    static Contractor::Shared createFullContractor(ContractorID id, ContractorID idOnContractorSide, bool isConfirmed);
    static Contractor::Shared createUnconfirmedContractor(ContractorID id, ContractorID idOnContractorSide);
    static Contractor::Shared createConfirmedContractor(ContractorID id, ContractorID idOnContractorSide);
    static std::vector<Contractor::Shared> createMultipleContractors(size_t count);
    
    // TrustLine-related test data
    static TrustLineID getValidTrustLineID();
    static TrustLineID getValidTrustLineID2();
    static TrustLineID getValidTrustLineID3();
    static SerializedEquivalent getValidEquivalent();
    static SerializedEquivalent getValidEquivalent2();
    static SerializedEquivalent getValidEquivalent3();
    static TrustLine::TrustLineState getValidTrustLineState();
    static TrustLine::TrustLineState getDifferentTrustLineState();
    
    // TrustLine factory methods
    static TrustLine::Shared createBasicTrustLine(TrustLineID id, ContractorID contractorID);
    static TrustLine::Shared createGatewayTrustLine(TrustLineID id, ContractorID contractorID);
    static TrustLine::Shared createTrustLineWithState(TrustLineID id, ContractorID contractorID, TrustLine::TrustLineState state);
    static TrustLine::Shared createFullTrustLine(TrustLineID id, ContractorID contractorID, bool isGateway, TrustLine::TrustLineState state);
    static std::vector<TrustLine::Shared> createMultipleTrustLines(size_t count, SerializedEquivalent equivalent);
    static std::vector<TrustLine::Shared> createTrustLinesForContractor(ContractorID contractorID, size_t count);
    
    // Transaction-related test data
    static TransactionUUID getValidTransactionUUID();
    static TransactionUUID getValidTransactionUUID2();
    static TransactionUUID getValidTransactionUUID3();
    static BytesShared createTestTransactionData(size_t size);
    static BytesShared createDifferentTestTransactionData(size_t size);
    static size_t getValidTransactionDataSize();
    static size_t getValidTransactionDataSize2();
    static size_t getValidTransactionDataSize3();
    
    // Validation utilities
    static bool isValidContractorID(ContractorID id);
    static bool isValidPort(uint16_t port);
    static bool isValidGNSIdentifier(const std::string &identifier);
    static bool isValidCryptoKey(const std::vector<uint8_t> &keyBytes);
    static bool isValidTrustLineID(TrustLineID id);
    static bool isValidEquivalent(SerializedEquivalent equivalent);
    static bool isValidTrustLineState(TrustLine::TrustLineState state);
    static bool isValidTransactionUUID(const TransactionUUID &uuid);
    static bool isValidTransactionData(BytesShared data, size_t size);
    
    // Test constants
    static const std::string DEFAULT_TEST_IP;
    static const uint16_t DEFAULT_TEST_PORT;
    static const std::string DEFAULT_GNS_IDENTIFIER;
    static const ContractorID DEFAULT_CONTRACTOR_ID;
    static const ContractorID DEFAULT_CONTRACTOR_ID_2;
    static const ContractorID DEFAULT_CONTRACTOR_ID_3;
    static const ContractorID DEFAULT_CONTRACTOR_SIDE_ID;
    static const size_t DEFAULT_CRYPTO_KEY_SIZE;
    static const TrustLineID DEFAULT_TRUST_LINE_ID;
    static const TrustLineID DEFAULT_TRUST_LINE_ID_2;
    static const TrustLineID DEFAULT_TRUST_LINE_ID_3;
    static const SerializedEquivalent DEFAULT_EQUIVALENT;
    static const SerializedEquivalent DEFAULT_EQUIVALENT_2;
    static const SerializedEquivalent DEFAULT_EQUIVALENT_3;
    static const TransactionUUID DEFAULT_TRANSACTION_UUID;
    static const TransactionUUID DEFAULT_TRANSACTION_UUID_2;
    static const TransactionUUID DEFAULT_TRANSACTION_UUID_3;
    static const size_t DEFAULT_TRANSACTION_DATA_SIZE;
    static const size_t DEFAULT_TRANSACTION_DATA_SIZE_2;
    static const size_t DEFAULT_TRANSACTION_DATA_SIZE_3;
};

#endif // VTCPD_POSTGRESQLTESTFIXTURES_H 