#include "PostgreSQLTestFixtures.h"
#include <cstring>
#include <random>

// Test constants
const std::string PostgreSQLTestFixtures::DEFAULT_TEST_IP = "192.168.1.100";
const uint16_t PostgreSQLTestFixtures::DEFAULT_TEST_PORT = 8080;
const std::string PostgreSQLTestFixtures::DEFAULT_GNS_IDENTIFIER = "testnode@gnsprovider";
const ContractorID PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_ID = 1;
const ContractorID PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_ID_2 = 2;
const ContractorID PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_ID_3 = 3;
const ContractorID PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID = 101;
const size_t PostgreSQLTestFixtures::DEFAULT_CRYPTO_KEY_SIZE = 32;
const TrustLineID PostgreSQLTestFixtures::DEFAULT_TRUST_LINE_ID = 1001;
const TrustLineID PostgreSQLTestFixtures::DEFAULT_TRUST_LINE_ID_2 = 1002;
const TrustLineID PostgreSQLTestFixtures::DEFAULT_TRUST_LINE_ID_3 = 1003;
const SerializedEquivalent PostgreSQLTestFixtures::DEFAULT_EQUIVALENT = 5;
const SerializedEquivalent PostgreSQLTestFixtures::DEFAULT_EQUIVALENT_2 = 10;
const SerializedEquivalent PostgreSQLTestFixtures::DEFAULT_EQUIVALENT_3 = 15;
const TransactionUUID PostgreSQLTestFixtures::DEFAULT_TRANSACTION_UUID = TransactionUUID("12345678-1234-5678-9012-123456789012");
const TransactionUUID PostgreSQLTestFixtures::DEFAULT_TRANSACTION_UUID_2 = TransactionUUID("12345678-1234-5678-9012-123456789013");
const TransactionUUID PostgreSQLTestFixtures::DEFAULT_TRANSACTION_UUID_3 = TransactionUUID("12345678-1234-5678-9012-123456789014");
const size_t PostgreSQLTestFixtures::DEFAULT_TRANSACTION_DATA_SIZE = 1024;
const size_t PostgreSQLTestFixtures::DEFAULT_TRANSACTION_DATA_SIZE_2 = 2048;
const size_t PostgreSQLTestFixtures::DEFAULT_TRANSACTION_DATA_SIZE_3 = 4096;

// Address-related test data
ContractorID PostgreSQLTestFixtures::getValidContractorID()
{
    return DEFAULT_CONTRACTOR_ID;
}

ContractorID PostgreSQLTestFixtures::getInvalidContractorID()
{
    return 0;
}

BaseAddress::Shared PostgreSQLTestFixtures::createIPv4Address(const std::string &host, uint16_t port)
{
    return std::make_shared<IPv4WithPortAddress>(host, port);
}

BaseAddress::Shared PostgreSQLTestFixtures::createGNSAddress(const std::string &identifier)
{
    return std::make_shared<GNSAddress>(identifier);
}

std::vector<BaseAddress::Shared> PostgreSQLTestFixtures::createMixedAddresses()
{
    std::vector<BaseAddress::Shared> addresses;
    addresses.push_back(createIPv4Address("192.168.1.1", 8080));
    addresses.push_back(createGNSAddress("test1@gnsprovider"));
    addresses.push_back(createIPv4Address("10.0.0.1", 9090));
    addresses.push_back(createGNSAddress("test2@gnsprovider"));
    return addresses;
}

std::vector<BaseAddress::Shared> PostgreSQLTestFixtures::createIPv4OnlyAddresses()
{
    std::vector<BaseAddress::Shared> addresses;
    addresses.push_back(createIPv4Address("192.168.1.1", 8080));
    addresses.push_back(createIPv4Address("10.0.0.1", 9090));
    addresses.push_back(createIPv4Address("172.16.0.1", 7070));
    return addresses;
}

std::vector<BaseAddress::Shared> PostgreSQLTestFixtures::createGNSOnlyAddresses()
{
    std::vector<BaseAddress::Shared> addresses;
    addresses.push_back(createGNSAddress("test1@gnsprovider"));
    addresses.push_back(createGNSAddress("test2@gnsprovider"));
    addresses.push_back(createGNSAddress("test3@gnsprovider"));
    return addresses;
}

// Contractor-related test data
ContractorID PostgreSQLTestFixtures::getValidContractorID2()
{
    return DEFAULT_CONTRACTOR_ID_2;
}

ContractorID PostgreSQLTestFixtures::getValidContractorID3()
{
    return DEFAULT_CONTRACTOR_ID_3;
}

ContractorID PostgreSQLTestFixtures::getInvalidContractorSideID()
{
    return 0;
}

