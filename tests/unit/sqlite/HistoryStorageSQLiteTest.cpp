#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <filesystem>
#include <memory>
#include <vector>

#include "../../../src/core/io/storage/sqlite/HistoryStorageSQLite.h"
#include "../../../src/core/common/time/TimeUtils.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/contractors/Contractor.h"
#include "../../../src/core/common/memory/MemoryUtils.h"
#include "../../../src/core/contractors/addresses/GNSAddress.h"
#include "../../../src/core/logger/Logger.h"

using namespace std;
using ::testing::_;

class HistoryStorageSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Prepare temporary path for SQLite file
        testDir = std::filesystem::temp_directory_path() / "vtcpd_test_history_storage";
        std::filesystem::create_directories(testDir);

        dbPath = testDir / "history_storage_test.db";
        int rc = sqlite3_open(dbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database: " << sqlite3_errmsg(db);

        logger = std::make_unique<Logger>();
        mainTable = "history_main";
        additionalTable = "history_additional";

        storage = std::make_unique<HistoryStorageSQLite>(db, mainTable, additionalTable, *logger);
    }

    void TearDown() override {
        storage.reset();
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        std::error_code ec;
        std::filesystem::remove_all(testDir, ec);
    }

    // Helpers -----------------------------------------------------------
    Contractor::Shared createTestContractor() {
        vector<BaseAddress::Shared> addresses;
        addresses.push_back(std::make_shared<GNSAddress>("user@provider"));
        return std::make_shared<Contractor>(addresses);
    }

    TrustLineRecord::Shared createTrustLineRecord(const SerializedEquivalent equivalent) {
        TransactionUUID opUUID;
        auto contractor = createTestContractor();
        TrustLineAmount amount = 100;
        return std::make_shared<TrustLineRecord>(opUUID,
                                                 TrustLineRecord::TrustLineOperationType::Opening,
                                                 contractor,
                                                 amount);
    }

    PaymentRecord::Shared createOutgoingPaymentRecord(const SerializedEquivalent equivalent) {
        TransactionUUID opUUID;
        auto contractor = createTestContractor();
        TrustLineAmount amount = 200;
        TrustLineBalance balance = 300;
        vector<pair<ContractorID, TrustLineAmount>> outgoingTransfers;
        vector<pair<ContractorID, TrustLineAmount>> incomingTransfers;
        outgoingTransfers.emplace_back(1, amount);
        return std::make_shared<PaymentRecord>(equivalent,
                                               opUUID,
                                               PaymentRecord::OutgoingPaymentType,
                                               contractor,
                                               amount,
                                               balance,
                                               outgoingTransfers,
                                               incomingTransfers);
    }

    PaymentRecord::Shared createIncomingPaymentRecord(const SerializedEquivalent equivalent) {
        TransactionUUID opUUID;
        auto contractor = createTestContractor();
        TrustLineAmount amount = 100;
        TrustLineBalance balance = 300;
        vector<pair<ContractorID, TrustLineAmount>> outgoingTransfers;
        vector<pair<ContractorID, TrustLineAmount>> incomingTransfers;
        incomingTransfers.emplace_back(1, amount);
        return std::make_shared<PaymentRecord>(equivalent,
                                               opUUID,
                                               PaymentRecord::IncomingPaymentType,
                                               contractor,
                                               amount,
                                               balance,
                                               outgoingTransfers,
                                               incomingTransfers);
    }

    PaymentAdditionalRecord::Shared createPaymentAdditionalRecord() {
        TransactionUUID opUUID;
        TrustLineAmount amount = 100;
        vector<pair<ContractorID, TrustLineAmount>> outgoingTransfers;
        vector<pair<ContractorID, TrustLineAmount>> incomingTransfers;
        return std::make_shared<PaymentAdditionalRecord>(opUUID,
                                                         PaymentAdditionalRecord::PaymentAdditionalOperationType::IntermediatePaymentType,
                                                         amount,
                                                         outgoingTransfers,
                                                         incomingTransfers);
    }

protected:
    std::filesystem::path testDir;
    std::filesystem::path dbPath;
    sqlite3 *db = nullptr;
    std::unique_ptr<Logger> logger;
    std::string mainTable;
    std::string additionalTable;
    std::unique_ptr<HistoryStorageSQLite> storage;
};

//---------------------------------------------------------------------
// Constructor / Initialization tests
//---------------------------------------------------------------------
TEST_F(HistoryStorageSQLiteTest, ConstructorCreatesTables) {
    // Verify main table exists
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + mainTable + "'";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    sqlite3_finalize(stmt);

    // Verify additional table exists
    query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + additionalTable + "'";
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    sqlite3_finalize(stmt);
}

