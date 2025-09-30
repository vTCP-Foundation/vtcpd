#include "TestDataFactory.h"
#include "../../../src/core/contractors/addresses/GNSAddress.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/common/time/TimeUtils.h"
#include <chrono>
#include <cstring>
#include <filesystem>

// Static member initialization
mt19937 TestDataFactory::sRandomGenerator;
bool TestDataFactory::sIsSeeded = false;

// Private utility methods
void TestDataFactory::ensureSeeded() {
    if (!sIsSeeded) {
        sRandomGenerator.seed(static_cast<uint32_t>(chrono::steady_clock::now().time_since_epoch().count()));
        sIsSeeded = true;
    }
}

uint32_t TestDataFactory::generateRandomUInt32() {
    ensureSeeded();
    return sRandomGenerator();
}

uint64_t TestDataFactory::generateRandomUInt64() {
    ensureSeeded();
    return (static_cast<uint64_t>(sRandomGenerator()) << 32) | sRandomGenerator();
}

uint8_t TestDataFactory::generateRandomByte() {
    ensureSeeded();
    return static_cast<uint8_t>(sRandomGenerator() & 0xFF);
}

// Public factory methods
ContractorID TestDataFactory::createValidContractorID() {
    return static_cast<ContractorID>(generateRandomUInt32() % 10000 + 1);
}

ContractorID TestDataFactory::createInvalidContractorID() {
    return 0; // Invalid contractor ID
}

TrustLineID TestDataFactory::createValidTrustLineID() {
    return static_cast<TrustLineID>(generateRandomUInt32() % 10000 + 1);
}

TrustLineID TestDataFactory::createInvalidTrustLineID() {
    return 0; // Invalid trust line ID
}

TransactionUUID TestDataFactory::createValidTransactionUUID() {
    TransactionUUID uuid;
    for (size_t i = 0; i < TransactionUUID::kBytesSize; ++i) {
        uuid.data[i] = generateRandomByte();
    }
    return uuid;
}

SerializedEquivalent TestDataFactory::createValidEquivalent() {
    return static_cast<SerializedEquivalent>(generateRandomUInt32() % 100);
}

AuditNumber TestDataFactory::createValidAuditNumber() {
    return static_cast<AuditNumber>(generateRandomUInt32() % 1000 + 1);
}

BlockNumber TestDataFactory::createValidBlockNumber() {
    return static_cast<BlockNumber>(generateRandomUInt64());
}

string TestDataFactory::createValidTableName() {
    return "test_table_" + to_string(generateRandomUInt32());
}

string TestDataFactory::createInvalidTableName() {
    return ""; // Empty table name is invalid
}

string TestDataFactory::createValidDirectoryPath() {
    return "/tmp/vtcp_test_" + to_string(generateRandomUInt32());
}

string TestDataFactory::createInvalidDirectoryPath() {
    return "/invalid/path/that/should/not/exist/ever";
}

string TestDataFactory::createValidDatabaseName() {
    return "test_db_" + to_string(generateRandomUInt32()) + ".db";
}

BytesShared TestDataFactory::createValidByteData(size_t size) {
    auto data = mallocAndAdoptShared<byte>(size);
    for (size_t i = 0; i < size; ++i) {
        data.get()[i] = generateRandomByte();
    }
    return data;
}

BytesShared TestDataFactory::createEmptyByteData() {
    return nullptr;
}

BytesShared TestDataFactory::createLargeByteData(size_t size) {
    return createValidByteData(size);
}

KeyHash::Shared TestDataFactory::createValidKeyHash() {
    auto hash = make_shared<KeyHash>();
    for (size_t i = 0; i < KeyHash::kBytesSize; ++i) {
        hash->data()[i] = generateRandomByte();
    }
    return hash;
}

Signature::Shared TestDataFactory::createValidSignature() {
    auto signature = make_shared<Signature>();
    for (size_t i = 0; i < signature->signatureSize(); ++i) {
        signature->data()[i] = generateRandomByte();
    }
    return signature;
}

KeyHash::Shared TestDataFactory::createEmptyKeyHash() {
    return nullptr;
}

Signature::Shared TestDataFactory::createEmptySignature() {
    return nullptr;
}

TrustLineAmount TestDataFactory::createValidTrustLineAmount() {
    return TrustLineAmount(generateRandomUInt64() % 1000000);
}