MsgEncryptor::KeyTrio::Shared PostgreSQLTestFixtures::createValidCryptoKey()
{
    return MsgEncryptor::generateKeyTrio();
}

MsgEncryptor::KeyTrio::Shared PostgreSQLTestFixtures::createDifferentCryptoKey()
{
    return MsgEncryptor::generateKeyTrio();
}

std::vector<uint8_t> PostgreSQLTestFixtures::createValidCryptoKeyBytes()
{
    std::vector<uint8_t> keyBytes;
    // Generate deterministic test key data
    for (size_t i = 0; i < DEFAULT_CRYPTO_KEY_SIZE; ++i) {
        keyBytes.push_back(static_cast<uint8_t>((i * 7 + 13) % 256));
    }
    return keyBytes;
}

std::vector<uint8_t> PostgreSQLTestFixtures::createDifferentCryptoKeyBytes()
{
    std::vector<uint8_t> keyBytes;
    // Generate different deterministic test key data
    for (size_t i = 0; i < DEFAULT_CRYPTO_KEY_SIZE; ++i) {
        keyBytes.push_back(static_cast<uint8_t>((i * 11 + 17) % 256));
    }
    return keyBytes;
}

// Contractor factory methods
Contractor::Shared PostgreSQLTestFixtures::createBasicContractor(ContractorID id)
{
    auto addresses = createMixedAddresses();
    auto cryptoKey = createValidCryptoKey();
    return std::make_shared<Contractor>(id, addresses, cryptoKey);
}

Contractor::Shared PostgreSQLTestFixtures::createBasicContractorWithAddress(ContractorID id, const std::string &host, uint16_t port)
{
    std::vector<BaseAddress::Shared> addresses;
    addresses.push_back(createIPv4Address(host, port));
    auto cryptoKey = createValidCryptoKey();
    return std::make_shared<Contractor>(id, addresses, cryptoKey);
}

Contractor::Shared PostgreSQLTestFixtures::createFullContractor(ContractorID id, ContractorID idOnContractorSide, bool isConfirmed)
{
    auto cryptoKey = createValidCryptoKey();
    return std::make_shared<Contractor>(id, idOnContractorSide, cryptoKey, isConfirmed);
}

Contractor::Shared PostgreSQLTestFixtures::createUnconfirmedContractor(ContractorID id, ContractorID idOnContractorSide)
{
    return createFullContractor(id, idOnContractorSide, false);
}

Contractor::Shared PostgreSQLTestFixtures::createConfirmedContractor(ContractorID id, ContractorID idOnContractorSide)
{
    return createFullContractor(id, idOnContractorSide, true);
}

std::vector<Contractor::Shared> PostgreSQLTestFixtures::createMultipleContractors(size_t count)
{
    std::vector<Contractor::Shared> contractors;
    for (size_t i = 0; i < count; ++i) {
        contractors.push_back(createBasicContractor(DEFAULT_CONTRACTOR_ID + i));
    }
    return contractors;
}

// TrustLine-related test data
TrustLineID PostgreSQLTestFixtures::getValidTrustLineID()
{
    return DEFAULT_TRUST_LINE_ID;
}

TrustLineID PostgreSQLTestFixtures::getValidTrustLineID2()
{
    return DEFAULT_TRUST_LINE_ID_2;
}

TrustLineID PostgreSQLTestFixtures::getValidTrustLineID3()
{
    return DEFAULT_TRUST_LINE_ID_3;
}

SerializedEquivalent PostgreSQLTestFixtures::getValidEquivalent()
{
    return DEFAULT_EQUIVALENT;
}

SerializedEquivalent PostgreSQLTestFixtures::getValidEquivalent2()
{
    return DEFAULT_EQUIVALENT_2;
}

SerializedEquivalent PostgreSQLTestFixtures::getValidEquivalent3()
{
    return DEFAULT_EQUIVALENT_3;
}

TrustLine::TrustLineState PostgreSQLTestFixtures::getValidTrustLineState()
{
    return TrustLine::TrustLineState::Init;
}

TrustLine::TrustLineState PostgreSQLTestFixtures::getDifferentTrustLineState()
{
    return TrustLine::TrustLineState::Active;
}

// TrustLine factory methods
TrustLine::Shared PostgreSQLTestFixtures::createBasicTrustLine(TrustLineID id, ContractorID contractorID)
{
    return std::make_shared<TrustLine>(id, contractorID, false, TrustLine::TrustLineState::Init);
}

TrustLine::Shared PostgreSQLTestFixtures::createGatewayTrustLine(TrustLineID id, ContractorID contractorID)
{
    return std::make_shared<TrustLine>(id, contractorID, true, TrustLine::TrustLineState::Active);
}

