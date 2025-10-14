#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <limits>

#include "core/paths/lib/ExchangePath.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/contractors/ContractorsManager.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/logger/Logger.h"
#include "core/common/exceptions/NotFoundError.h"

using namespace testing;

// Helper to create IPv4WithPortAddress
inline BaseAddress::Shared createAddress(const string& addr) {
    return make_shared<IPv4WithPortAddress>(addr);
}

// Helper for tests that need ContractorsManager
struct TestEnvironment {
    Logger logger;
    std::string dbDir;
    std::unique_ptr<StorageHandlerSQLite> storage;
    std::unique_ptr<ContractorsManager> contractors;

    TestEnvironment(const std::string& testName) {
        dbDir = "build-tests/testdb_exchange_path_" + testName;
        std::filesystem::remove_all(dbDir);

        storage = std::make_unique<StorageHandlerSQLite>(dbDir, "test.db", logger);

        vector<pair<string, string>> ownAddrs = {{"ipv4", "127.0.0.1:2000"}};
        contractors = std::make_unique<ContractorsManager>(ownAddrs, storage.get(), logger);
    }

    ~TestEnvironment() {
        std::filesystem::remove_all(dbDir);
    }
};

// Test Category 3: ExchangePath Enhancement (5+ tests)

TEST(ExchangePath, FieldRenamed) {
    ExchangePath path;

    // ids field should exist (renamed from nodes)
    path.ids.push_back(ContractorID(1));
    path.ids.push_back(ContractorID(2));

    EXPECT_EQ(path.ids.size(), 2);
    EXPECT_EQ(path.ids[0], ContractorID(1));
}

TEST(ExchangePath, NodesFieldAdded) {
    ExchangePath path;

    // nodes (BaseAddress) field should exist
    auto address1 = createAddress("127.0.0.1:2000");
    auto address2 = createAddress("127.0.0.1:2001");

    path.nodes.push_back(address1);
    path.nodes.push_back(address2);

    EXPECT_EQ(path.nodes.size(), 2);
}

TEST(ExchangePath, MethodsFromPath) {
    ExchangePath path;
    path.ids = {1, 2, 3};
    path.equivalents = {1, 1, 1};
    path.minCapacity = TrustLineAmount(100);
    path.effectiveExchangeRate = 1.0;

    // Test methods from Path class exist and work
    bool valid = path.isValid();
    TrustLineAmount capacity = path.calculateMaxCapacity();
    double rate = path.calculateEffectiveExchangeRate();

    EXPECT_TRUE(valid);
    EXPECT_EQ(capacity, TrustLineAmount(100));
    EXPECT_EQ(rate, 1.0);
}

TEST(ExchangePath, IdsAndNodesIndependent) {
    ExchangePath path;

    path.ids.push_back(ContractorID(1));
    path.ids.push_back(ContractorID(2));

    auto address = createAddress("127.0.0.1:2000");
    path.nodes.push_back(address);

    // ids and nodes should be independent
    EXPECT_EQ(path.ids.size(), 2);
    EXPECT_EQ(path.nodes.size(), 1);
}

TEST(ExchangePath, IntermediatesMethod) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    path.nodes = {addr1, addr2, addr3};

    auto intermediates = path.intermediates();

    EXPECT_EQ(intermediates.size(), 3);
    EXPECT_EQ(intermediates[0], addr1);
    EXPECT_EQ(intermediates[1], addr2);
    EXPECT_EQ(intermediates[2], addr3);
}

TEST(ExchangePath, PositionOfNodeFound) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    path.nodes = {addr1, addr2, addr3};

    int pos = path.positionOfNode(addr2);

    EXPECT_EQ(pos, 1);
}

TEST(ExchangePath, PositionOfNodeNotFound) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    path.nodes = {addr1, addr2};

    int pos = path.positionOfNode(addr3);

    EXPECT_EQ(pos, -1);
}

