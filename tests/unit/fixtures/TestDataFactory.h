#ifndef VTCPD_TESTDATAFACTORY_H
#define VTCPD_TESTDATAFACTORY_H

#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/common/Types.h"
#include "../../../src/core/common/memory/MemoryUtils.h"
#include "../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../src/core/contractors/addresses/BaseAddress.h"
#include "../../../src/core/crypto/lamportkeys.h"
#include "../../../src/core/crypto/lamportscheme.h"
#include "../../../src/core/logger/Logger.h"
#include <memory>
#include <vector>
#include <string>
#include <random>

using namespace std;
using namespace crypto::lamport;

/**
 * Factory class for creating test data objects for all SQLite handler tests.
 * Provides convenient methods to generate valid test data with proper types and sizes.
 */
class TestDataFactory
{
public:
    // Common test data generation
    static ContractorID createValidContractorID();
    static ContractorID createInvalidContractorID();
    static TrustLineID createValidTrustLineID();
    static TrustLineID createInvalidTrustLineID();
    static TransactionUUID createValidTransactionUUID();
    static SerializedEquivalent createValidEquivalent();
    static AuditNumber createValidAuditNumber();
    static BlockNumber createValidBlockNumber();
    static string createValidTableName();
    static string createInvalidTableName();
    static string createValidDirectoryPath();
    static string createInvalidDirectoryPath();
    static string createValidDatabaseName();
    
    // Byte data generation
    static BytesShared createValidByteData(size_t size = 64);
    static BytesShared createEmptyByteData();
    static BytesShared createLargeByteData(size_t size = 1024);
    
    // Lamport cryptographic data
    static KeyHash::Shared createValidKeyHash();
    static Signature::Shared createValidSignature();
    static KeyHash::Shared createEmptyKeyHash();
    static Signature::Shared createEmptySignature();
    
    // Trust line and amounts
    static TrustLineAmount createValidTrustLineAmount();
    static TrustLineAmount createZeroTrustLineAmount();
    static TrustLineBalance createValidTrustLineBalance();
    static TrustLineBalance createZeroTrustLineBalance();
    
    // Address data
    static BaseAddress::Shared createValidGNSAddress();
    static BaseAddress::Shared createValidIPv4Address();
    static BaseAddress::Shared createValidIPv6Address();
    static vector<BaseAddress::Shared> createValidAddressList();
    
    // Feature data
    static string createValidFeatureName();
    static string createValidFeatureValue();
    static string createEmptyFeatureName();
    static string createEmptyFeatureValue();
    static string createLongFeatureName();
    static string createLongFeatureValue();
    
    // Message data
    static Message::SerializedType createValidMessageType();
    static Message::SerializedType createInvalidMessageType();
    
    // Time data
    static GEOEpochTimestamp createValidTimestamp();
    static GEOEpochTimestamp createZeroTimestamp();
    static GEOEpochTimestamp createFutureTimestamp();
    
    // Logger mock
    static unique_ptr<Logger> createMockLogger();
    
    // Database and connection data
    static string createValidConnectionString();
    static string createInvalidConnectionString();
    
    // Key management test data
    static KeysSetSequenceNumber createValidKeysSetSequenceNumber();
    static KeyNumber createValidKeyNumber();
    static bool createValidValidityFlag();
    
    // Observing state data
    static int createValidObservingState();
    static int createInvalidObservingState();
    
    // Utility methods
    static vector<uint8_t> createRandomBytes(size_t size);
    static string createRandomString(size_t length);
    static void seedRandomGenerator(uint32_t seed = 0);

private:
    static mt19937 sRandomGenerator;
    static bool sIsSeeded;
    
    // Private utility methods
    static void ensureSeeded();
    static uint32_t generateRandomUInt32();
    static uint64_t generateRandomUInt64();
    static uint8_t generateRandomByte();
};

/**
 * Specialized test fixtures for specific handlers.
 * Each fixture provides complete test data sets for specific handler scenarios.
 */
class TestFixtures
{
public:
    // Audit handler test data
    struct AuditTestData {
        AuditNumber auditNumber;
        TrustLineID trustLineID;
        KeyHash::Shared ownKeyHash;
        Signature::Shared ownSignature;
        KeyHash::Shared contractorKeyHash;
        Signature::Shared contractorSignature;
        KeyHash::Shared ownKeysSetHash;
        KeyHash::Shared contractorKeysSetHash;
        TrustLineAmount incomingAmount;
        TrustLineAmount outgoingAmount;
        TrustLineBalance balance;
        
        static AuditTestData createValid();
        static AuditTestData createInvalid();
        static AuditTestData createPartial();
    };
    
    // Transaction handler test data
    struct TransactionTestData {
        TransactionUUID transactionUUID;
        BytesShared transactionBody;
        size_t transactionBytesCount;
        
        static TransactionTestData createValid();
        static TransactionTestData createEmpty();
        static TransactionTestData createLarge();
    };
    
    // Features handler test data
    struct FeatureTestData {
        string featureName;
        string featureValue;
        
        static FeatureTestData createValid();
        static FeatureTestData createEmpty();
        static FeatureTestData createLong();
        static vector<FeatureTestData> createMultiple();
    };
    
    // Address handler test data
    struct AddressTestData {
        ContractorID contractorID;
        BaseAddress::AddressType addressType;
        BytesShared addressData;
        size_t addressSize;
        
        static AddressTestData createValid();
        static AddressTestData createInvalid();
        static vector<AddressTestData> createMultiple();
    };
    
    // Payment transaction test data
    struct PaymentTransactionTestData {
        TransactionUUID transactionUUID;
        BlockNumber maximalClaimingBlockNumber;
        int observingState;
        GEOEpochTimestamp recordingTime;
        
        static PaymentTransactionTestData createValid();
        static PaymentTransactionTestData createInvalid();
        static vector<PaymentTransactionTestData> createMultiple();
    };
    
    // Keys handler test data
    struct KeyTestData {
        KeyHash::Shared keyHash;
        TrustLineID trustLineID;
        KeysSetSequenceNumber keysSetSequenceNumber;
        PublicKey::Shared publicKey;
        PrivateKey::Shared privateKey;
        KeyNumber keyNumber;
        bool isValid;
        
        static KeyTestData createValidOwn();
        static KeyTestData createValidContractor();
        static KeyTestData createInvalid();
        static vector<KeyTestData> createMultiple();
    };
    
    // Contractor test data
    struct ContractorTestData {
        ContractorID contractorID;
        ContractorID contractorIDOnContractorSide;
        BytesShared cryptoKey;
        bool isConfirmed;
        
        static ContractorTestData createValid();
        static ContractorTestData createUnconfirmed();
        static vector<ContractorTestData> createMultiple();
    };
    
    // History storage test data
    struct HistoryTestData {
        Record::RecordType recordType;
        SerializedEquivalent equivalent;
        GEOEpochTimestamp timestamp;
        BytesShared recordData;
        size_t recordSize;
        
        static HistoryTestData createValid();
        static HistoryTestData createInvalid();
        static vector<HistoryTestData> createMultiple();
    };
    
    // Communicator messages queue test data
    struct CommunicatorMessageTestData {
        ContractorID contractorID;
        SerializedEquivalent equivalent;
        TransactionUUID transactionUUID;
        Message::SerializedType messageType;
        BytesShared messageData;
        size_t messageBytesCount;
        GEOEpochTimestamp recordingTime;
        
        static CommunicatorMessageTestData createValid();
        static CommunicatorMessageTestData createInvalid();
        static vector<CommunicatorMessageTestData> createMultiple();
    };
};

#endif // VTCPD_TESTDATAFACTORY_H 