TrustLineAmount TestDataFactory::createZeroTrustLineAmount() {
    return TrustLineAmount(0);
}

TrustLineBalance TestDataFactory::createValidTrustLineBalance() {
    return TrustLineBalance(static_cast<int64_t>(generateRandomUInt64() % 1000000));
}

TrustLineBalance TestDataFactory::createZeroTrustLineBalance() {
    return TrustLineBalance(0);
}

BaseAddress::Shared TestDataFactory::createValidGNSAddress() {
    auto address = createRandomString(16);
    return make_shared<GNSAddress>(address);
}

BaseAddress::Shared TestDataFactory::createValidIPv4Address() {
    string ipv4 = to_string(generateRandomByte()) + "." + 
                  to_string(generateRandomByte()) + "." + 
                  to_string(generateRandomByte()) + "." + 
                  to_string(generateRandomByte());
    uint16_t port = static_cast<uint16_t>(generateRandomUInt32() % 65535 + 1);
    return make_shared<IPv4WithPortAddress>(ipv4, port);
}

BaseAddress::Shared TestDataFactory::createValidIPv6Address() {
    // Fallback to IPv4 representation if IPv6 address class is not available in this branch
    return createValidIPv4Address();
}

vector<BaseAddress::Shared> TestDataFactory::createValidAddressList() {
    vector<BaseAddress::Shared> addresses;
    addresses.push_back(createValidGNSAddress());
    addresses.push_back(createValidIPv4Address());
    addresses.push_back(createValidIPv6Address());
    return addresses;
}

string TestDataFactory::createValidFeatureName() {
    return "feature_" + createRandomString(8);
}

string TestDataFactory::createValidFeatureValue() {
    return "value_" + createRandomString(16);
}

string TestDataFactory::createEmptyFeatureName() {
    return "";
}

string TestDataFactory::createEmptyFeatureValue() {
    return "";
}

string TestDataFactory::createLongFeatureName() {
    return createRandomString(1000);
}

string TestDataFactory::createLongFeatureValue() {
    return createRandomString(5000);
}

Message::SerializedType TestDataFactory::createValidMessageType() {
    return static_cast<Message::SerializedType>(generateRandomUInt32() % 100 + 1);
}

Message::SerializedType TestDataFactory::createInvalidMessageType() {
    return 0; // Assuming 0 is invalid
}

GEOEpochTimestamp TestDataFactory::createValidTimestamp() {
    return microsecondsSinceGEOEpoch(utc_now());
}

GEOEpochTimestamp TestDataFactory::createZeroTimestamp() {
    return 0;
}

GEOEpochTimestamp TestDataFactory::createFutureTimestamp() {
    auto future = utc_now() + chrono::days(365);
    return microsecondsSinceGEOEpoch(future);
}

unique_ptr<Logger> TestDataFactory::createMockLogger() {
    // Create a temporary logger for testing
    auto tempDir = filesystem::temp_directory_path() / ("vtcp_test_" + to_string(generateRandomUInt32()));
    filesystem::create_directories(tempDir);
    return make_unique<Logger>(tempDir.string());
}

string TestDataFactory::createValidConnectionString() {
    return "host=localhost port=5432 dbname=test_db user=test password=test";
}

string TestDataFactory::createInvalidConnectionString() {
    return "invalid connection string";
}

KeysSetSequenceNumber TestDataFactory::createValidKeysSetSequenceNumber() {
    return static_cast<KeysSetSequenceNumber>(generateRandomUInt32() % 1000);
}

KeyNumber TestDataFactory::createValidKeyNumber() {
    return static_cast<KeyNumber>(generateRandomUInt32() % 100);
}

bool TestDataFactory::createValidValidityFlag() {
    return (generateRandomUInt32() % 2) == 1;
}

int TestDataFactory::createValidObservingState() {
    return static_cast<int>(generateRandomUInt32() % 10);
}

int TestDataFactory::createInvalidObservingState() {
    return -1; // Assuming negative values are invalid
}

vector<uint8_t> TestDataFactory::createRandomBytes(size_t size) {
    vector<uint8_t> bytes(size);
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = generateRandomByte();
    }
    return bytes;
}

string TestDataFactory::createRandomString(size_t length) {
    const string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[generateRandomUInt32() % charset.length()];
    }
    
    return result;
}

void TestDataFactory::seedRandomGenerator(uint32_t seed) {
    sRandomGenerator.seed(seed);
    sIsSeeded = true;
}