TEST(ExchangePath, AddReceiver) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");

    path.nodes = {addr1};
    path.addReceiver(addr2);

    EXPECT_EQ(path.nodes.size(), 2);
    EXPECT_EQ(path.nodes[1], addr2);
}

TEST(ExchangePath, ContainsTrustLineTrue) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    path.nodes = {addr1, addr2, addr3};

    EXPECT_TRUE(path.containsTrustLine(addr1, addr2));
    EXPECT_TRUE(path.containsTrustLine(addr2, addr3));
}

TEST(ExchangePath, ContainsTrustLineFalse) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    path.nodes = {addr1, addr2};

    EXPECT_FALSE(path.containsTrustLine(addr1, addr3));
    EXPECT_FALSE(path.containsTrustLine(addr2, addr1)); // Wrong direction
}

TEST(ExchangePath, Length) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");
    auto addr3 = createAddress("127.0.0.1:2002");

    path.nodes = {addr1, addr2, addr3};

    EXPECT_EQ(path.length(), 3);
}

TEST(ExchangePath, LengthEmpty) {
    ExchangePath path;

    EXPECT_EQ(path.length(), 0);
}

TEST(ExchangePath, ToString) {
    ExchangePath path;

    auto addr1 = createAddress("127.0.0.1:2000");
    auto addr2 = createAddress("127.0.0.1:2001");

    path.nodes = {addr1, addr2};

    string str = path.toString();

    // Should contain the addresses
    EXPECT_NE(str.find("127.0.0.1"), string::npos);
}

TEST(ExchangePath, ToStringEmpty) {
    ExchangePath path;

    string str = path.toString();

    EXPECT_EQ(str, "direct exchange path");
}

TEST(ExchangePath, IsValidTrue) {
    ExchangePath path;
    path.ids = {1, 2, 3};
    path.equivalents = {1, 1, 1};
    path.nodes = {createAddress("127.0.0.1:2000"), createAddress("127.0.0.1:2001"), createAddress("127.0.0.1:2002")};

    EXPECT_TRUE(path.isValid());
}

TEST(ExchangePath, IsValidFalseEmptyIds) {
    ExchangePath path;

    EXPECT_FALSE(path.isValid());
}

TEST(ExchangePath, IsValidFalseMismatchedSizes) {
    ExchangePath path;
    path.ids = {1, 2};
    path.equivalents = {1}; // Different size

    EXPECT_FALSE(path.isValid());
}

TEST(ExchangePath, IsValidWithEmptyNodes) {
    ExchangePath path;
    path.ids = {1, 2, 3};
    path.equivalents = {1, 1, 1};
    // nodes is empty

    EXPECT_TRUE(path.isValid()); // nodes can be empty
}

TEST(ExchangePath, StartsWithEquivalentTrue) {
    ExchangePath path;
    path.equivalents = {5, 6, 7};

    EXPECT_TRUE(path.startsWithEquivalent(5));
}

TEST(ExchangePath, StartsWithEquivalentFalse) {
    ExchangePath path;
    path.equivalents = {5, 6, 7};

    EXPECT_FALSE(path.startsWithEquivalent(6));
}

TEST(ExchangePath, StartsWithEquivalentFalseEmpty) {
    ExchangePath path;

    EXPECT_FALSE(path.startsWithEquivalent(1));
}

// Test for sumFixedCommissions() method (Requirement from user feedback)
TEST(ExchangePath, SumFixedCommissions) {
    ExchangePath path;

    // Test with zero commissions
    path.totalCommissions = TrustLineAmount(0);
    EXPECT_EQ(path.sumFixedCommissions(), TrustLineAmount(0));

    // Test with non-zero commissions
    path.totalCommissions = TrustLineAmount(250);
    EXPECT_EQ(path.sumFixedCommissions(), TrustLineAmount(250));

    // Test with large commissions
    path.totalCommissions = TrustLineAmount(999999);
    EXPECT_EQ(path.sumFixedCommissions(), TrustLineAmount(999999));
}