TEST_F(HistoryStorageSQLiteTest, ConstructorCreatesIndexes) {
    // Only check one representative index for each table
    std::string idxName = mainTable + "_operation_uuid_idx";
    std::string query = "SELECT name FROM sqlite_master WHERE type='index' AND name='" + idxName + "'";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    sqlite3_finalize(stmt);
}

//---------------------------------------------------------------------
// Trust line record tests
//---------------------------------------------------------------------
TEST_F(HistoryStorageSQLiteTest, SaveTrustLineRecordAndRetrieve) {
    SerializedEquivalent eq = 10;
    auto record = createTrustLineRecord(eq);

    EXPECT_NO_THROW(storage->saveTrustLineRecord(record, eq));

    auto results = storage->allTrustLineRecords(eq,
                                                /*recordsCount*/10,
                                                /*fromRecord*/0,
                                                DateTime(), false,
                                                DateTime(), false);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->operationUUID(), record->operationUUID());
}

TEST_F(HistoryStorageSQLiteTest, SaveTrustLineRecordNullPointerThrows) {
    EXPECT_THROW(storage->saveTrustLineRecord(nullptr, 1), ValueError);
}

//---------------------------------------------------------------------
// Payment main table record tests
//---------------------------------------------------------------------
TEST_F(HistoryStorageSQLiteTest, SaveOutgoingPaymentRecordAndRetrieve) {
    SerializedEquivalent eq = 20;
    auto payment = createOutgoingPaymentRecord(eq);

    EXPECT_NO_THROW(storage->savePaymentRecord(payment));

    auto results = storage->allPaymentRecords(eq,
                                              /*recordsCount*/10,
                                              /*fromRecord*/0,
                                              DateTime(), false,
                                              DateTime(), false,
                                              0, false,
                                              0, false);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->operationUUID(), payment->operationUUID());
}

TEST_F(HistoryStorageSQLiteTest, SaveIncomingPaymentRecordAndRetrieveAllEquivalents) {
    SerializedEquivalent eq = 30;
    auto payment = createIncomingPaymentRecord(eq);
    storage->savePaymentRecord(payment);

    auto results = storage->paymentRecordsAllEquivalents(/*recordsCount*/10,
                                                         /*fromRecord*/0,
                                                         DateTime(), false,
                                                         DateTime(), false,
                                                         0, false,
                                                         0, false);
    EXPECT_FALSE(results.empty());
}

TEST_F(HistoryStorageSQLiteTest, SavePaymentRecordNullPointerThrows) {
    EXPECT_THROW(storage->savePaymentRecord(nullptr), ValueError);
}

//---------------------------------------------------------------------
// Payment additional record tests
//---------------------------------------------------------------------
TEST_F(HistoryStorageSQLiteTest, SavePaymentAdditionalRecordAndRetrieve) {
    SerializedEquivalent eq = 40;
    auto additional = createPaymentAdditionalRecord();
    EXPECT_NO_THROW(storage->savePaymentAdditionalRecord(additional, eq));

    auto results = storage->allPaymentAdditionalRecords(eq,
                                                        /*recordsCount*/10,
                                                        /*fromRecord*/0,
                                                        DateTime(), false,
                                                        DateTime(), false,
                                                        0, false,
                                                        0, false);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->operationUUID(), additional->operationUUID());
}

TEST_F(HistoryStorageSQLiteTest, SavePaymentAdditionalRecordNullPointerThrows) {
    EXPECT_THROW(storage->savePaymentAdditionalRecord(nullptr, 1), ValueError);
}

//---------------------------------------------------------------------
// Operation existence check
//---------------------------------------------------------------------
TEST_F(HistoryStorageSQLiteTest, WhetherOperationWasConductedReturnsTrue) {
    auto record = createTrustLineRecord(55);
    storage->saveTrustLineRecord(record, 55);

    EXPECT_TRUE(storage->whetherOperationWasConducted(record->operationUUID()));
}

TEST_F(HistoryStorageSQLiteTest, WhetherOperationWasConductedReturnsFalse) {
    TransactionUUID randomUUID;
    EXPECT_FALSE(storage->whetherOperationWasConducted(randomUUID));
}

//---------------------------------------------------------------------
// Records with contractor filter
//---------------------------------------------------------------------
TEST_F(HistoryStorageSQLiteTest, RecordsWithContractorFilter) {
    SerializedEquivalent eq = 70;
    auto payment = createOutgoingPaymentRecord(eq);
    storage->savePaymentRecord(payment);

    auto contractorAddresses = payment->contractor()->addresses();
    auto results = storage->recordsWithContractor(contractorAddresses, eq, 10, 0);
    ASSERT_FALSE(results.empty());
} 