// TestFixtures implementations
TestFixtures::AuditTestData TestFixtures::AuditTestData::createValid() {
    AuditTestData data;
    data.auditNumber = TestDataFactory::createValidAuditNumber();
    data.trustLineID = TestDataFactory::createValidTrustLineID();
    data.ownKeyHash = TestDataFactory::createValidKeyHash();
    data.ownSignature = TestDataFactory::createValidSignature();
    data.contractorKeyHash = TestDataFactory::createValidKeyHash();
    data.contractorSignature = TestDataFactory::createValidSignature();
    data.ownKeysSetHash = TestDataFactory::createValidKeyHash();
    data.contractorKeysSetHash = TestDataFactory::createValidKeyHash();
    data.incomingAmount = TestDataFactory::createValidTrustLineAmount();
    data.outgoingAmount = TestDataFactory::createValidTrustLineAmount();
    data.balance = TestDataFactory::createValidTrustLineBalance();
    return data;
}

TestFixtures::AuditTestData TestFixtures::AuditTestData::createInvalid() {
    AuditTestData data;
    data.auditNumber = 0; // Invalid audit number
    data.trustLineID = TestDataFactory::createInvalidTrustLineID();
    data.ownKeyHash = TestDataFactory::createEmptyKeyHash();
    data.ownSignature = TestDataFactory::createEmptySignature();
    data.contractorKeyHash = TestDataFactory::createEmptyKeyHash();
    data.contractorSignature = TestDataFactory::createEmptySignature();
    data.ownKeysSetHash = TestDataFactory::createEmptyKeyHash();
    data.contractorKeysSetHash = TestDataFactory::createEmptyKeyHash();
    data.incomingAmount = TestDataFactory::createZeroTrustLineAmount();
    data.outgoingAmount = TestDataFactory::createZeroTrustLineAmount();
    data.balance = TestDataFactory::createZeroTrustLineBalance();
    return data;
}

TestFixtures::AuditTestData TestFixtures::AuditTestData::createPartial() {
    AuditTestData data;
    data.auditNumber = TestDataFactory::createValidAuditNumber();
    data.trustLineID = TestDataFactory::createValidTrustLineID();
    data.ownKeyHash = TestDataFactory::createValidKeyHash();
    data.ownSignature = TestDataFactory::createValidSignature();
    data.contractorKeyHash = nullptr; // Partial audit without contractor data
    data.contractorSignature = nullptr;
    data.ownKeysSetHash = TestDataFactory::createValidKeyHash();
    data.contractorKeysSetHash = TestDataFactory::createValidKeyHash();
    data.incomingAmount = TestDataFactory::createValidTrustLineAmount();
    data.outgoingAmount = TestDataFactory::createValidTrustLineAmount();
    data.balance = TestDataFactory::createValidTrustLineBalance();
    return data;
}

TestFixtures::TransactionTestData TestFixtures::TransactionTestData::createValid() {
    TransactionTestData data;
    data.transactionUUID = TestDataFactory::createValidTransactionUUID();
    data.transactionBytesCount = 256;
    data.transactionBody = TestDataFactory::createValidByteData(data.transactionBytesCount);
    return data;
}

TestFixtures::TransactionTestData TestFixtures::TransactionTestData::createEmpty() {
    TransactionTestData data;
    data.transactionUUID = TestDataFactory::createValidTransactionUUID();
    data.transactionBytesCount = 0;
    data.transactionBody = TestDataFactory::createEmptyByteData();
    return data;
}

TestFixtures::TransactionTestData TestFixtures::TransactionTestData::createLarge() {
    TransactionTestData data;
    data.transactionUUID = TestDataFactory::createValidTransactionUUID();
    data.transactionBytesCount = 10240; // 10KB
    data.transactionBody = TestDataFactory::createLargeByteData(data.transactionBytesCount);
    return data;
}

TestFixtures::FeatureTestData TestFixtures::FeatureTestData::createValid() {
    FeatureTestData data;
    data.featureName = TestDataFactory::createValidFeatureName();
    data.featureValue = TestDataFactory::createValidFeatureValue();
    return data;
}

TestFixtures::FeatureTestData TestFixtures::FeatureTestData::createEmpty() {
    FeatureTestData data;
    data.featureName = TestDataFactory::createEmptyFeatureName();
    data.featureValue = TestDataFactory::createEmptyFeatureValue();
    return data;
}