// Test ContractorID → BaseAddress conversion via ContractorsManager (Requirements 10-11)
TEST(ExchangePath, ContractorIDToAddressConversion) {
    TestEnvironment env("contractor_conversion");

    // Create test addresses
    auto addr1 = createAddress("192.168.1.1:3000");
    auto addr2 = createAddress("192.168.1.2:3001");
    auto addr3 = createAddress("192.168.1.3:3002");

    // Create contractors in ContractorsManager
    auto ioTransaction = env.storage->beginTransaction();
    auto contractor1 = env.contractors->createContractor(ioTransaction, {addr1});
    auto contractor2 = env.contractors->createContractor(ioTransaction, {addr2});
    auto contractor3 = env.contractors->createContractor(ioTransaction, {addr3});

    auto contractor1ID = contractor1->getID();
    auto contractor2ID = contractor2->getID();
    auto contractor3ID = contractor3->getID();

    // Create path with ContractorIDs
    ExchangePath path;
    path.ids = {contractor1ID, contractor2ID, contractor3ID};
    path.equivalents = {1, 1, 1};

    // Convert ContractorIDs back to BaseAddresses using ContractorsManager
    // This demonstrates the expected usage pattern from CoordinatorExchangePaymentTransaction::addPathForFurtherProcessing
    for (const auto& contractorID : path.ids) {
        auto contractor = env.contractors->contractor(contractorID);
        ASSERT_NE(contractor, nullptr);
        path.nodes.push_back(contractor->mainAddress());
    }

    // Verify conversion worked
    EXPECT_EQ(path.nodes.size(), 3);
    EXPECT_EQ(path.nodes[0]->fullAddress(), addr1->fullAddress());
    EXPECT_EQ(path.nodes[1]->fullAddress(), addr2->fullAddress());
    EXPECT_EQ(path.nodes[2]->fullAddress(), addr3->fullAddress());
}

// Test conversion error handling when ContractorID not found (Requirement 11)
TEST(ExchangePath, ContractorIDConversionErrorHandling) {
    TestEnvironment env("contractor_conversion_error");

    ExchangePath path;

    // Use a ContractorID that doesn't exist in ContractorsManager
    // kNotFoundContractorID = std::numeric_limits<ContractorID>::max()
    ContractorID nonExistentID = std::numeric_limits<ContractorID>::max();
    path.ids = {nonExistentID};
    path.equivalents = {1};

    // Attempt to convert - should throw NotFoundError
    // contractor() throws exception when ContractorID not found (line 233-236 ContractorsManager.cpp)
    EXPECT_THROW(env.contractors->contractor(nonExistentID), NotFoundError);

    // This demonstrates the error handling pattern from CoordinatorExchangePaymentTransaction:
    // if (firstIntermediateNodeID == kNotFoundContractorID) {
    //     warning() << "First intermediate node not found in contractors";
    //     return;
    // }
    // The check for kNotFoundContractorID should happen BEFORE calling contractor()
}

// Test bidirectional conversion: BaseAddress → ContractorID → BaseAddress
TEST(ExchangePath, BidirectionalConversion) {
    TestEnvironment env("bidirectional_conversion");

    // Start with BaseAddress
    auto originalAddr = createAddress("10.0.0.1:4000");

    // Create contractor with this address
    auto ioTransaction = env.storage->beginTransaction();
    auto createdContractor = env.contractors->createContractor(ioTransaction, {originalAddr});
    auto expectedID = createdContractor->getID();

    // Convert BaseAddress → ContractorID
    auto contractorID = env.contractors->contractorIDByAddress(originalAddr);
    EXPECT_NE(contractorID, std::numeric_limits<ContractorID>::max());
    EXPECT_EQ(contractorID, expectedID);

    // Convert ContractorID → BaseAddress
    auto contractor = env.contractors->contractor(contractorID);
    ASSERT_NE(contractor, nullptr);
    auto retrievedAddr = contractor->mainAddress();

    // Verify addresses match
    EXPECT_EQ(retrievedAddr->fullAddress(), originalAddr->fullAddress());
}