TrustLine::Shared PostgreSQLTestFixtures::createTrustLineWithState(TrustLineID id, ContractorID contractorID, TrustLine::TrustLineState state)
{
    return std::make_shared<TrustLine>(id, contractorID, false, state);
}

TrustLine::Shared PostgreSQLTestFixtures::createFullTrustLine(TrustLineID id, ContractorID contractorID, bool isGateway, TrustLine::TrustLineState state)
{
    return std::make_shared<TrustLine>(id, contractorID, isGateway, state);
}

std::vector<TrustLine::Shared> PostgreSQLTestFixtures::createMultipleTrustLines(size_t count, SerializedEquivalent equivalent)
{
    std::vector<TrustLine::Shared> trustLines;
    for (size_t i = 0; i < count; ++i) {
        trustLines.push_back(createBasicTrustLine(DEFAULT_TRUST_LINE_ID + i, DEFAULT_CONTRACTOR_ID + i));
    }
    return trustLines;
}

std::vector<TrustLine::Shared> PostgreSQLTestFixtures::createTrustLinesForContractor(ContractorID contractorID, size_t count)
{
    std::vector<TrustLine::Shared> trustLines;
    for (size_t i = 0; i < count; ++i) {
        trustLines.push_back(createBasicTrustLine(DEFAULT_TRUST_LINE_ID + i, contractorID));
    }
    return trustLines;
}

// Validation utilities
bool PostgreSQLTestFixtures::isValidContractorID(ContractorID id)
{
    return id > 0;
}

bool PostgreSQLTestFixtures::isValidPort(uint16_t port)
{
    return port > 0 && port <= 65535;
}

bool PostgreSQLTestFixtures::isValidGNSIdentifier(const std::string &identifier)
{
    return !identifier.empty() && identifier.size() <= 255;
}

bool PostgreSQLTestFixtures::isValidCryptoKey(const std::vector<uint8_t> &keyBytes)
{
    return keyBytes.size() >= 32;
}

bool PostgreSQLTestFixtures::isValidTrustLineID(TrustLineID id)
{
    return id > 0;
}

bool PostgreSQLTestFixtures::isValidEquivalent(SerializedEquivalent equivalent)
{
    return equivalent > 0;
}

bool PostgreSQLTestFixtures::isValidTrustLineState(TrustLine::TrustLineState state)
{
    return state >= TrustLine::TrustLineState::Init && state <= TrustLine::TrustLineState::KeysSharing;
}

// Transaction-related test data
TransactionUUID PostgreSQLTestFixtures::getValidTransactionUUID()
{
    return DEFAULT_TRANSACTION_UUID;
}

TransactionUUID PostgreSQLTestFixtures::getValidTransactionUUID2()
{
    return DEFAULT_TRANSACTION_UUID_2;
}

TransactionUUID PostgreSQLTestFixtures::getValidTransactionUUID3()
{
    return DEFAULT_TRANSACTION_UUID_3;
}

BytesShared PostgreSQLTestFixtures::createTestTransactionData(size_t size)
{
    if (size == 0) {
        return nullptr;
    }
    
    auto data = tryMalloc(size);
    if (!data) {
        return nullptr;
    }
    
    // Fill with deterministic test data
    uint8_t* bytePtr = static_cast<uint8_t*>(data.get());
    for (size_t i = 0; i < size; ++i) {
        bytePtr[i] = static_cast<uint8_t>((i * 3 + 17) % 256);
    }
    
    return data;
}

BytesShared PostgreSQLTestFixtures::createDifferentTestTransactionData(size_t size)
{
    if (size == 0) {
        return nullptr;
    }
    
    auto data = tryMalloc(size);
    if (!data) {
        return nullptr;
    }
    
    // Fill with different deterministic test data
    uint8_t* bytePtr = static_cast<uint8_t*>(data.get());
    for (size_t i = 0; i < size; ++i) {
        bytePtr[i] = static_cast<uint8_t>((i * 5 + 23) % 256);
    }
    
    return data;
}

size_t PostgreSQLTestFixtures::getValidTransactionDataSize()
{
    return DEFAULT_TRANSACTION_DATA_SIZE;
}

size_t PostgreSQLTestFixtures::getValidTransactionDataSize2()
{
    return DEFAULT_TRANSACTION_DATA_SIZE_2;
}

size_t PostgreSQLTestFixtures::getValidTransactionDataSize3()
{
    return DEFAULT_TRANSACTION_DATA_SIZE_3;
}

bool PostgreSQLTestFixtures::isValidTransactionUUID(const TransactionUUID &uuid)
{
    return uuid.stringUUID() != "00000000-0000-0000-0000-000000000000";
}

bool PostgreSQLTestFixtures::isValidTransactionData(BytesShared data, size_t size)
{
    return data != nullptr && size > 0;
} 