TestFixtures::FeatureTestData TestFixtures::FeatureTestData::createLong() {
    FeatureTestData data;
    data.featureName = TestDataFactory::createLongFeatureName();
    data.featureValue = TestDataFactory::createLongFeatureValue();
    return data;
}

vector<TestFixtures::FeatureTestData> TestFixtures::FeatureTestData::createMultiple() {
    vector<FeatureTestData> features;
    features.push_back(createValid());
    features.push_back(createValid());
    features.push_back(createValid());
    return features;
}

TestFixtures::AddressTestData TestFixtures::AddressTestData::createValid() {
    AddressTestData data;
    data.contractorID = TestDataFactory::createValidContractorID();
    data.addressType = BaseAddress::AddressType::GNS;
    auto address = TestDataFactory::createValidGNSAddress();
    data.addressSize = address->serializedSize();
    data.addressData = mallocAndAdoptShared<byte>(data.addressSize);
    address->serialize(data.addressData.get());
    return data;
}

TestFixtures::AddressTestData TestFixtures::AddressTestData::createInvalid() {
    AddressTestData data;
    data.contractorID = TestDataFactory::createInvalidContractorID();
    data.addressType = static_cast<BaseAddress::AddressType>(-1); // Invalid type
    data.addressSize = 0;
    data.addressData = TestDataFactory::createEmptyByteData();
    return data;
}

vector<TestFixtures::AddressTestData> TestFixtures::AddressTestData::createMultiple() {
    vector<AddressTestData> addresses;
    addresses.push_back(createValid());
    addresses.push_back(createValid());
    addresses.push_back(createValid());
    return addresses;
}

TestFixtures::PaymentTransactionTestData TestFixtures::PaymentTransactionTestData::createValid() {
    PaymentTransactionTestData data;
    data.transactionUUID = TestDataFactory::createValidTransactionUUID();
    data.maximalClaimingBlockNumber = TestDataFactory::createValidBlockNumber();
    data.observingState = TestDataFactory::createValidObservingState();
    data.recordingTime = TestDataFactory::createValidTimestamp();
    return data;
}

TestFixtures::PaymentTransactionTestData TestFixtures::PaymentTransactionTestData::createInvalid() {
    PaymentTransactionTestData data;
    data.transactionUUID = TransactionUUID(); // Empty UUID
    data.maximalClaimingBlockNumber = 0;
    data.observingState = TestDataFactory::createInvalidObservingState();
    data.recordingTime = TestDataFactory::createZeroTimestamp();
    return data;
}

vector<TestFixtures::PaymentTransactionTestData> TestFixtures::PaymentTransactionTestData::createMultiple() {
    vector<PaymentTransactionTestData> transactions;
    transactions.push_back(createValid());
    transactions.push_back(createValid());
    transactions.push_back(createValid());
    return transactions;
}

TestFixtures::KeyTestData TestFixtures::KeyTestData::createValidOwn() {
    KeyTestData data;
    data.keyHash = TestDataFactory::createValidKeyHash();
    data.trustLineID = TestDataFactory::createValidTrustLineID();
    data.keysSetSequenceNumber = TestDataFactory::createValidKeysSetSequenceNumber();
    
    // Create Lamport key pair
    auto keyPair = Keys::generateKeyPair();
    data.publicKey = keyPair.publicKey;
    data.privateKey = keyPair.privateKey;
    data.keyNumber = TestDataFactory::createValidKeyNumber();
    data.isValid = TestDataFactory::createValidValidityFlag();
    
    return data;
}

TestFixtures::KeyTestData TestFixtures::KeyTestData::createValidContractor() {
    KeyTestData data;
    data.keyHash = TestDataFactory::createValidKeyHash();
    data.trustLineID = TestDataFactory::createValidTrustLineID();
    data.keysSetSequenceNumber = TestDataFactory::createValidKeysSetSequenceNumber();
    
    // For contractor keys, we only have public key
    auto keyPair = Keys::generateKeyPair();
    data.publicKey = keyPair.publicKey;
    data.privateKey = nullptr; // Contractor keys don't store private keys
    data.keyNumber = TestDataFactory::createValidKeyNumber();
    data.isValid = TestDataFactory::createValidValidityFlag();
    
    return data;
}

