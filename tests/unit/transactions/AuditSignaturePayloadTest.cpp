#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <cstring>

#include "core/transactions/transactions/trust_lines/base/BaseTrustLineTransaction.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/contractors/ContractorsManager.h"
#include "core/crypto/keychain.h"
#include "core/features/FeaturesManager.h"
#include "core/subsystems_controller/TrustLinesInfluenceController.h"
#include "core/common/Types.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"

namespace {
const SerializedEquivalent kTestEquivalent = 1;
const AuditNumber kTestAuditNumber = 4;
const std::string kRegistryAddress = "registry.example.local";
const std::string kOwnAddresses = "127.0.0.1:2000";
} // namespace

class AuditSignaturePayloadTransaction final : public BaseTrustLineTransaction {
public:
    AuditSignaturePayloadTransaction(
        const SerializedEquivalent equivalent,
        ContractorID contractorID,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLines,
        StorageHandler *storageHandler,
        Keystore *keystore,
        FeaturesManager *featuresManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger) :
        BaseTrustLineTransaction(
            BaseTransaction::AuditSourceTransactionType,
            equivalent,
            contractorID,
            contractorsManager,
            trustLines,
            storageHandler,
            keystore,
            featuresManager,
            trustLinesInfluenceController,
            logger)
    {}

    TransactionResult::SharedConst run() override {
        return resultDone();
    }

    const std::string logHeader() const override {
        return "AuditSignaturePayloadTransaction";
    }

    using BaseTrustLineTransaction::getOwnSerializedAuditDataWithTransactionHash;

    void setAuditContext(
        AuditNumber auditNumber,
        TrustLineBalance auditBalance,
        const std::vector<TransactionUUID> &transactionUUIDs)
    {
        mAuditNumber = auditNumber;
        mAuditBalance = auditBalance;
        mCurrentTransactionList = transactionUUIDs;
    }
};

class AuditSignaturePayloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        tempDir = std::filesystem::temp_directory_path()
            / ("vtcpd_audit_payload_test_" + suffix);
        std::filesystem::create_directories(tempDir);

        logger = std::make_unique<Logger>();
        storage = std::make_unique<StorageHandlerSQLite>(
            tempDir.string(),
            "audit_payload_test.db",
            *logger);

        std::vector<std::pair<std::string, std::string>> ownAddresses = {
            {"ipv4", kOwnAddresses}
        };
        contractorsManager = std::make_unique<ContractorsManager>(
            ownAddresses,
            storage.get(),
            *logger);

        keystore = std::make_unique<crypto::Keystore>(*logger);

        trustLinesManager = std::make_unique<TrustLinesManager>(
            kTestEquivalent,
            storage.get(),
            keystore.get(),
            contractorsManager.get(),
            *logger);

        auto ioTransaction = storage->beginTransaction();
        std::vector<BaseAddress::Shared> contractorAddresses;
        contractorAddresses.push_back(std::make_shared<IPv4WithPortAddress>(
            std::string("127.0.0.1"), static_cast<uint16_t>(3001)));
        auto contractor = contractorsManager->createContractor(
            ioTransaction,
            contractorAddresses);
        contractorID = contractor->getID();

        trustLinesManager->open(contractorID, ioTransaction);
        ioTransaction->rollback();

        trustLinesManager->setIncoming(contractorID, TrustLineAmount(1000));
        trustLinesManager->setOutgoing(contractorID, TrustLineAmount(2000));

        trustLinesManager->trustLines()[contractorID]->setBalance(
            TrustLineBalance(300));

        influenceController = std::make_unique<TrustLinesInfluenceController>(*logger);
        featuresManager = std::make_unique<FeaturesManager>(
            ioContext,
            kRegistryAddress,
            kOwnAddresses,
            storage.get(),
            *logger);
    }

    void TearDown() override {
        featuresManager.reset();
        influenceController.reset();
        trustLinesManager.reset();
        keystore.reset();
        contractorsManager.reset();
        storage.reset();
        logger.reset();
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
    as::io_context ioContext;
    ContractorID contractorID = 0;
    std::unique_ptr<Logger> logger;
    std::unique_ptr<StorageHandlerSQLite> storage;
    std::unique_ptr<ContractorsManager> contractorsManager;
    std::unique_ptr<crypto::Keystore> keystore;
    std::unique_ptr<TrustLinesManager> trustLinesManager;
    std::unique_ptr<TrustLinesInfluenceController> influenceController;
    std::unique_ptr<FeaturesManager> featuresManager;
};

static TransactionUUID makeUUID(uint8_t firstByte, uint8_t lastByte)
{
    uint8_t bytes[TransactionUUID::kBytesSize];
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = firstByte;
    bytes[TransactionUUID::kBytesSize - 1] = lastByte;
    return TransactionUUID(bytes);
}

TEST_F(AuditSignaturePayloadTest, SignaturePayloadIncludesTransactionListHash) {
    AuditSignaturePayloadTransaction transaction(
        kTestEquivalent,
        contractorID,
        contractorsManager.get(),
        trustLinesManager.get(),
        storage.get(),
        keystore.get(),
        featuresManager.get(),
        influenceController.get(),
        *logger);

    std::vector<TransactionUUID> list = {
        makeUUID(0x02, 0x02),
        makeUUID(0x01, 0x01)
    };
    transaction.setAuditContext(
        kTestAuditNumber,
        TrustLineBalance(300),
        list);

    auto payload = transaction.getOwnSerializedAuditDataWithTransactionHash();
    auto expectedHash = AuditMessage::computeTransactionListHash(list);

    const size_t hashOffset =
        sizeof(AuditNumber) +
        kTrustLineAmountBytesCount +
        kTrustLineAmountBytesCount +
        kTrustLineBalanceSerializeBytesCount;

    EXPECT_EQ(0, memcmp(
        payload.first.get() + hashOffset,
        expectedHash.get(),
        AuditMessage::kTransactionUUIDsHashSize));
}

TEST_F(AuditSignaturePayloadTest, SignaturePayloadHashPositionIsBeforeRegistryAddress) {
    AuditSignaturePayloadTransaction transaction(
        kTestEquivalent,
        contractorID,
        contractorsManager.get(),
        trustLinesManager.get(),
        storage.get(),
        keystore.get(),
        featuresManager.get(),
        influenceController.get(),
        *logger);

    std::vector<TransactionUUID> list = {
        makeUUID(0x03, 0x03)
    };
    transaction.setAuditContext(
        kTestAuditNumber,
        TrustLineBalance(300),
        list);

    auto payload = transaction.getOwnSerializedAuditDataWithTransactionHash();

    const size_t hashOffset =
        sizeof(AuditNumber) +
        kTrustLineAmountBytesCount +
        kTrustLineAmountBytesCount +
        kTrustLineBalanceSerializeBytesCount;

    const size_t lengthOffset =
        hashOffset + AuditMessage::kTransactionUUIDsHashSize;

    EquivalentRegisterAddressLength addressLength = 0;
    memcpy(&addressLength, payload.first.get() + lengthOffset, sizeof(addressLength));

    EXPECT_EQ(addressLength, static_cast<EquivalentRegisterAddressLength>(kRegistryAddress.size()));
}
