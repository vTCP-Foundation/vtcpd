#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>

#include "core/trust_lines/manager/TrustLinesManager.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/contractors/ContractorsManager.h"
#include "core/crypto/keychain.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"

class TrustLinesManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        tempDir = std::filesystem::temp_directory_path()
            / ("vtcpd_trust_lines_manager_test_" + suffix);
        std::filesystem::create_directories(tempDir);

        logger = std::make_unique<Logger>();
        storage = std::make_unique<StorageHandlerSQLite>(
            tempDir.string(),
            "trust_lines_manager_test.db",
            *logger);

        std::vector<std::pair<std::string, std::string>> ownAddresses = {
            {"ipv4", "127.0.0.1:2000"}
        };
        contractorsManager = std::make_unique<ContractorsManager>(
            ownAddresses,
            storage.get(),
            *logger);

        keystore = std::make_unique<crypto::Keystore>(*logger);

        trustLinesManager = std::make_unique<TrustLinesManager>(
            static_cast<SerializedEquivalent>(1),
            storage.get(),
            keystore.get(),
            contractorsManager.get(),
            *logger);

        auto ioTransaction = storage->beginTransaction();
        std::vector<BaseAddress::Shared> contractorAddresses;
        contractorAddresses.push_back(std::make_shared<IPv4WithPortAddress>(
            std::string("127.0.0.1"), static_cast<uint16_t>(3000)));
        auto contractor = contractorsManager->createContractor(
            ioTransaction,
            contractorAddresses);
        contractorID = contractor->getID();

        trustLinesManager->open(contractorID, ioTransaction);
        ioTransaction->rollback();
    }

    void TearDown() override {
        trustLinesManager.reset();
        keystore.reset();
        contractorsManager.reset();
        storage.reset();
        logger.reset();
        std::filesystem::remove_all(tempDir);
    }

    TrustLine::Shared trustLine() const {
        return trustLinesManager->trustLines()[contractorID];
    }

    std::filesystem::path tempDir;
    ContractorID contractorID = 0;
    std::unique_ptr<Logger> logger;
    std::unique_ptr<StorageHandlerSQLite> storage;
    std::unique_ptr<ContractorsManager> contractorsManager;
    std::unique_ptr<crypto::Keystore> keystore;
    std::unique_ptr<TrustLinesManager> trustLinesManager;
};

TEST_F(TrustLinesManagerTest, UpdateTrustLineTotalReceiptsAmountsCalculatesNewBalance) {
    trustLine()->setBalance(TrustLineBalance(2000));

    trustLinesManager->updateTrustLineTotalReceiptsAmounts(
        contractorID,
        TrustLineAmount(1000),
        TrustLineAmount(500));

    EXPECT_EQ(trustLinesManager->balance(contractorID), TrustLineBalance(1500));
}

TEST_F(TrustLinesManagerTest, UpdateTrustLineTotalReceiptsAmountsPreservesExcludedIncomingAmount) {
    trustLinesManager->setOutgoing(contractorID, TrustLineAmount(100));
    EXPECT_FALSE(trustLine()->isTrustLineOverflowed());

    trustLinesManager->updateTrustLineTotalReceiptsAmounts(
        contractorID,
        TrustLineAmount(150),
        TrustLineAmount(0));

    EXPECT_TRUE(trustLine()->isTrustLineOverflowed());
}

TEST_F(TrustLinesManagerTest, UpdateTrustLineTotalReceiptsAmountsPreservesExcludedOutgoingAmount) {
    trustLinesManager->setIncoming(contractorID, TrustLineAmount(100));
    EXPECT_FALSE(trustLine()->isTrustLineOverflowed());

    trustLinesManager->updateTrustLineTotalReceiptsAmounts(
        contractorID,
        TrustLineAmount(0),
        TrustLineAmount(150));

    EXPECT_TRUE(trustLine()->isTrustLineOverflowed());
}

TEST_F(TrustLinesManagerTest, UpdateTrustLineTotalReceiptsAmountsWithZeroExcludedAmounts) {
    trustLine()->setBalance(TrustLineBalance(500));

    trustLinesManager->updateTrustLineTotalReceiptsAmounts(
        contractorID,
        TrustLineAmount(0),
        TrustLineAmount(0));

    EXPECT_EQ(trustLinesManager->balance(contractorID), TrustLineBalance(500));
    EXPECT_FALSE(trustLine()->isTrustLineOverflowed());
}

TEST_F(TrustLinesManagerTest, UpdateTrustLineTotalReceiptsAmountsWithZeroIncludedAmounts) {
    trustLine()->setBalance(TrustLineBalance(0));

    trustLinesManager->updateTrustLineTotalReceiptsAmounts(
        contractorID,
        TrustLineAmount(300),
        TrustLineAmount(100));

    EXPECT_EQ(trustLinesManager->balance(contractorID), TrustLineBalance(-200));
}