TestFixtures::KeyTestData TestFixtures::KeyTestData::createInvalid() {
    KeyTestData data;
    data.keyHash = TestDataFactory::createEmptyKeyHash();
    data.trustLineID = TestDataFactory::createInvalidTrustLineID();
    data.keysSetSequenceNumber = 0;
    data.publicKey = nullptr;
    data.privateKey = nullptr;
    data.keyNumber = 0;
    data.isValid = false;
    return data;
}

vector<TestFixtures::KeyTestData> TestFixtures::KeyTestData::createMultiple() {
    vector<KeyTestData> keys;
    keys.push_back(createValidOwn());
    keys.push_back(createValidContractor());
    keys.push_back(createValidOwn());
    return keys;
}

TestFixtures::ContractorTestData TestFixtures::ContractorTestData::createValid() {
    ContractorTestData data;
    data.contractorID = TestDataFactory::createValidContractorID();
    data.contractorIDOnContractorSide = TestDataFactory::createValidContractorID();
    data.cryptoKey = TestDataFactory::createValidByteData(64); // Typical crypto key size
    data.isConfirmed = true;
    return data;
}

TestFixtures::ContractorTestData TestFixtures::ContractorTestData::createUnconfirmed() {
    ContractorTestData data;
    data.contractorID = TestDataFactory::createValidContractorID();
    data.contractorIDOnContractorSide = TestDataFactory::createValidContractorID();
    data.cryptoKey = TestDataFactory::createValidByteData(64);
    data.isConfirmed = false;
    return data;
}

vector<TestFixtures::ContractorTestData> TestFixtures::ContractorTestData::createMultiple() {
    vector<ContractorTestData> contractors;
    contractors.push_back(createValid());
    contractors.push_back(createUnconfirmed());
    contractors.push_back(createValid());
    return contractors;
}

TestFixtures::HistoryTestData TestFixtures::HistoryTestData::createValid() {
    HistoryTestData data;
    data.recordType = Record::RecordType::PaymentRecord;
    data.equivalent = TestDataFactory::createValidEquivalent();
    data.timestamp = TestDataFactory::createValidTimestamp();
    data.recordSize = 512;
    data.recordData = TestDataFactory::createValidByteData(data.recordSize);
    return data;
}

TestFixtures::HistoryTestData TestFixtures::HistoryTestData::createInvalid() {
    HistoryTestData data;
    data.recordType = static_cast<Record::RecordType>(-1); // Invalid record type
    data.equivalent = TestDataFactory::createValidEquivalent();
    data.timestamp = TestDataFactory::createZeroTimestamp();
    data.recordSize = 0;
    data.recordData = TestDataFactory::createEmptyByteData();
    return data;
}

vector<TestFixtures::HistoryTestData> TestFixtures::HistoryTestData::createMultiple() {
    vector<HistoryTestData> records;
    records.push_back(createValid());
    records.push_back(createValid());
    records.push_back(createValid());
    return records;
}

TestFixtures::CommunicatorMessageTestData TestFixtures::CommunicatorMessageTestData::createValid() {
    CommunicatorMessageTestData data;
    data.contractorID = TestDataFactory::createValidContractorID();
    data.equivalent = TestDataFactory::createValidEquivalent();
    data.transactionUUID = TestDataFactory::createValidTransactionUUID();
    data.messageType = TestDataFactory::createValidMessageType();
    data.messageBytesCount = 256;
    data.messageData = TestDataFactory::createValidByteData(data.messageBytesCount);
    data.recordingTime = TestDataFactory::createValidTimestamp();
    return data;
}

TestFixtures::CommunicatorMessageTestData TestFixtures::CommunicatorMessageTestData::createInvalid() {
    CommunicatorMessageTestData data;
    data.contractorID = TestDataFactory::createInvalidContractorID();
    data.equivalent = TestDataFactory::createValidEquivalent();
    data.transactionUUID = TransactionUUID(); // Empty UUID
    data.messageType = TestDataFactory::createInvalidMessageType();
    data.messageBytesCount = 0;
    data.messageData = TestDataFactory::createEmptyByteData();
    data.recordingTime = TestDataFactory::createZeroTimestamp();
    return data;
}

vector<TestFixtures::CommunicatorMessageTestData> TestFixtures::CommunicatorMessageTestData::createMultiple() {
    vector<CommunicatorMessageTestData> messages;
    messages.push_back(createValid());
    messages.push_back(createValid());
    messages.push_back(createValid());
    return messages;
} 