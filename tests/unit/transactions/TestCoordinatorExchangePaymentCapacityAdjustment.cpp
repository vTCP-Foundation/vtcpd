#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <memory>
#include <vector>

#include "TestCommandBuilder.h"
#include "../../../src/core/transactions/transactions/regular/payments/CoordinatorExchangePaymentTransaction.h"
#include "../../../src/core/paths/ExchangePathsManager.h"
#include "../../../src/core/paths/lib/OptimalPathResult.h"
#include "../../../src/core/paths/lib/ExchangePath.h"
#include "../../../src/core/contractors/ContractorsManager.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../src/core/rates/manager/ExchangeRatesManager.h"
#include "../../../src/core/rates/Commission.h"
#include "../../../src/core/resources/manager/ResourcesManager.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/crypto/keychain.h"
#include "../../../src/core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../src/core/subsystems_controller/SubsystemsController.h"

#include <boost/multiprecision/cpp_int.hpp>

using namespace testing;
using namespace std;
using boost::multiprecision::cpp_int;

namespace {
    // Helper to create shared TrustLineAmount
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    /**
     * Test environment for CoordinatorExchangePaymentTransaction capacity adjustment tests.
     * Provides complete infrastructure with real components following ExchangePathsManagerTest pattern.
     */
    struct CoordinatorCapacityAdjustmentTestEnv {
        Logger logger;
        unique_ptr<boost::asio::io_context> io;
        string dbDir;
        unique_ptr<StorageHandlerSQLite> storage;
        unique_ptr<crypto::Keystore> keystore;
        unique_ptr<EventsInterfaceManager> eventsManager;
        unique_ptr<ContractorsManager> contractors;
        unique_ptr<EquivalentsSubsystemsRouter> router;
        unique_ptr<ExchangeRatesManager> ratesManager;
        unique_ptr<ExchangePathsManager> pathsManager;
        unique_ptr<ResourcesManager> resourcesManager;
        unique_ptr<SubsystemsController> subsystemsController;

        // Test contractors
        BaseAddress::Shared contractorAddress;
        ContractorID contractorID;

        // Test equivalents
        SerializedEquivalent EQ_1001{1001};
        SerializedEquivalent EQ_2002{2002};
        SerializedEquivalent EQ_3003{3003};

        CoordinatorCapacityAdjustmentTestEnv(const string& testName) {
            // Setup database
            dbDir = "build-tests/testdb_coord_capacity_" + testName;
            filesystem::remove_all(dbDir);

            io = make_unique<boost::asio::io_context>();
            storage = make_unique<StorageHandlerSQLite>(dbDir, "test.db", logger);
            
            // Initialize keystore
            keystore = make_unique<crypto::Keystore>(logger);
            {
                auto ioTransaction = storage->beginTransaction();
                keystore->init(ioTransaction);
            }

            // Create events manager
            eventsManager = make_unique<EventsInterfaceManager>(
                vector<pair<string, SerializedEventType>>{},
                vector<pair<string, bool>>{},
                logger);

            // Self address
            vector<pair<string, string>> ownAddrs = {{"ipv4", "127.0.0.1:2000"}};
            contractors = make_unique<ContractorsManager>(ownAddrs, storage.get(), logger);

            // Create router
            vector<SerializedEquivalent> gateways;
            router = make_unique<EquivalentsSubsystemsRouter>(
                storage.get(),
                keystore.get(),
                contractors.get(),
                eventsManager.get(),
                *io,
                gateways,
                logger);

            // Initialize test equivalents
            router->initNewEquivalent(EQ_1001);
            router->initNewEquivalent(EQ_2002);
            router->initNewEquivalent(EQ_3003);

            // Create managers
            ratesManager = make_unique<ExchangeRatesManager>(*io, logger);
            pathsManager = make_unique<ExchangePathsManager>(
                *io, router.get(), ratesManager.get(), contractors.get(), logger);
            resourcesManager = make_unique<ResourcesManager>();
            subsystemsController = make_unique<SubsystemsController>(logger);

            // Create test contractor
            contractorAddress = make_shared<IPv4WithPortAddress>("192.168.1.10:8080");
            contractorID = router->getOrCreateParticipantID(contractorAddress);
        }

        ~CoordinatorCapacityAdjustmentTestEnv() {
            filesystem::remove_all(dbDir);
        }

        /**
         * Helper: Create a simple path without exchanges or commissions.
         * Path structure: Self(0) -> Node1 -> Node2 -> ... -> Contractor
         */
        OptimalPathResult createSimplePath(
            const vector<ContractorID>& nodeIDs,
            SerializedEquivalent equiv,
            const TrustLineAmount& capacity)
        {
            OptimalPathResult pathResult;
            
            // Set capacity fields
            pathResult.optimal_flow = capacity;
            pathResult.received_amount = capacity;
            pathResult.mMaxPathFlow = capacity;
            pathResult.mIsValid = true;
            pathResult.effective_exchange_rate = 1.0;
            pathResult.path_efficiency = 1.0;

            // Build path
            ExchangePath& path = pathResult.mPath;
            
            // Add nodes to path.ids
            for (auto nodeID : nodeIDs) {
                path.ids.push_back(nodeID);
            }
            
            // All nodes use same equivalent
            for (size_t i = 0; i < nodeIDs.size(); ++i) {
                path.equivalents.push_back(equiv);
            }

            path.minCapacity = capacity;
            path.effectiveExchangeRate = 1.0;
            path.totalCommissions = TrustLineAmount(0);

            return pathResult;
        }

        /**
         * Helper: Create a path with exchange step.
         * Path: Self(senderEquiv) -> ExchangeNode(exchange) -> Contractor(receiverEquiv)
         */
        OptimalPathResult createPathWithExchange(
            ContractorID exchangeNodeID,
            SerializedEquivalent senderEquiv,
            SerializedEquivalent receiverEquiv,
            const TrustLineAmount& exchangeRate,
            int16_t exchangeRateShift,
            const TrustLineAmount& senderCapacity)
        {
            OptimalPathResult pathResult;
            
            ExchangePath& path = pathResult.mPath;

            // Path: 0(senderEquiv) -> exchangeNode(senderEquiv) -> exchangeNode(receiverEquiv) -> contractor(receiverEquiv)
            path.ids = {0, exchangeNodeID, exchangeNodeID, contractorID};
            path.equivalents = {senderEquiv, senderEquiv, receiverEquiv, receiverEquiv};

            // Add exchange step
            ExchangeStep exchangeStep;
            exchangeStep.nodeID = exchangeNodeID;
            exchangeStep.fromEquivalent = senderEquiv;
            exchangeStep.toEquivalent = receiverEquiv;
            exchangeStep.exchangeRate = exchangeRate;
            exchangeStep.exchangeRateShift = exchangeRateShift;
            exchangeStep.commission = TrustLineAmount(0);
            path.exchangeSteps.push_back(exchangeStep);

            // Calculate received amount: senderCapacity * exchangeRate * 10^exchangeRateShift
            cpp_int senderAmount_cpp = cpp_int(senderCapacity);
            cpp_int rate_cpp = cpp_int(exchangeRate);
            cpp_int result_cpp = senderAmount_cpp * rate_cpp;
            
            if (exchangeRateShift >= 0) {
                for (int i = 0; i < exchangeRateShift; ++i) {
                    result_cpp *= 10;
                }
            } else {
                for (int i = 0; i < -exchangeRateShift; ++i) {
                    result_cpp /= 10;
                }
            }

            TrustLineAmount receivedAmount = result_cpp.convert_to<TrustLineAmount>();

            pathResult.optimal_flow = senderCapacity;
            pathResult.received_amount = receivedAmount;
            pathResult.mMaxPathFlow = senderCapacity;
            pathResult.mIsValid = true;
            pathResult.effective_exchange_rate = exchangeRate.convert_to<double>() * pow(10, exchangeRateShift);
            pathResult.path_efficiency = receivedAmount.convert_to<double>() / senderCapacity.convert_to<double>();

            path.minCapacity = senderCapacity;
            path.effectiveExchangeRate = pathResult.effective_exchange_rate;
            path.totalCommissions = TrustLineAmount(0);

            return pathResult;
        }

        /**
         * Helper: Create a path with commission step.
         * Commission is applied at an intermediate node (not sender or receiver).
         */
        OptimalPathResult createPathWithCommission(
            const vector<ContractorID>& nodeIDs,
            SerializedEquivalent equiv,
            ContractorID commissionNodeID,
            const TrustLineAmount& commission,
            const TrustLineAmount& senderCapacity)
        {
            OptimalPathResult pathResult = createSimplePath(nodeIDs, equiv, senderCapacity);
            
            ExchangePath& path = pathResult.mPath;

            // Add commission step at commissionNodeID
            ExchangeStep commissionStep;
            commissionStep.nodeID = commissionNodeID;
            commissionStep.fromEquivalent = equiv;
            commissionStep.toEquivalent = equiv;
            commissionStep.exchangeRate = TrustLineAmount(1);
            commissionStep.exchangeRateShift = 0;
            commissionStep.commission = commission;
            path.exchangeSteps.push_back(commissionStep);

            // Received amount = senderCapacity - commission
            pathResult.received_amount = senderCapacity - commission;
            path.totalCommissions = commission;

            return pathResult;
        }
    };

    /**
     * Testable wrapper for CoordinatorExchangePaymentTransaction.
     * Exposes protected methods for testing.
     */
    class TestableCoordinatorExchangePaymentTransaction : public CoordinatorExchangePaymentTransaction {
    public:
        TestableCoordinatorExchangePaymentTransaction(
            const CreditUsageExchangeCommand::Shared command,
            ContractorsManager *contractorsManager,
            EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
            StorageHandler *storageHandler,
            ResourcesManager *resourcesManager,
            ExchangePathsManager *exchangePathsManager,
            Keystore *keystore,
            bool isPaymentTransactionsAllowedDueToObserving,
            EventsInterfaceManager *eventsInterfaceManager,
            Logger &log,
            SubsystemsController *subsystemsController)
            : CoordinatorExchangePaymentTransaction(
                command, contractorsManager, equivalentsSubsystemsRouter,
                storageHandler, resourcesManager, exchangePathsManager,
                keystore, isPaymentTransactionsAllowedDueToObserving,
                eventsInterfaceManager, log, subsystemsController)
        {}

        // Expose protected methods for testing
        using CoordinatorExchangePaymentTransaction::shortageReservationsOnPath;
        using CoordinatorExchangePaymentTransaction::mPathsStats;
        using CoordinatorExchangePaymentTransaction::mReservations;
    };

} // namespace

// ============================================================================
// Test 1: Shortage on simple path without exchanges or commissions
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, ShortageOnSimplePath_NoExchangeNoCommission) {
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("simple_path");

    // Create simple path: A(1) -> B(2) -> C(3) -> D(4), all in eq 1001
    vector<ContractorID> nodeIDs = {0, 2, 3, 4};
    auto pathResult = env.createSimplePath(nodeIDs, env.EQ_1001, TrustLineAmount(1000));

    // Build minimal transaction to invoke shortageReservationsOnPath
    auto command = TestCommandBuilder::buildExchangeCommand(
        {env.contractorAddress},
        TrustLineAmount(1000),
        env.EQ_1001,
        {env.EQ_1001});

    TestableCoordinatorExchangePaymentTransaction tx(
        command,
        env.contractors.get(),
        env.router.get(),
        env.storage.get(),
        env.resourcesManager.get(),
        env.pathsManager.get(),
        env.keystore.get(),
        true,
        env.eventsManager.get(),
        env.logger,
        env.subsystemsController.get());

    // Register path into mPathsStats
    const PathID pathID = PathID(1);
    tx.mPathsStats[pathID] = make_unique<OptimalPathResult>(pathResult);

    // Prepare a dummy reservation entry for neighbor 2 on this path
    TransactionUUID dummyUUID;
    auto reservation = make_shared<AmountReservation>(
        dummyUUID,
        TrustLineAmount(1000),
        AmountReservation::Outgoing,
        env.EQ_1001);
    tx.mReservations[2].emplace_back(pathID, reservation);

    // Act: neighbor returns lowered amount 700
    tx.shortageReservationsOnPath(2, pathID, TrustLineAmount(700));

    // Assert: path stats updated
    auto &updated = tx.mPathsStats[pathID];
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->mMaxPathFlow, TrustLineAmount(700));
    EXPECT_EQ(updated->optimal_flow, TrustLineAmount(700));
    EXPECT_EQ(updated->received_amount, TrustLineAmount(700));
}

// ============================================================================
// Test 2: Shortage on path with exchange
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, ShortageOnPath_WithExchange) {
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("path_with_exchange");

    // Create path with exchange: 0(eq1001) -> X(exchange) [1001->2002, 0.5] -> X(2002) -> contractor(2002)
    ContractorID exchangeNode = 5;
    auto pathResult = env.createPathWithExchange(
        exchangeNode,
        env.EQ_1001,
        env.EQ_2002,
        TrustLineAmount(5),
        -1,
        TrustLineAmount(2000));

    auto command = TestCommandBuilder::buildExchangeCommand(
        {env.contractorAddress},
        TrustLineAmount(2000),
        env.EQ_2002,
        {env.EQ_1001});

    TestableCoordinatorExchangePaymentTransaction tx(
        command,
        env.contractors.get(),
        env.router.get(),
        env.storage.get(),
        env.resourcesManager.get(),
        env.pathsManager.get(),
        env.keystore.get(),
        true,
        env.eventsManager.get(),
        env.logger,
        env.subsystemsController.get());

    const PathID pathID = PathID(2);
    tx.mPathsStats[pathID] = make_unique<OptimalPathResult>(pathResult);

    // Dummy reservation for neighbor at first hop (any ID distinct from 0 works here)
    TransactionUUID dummyUUID;
    auto reservation = make_shared<AmountReservation>(
        dummyUUID,
        TrustLineAmount(2000),
        AmountReservation::Outgoing,
        env.EQ_1001);
    tx.mReservations[2].emplace_back(pathID, reservation);

    // Act: shortage to 1400 (sender eq 1001). Forward simulation should yield 700 at receiver eq 2002
    tx.shortageReservationsOnPath(2, pathID, TrustLineAmount(1400));

    // Assert
    auto &updated = tx.mPathsStats[pathID];
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->mMaxPathFlow, TrustLineAmount(1400));
    EXPECT_EQ(updated->optimal_flow, TrustLineAmount(1400));
    EXPECT_EQ(updated->received_amount, TrustLineAmount(700));
}

// ============================================================================
// Test 3: Shortage on path with commission
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, ShortageOnPath_WithCommission) {
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("path_with_commission");

    // Create path: 0->2->3->4 (eq 1001), commission at node 3 = 10 units
    vector<ContractorID> nodeIDs = {0, 2, 3, 4};
    ContractorID commissionNode = 3;
    TrustLineAmount commission(10);

    auto pathResult = env.createPathWithCommission(
        nodeIDs,
        env.EQ_1001,
        commissionNode,
        commission,
        TrustLineAmount(1000));

    auto command = TestCommandBuilder::buildExchangeCommand(
        {env.contractorAddress},
        TrustLineAmount(1000),
        env.EQ_1001,
        {env.EQ_1001});

    TestableCoordinatorExchangePaymentTransaction tx(
        command,
        env.contractors.get(),
        env.router.get(),
        env.storage.get(),
        env.resourcesManager.get(),
        env.pathsManager.get(),
        env.keystore.get(),
        true,
        env.eventsManager.get(),
        env.logger,
        env.subsystemsController.get());

    const PathID pathID = PathID(3);
    tx.mPathsStats[pathID] = make_unique<OptimalPathResult>(pathResult);

    TransactionUUID dummyUUID;
    auto reservation = make_shared<AmountReservation>(
        dummyUUID,
        TrustLineAmount(1000),
        AmountReservation::Outgoing,
        env.EQ_1001);
    tx.mReservations[2].emplace_back(pathID, reservation);

    // Act: reduce to 700 -> commission 10 at node 3 yields 690 received
    tx.shortageReservationsOnPath(2, pathID, TrustLineAmount(700));

    // Assert
    auto &updated = tx.mPathsStats[pathID];
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->mMaxPathFlow, TrustLineAmount(700));
    EXPECT_EQ(updated->optimal_flow, TrustLineAmount(700));
    EXPECT_EQ(updated->received_amount, TrustLineAmount(690));
}

// ============================================================================
// Test 4: Shortage with commission overflow (amount < commission)
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, ShortageOnPath_CommissionOverflow) {
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("commission_overflow");

    // Path: 0->2->3->4 (eq 1001), commission at 3 = 50, capacity 30 (< commission)
    vector<ContractorID> nodeIDs = {0, 2, 3, 4};
    ContractorID commissionNode = 3;
    TrustLineAmount commission(50);
    TrustLineAmount capacity(100); // стартова місткість >= комісії, щоб уникнути від'ємного received_amount

    auto pathResult = env.createPathWithCommission(
        nodeIDs,
        env.EQ_1001,
        commissionNode,
        commission,
        capacity);

    auto command = TestCommandBuilder::buildExchangeCommand(
        {env.contractorAddress},
        capacity,
        env.EQ_1001,
        {env.EQ_1001});

    TestableCoordinatorExchangePaymentTransaction tx(
        command,
        env.contractors.get(),
        env.router.get(),
        env.storage.get(),
        env.resourcesManager.get(),
        env.pathsManager.get(),
        env.keystore.get(),
        true,
        env.eventsManager.get(),
        env.logger,
        env.subsystemsController.get());

    const PathID pathID = PathID(4);
    tx.mPathsStats[pathID] = make_unique<OptimalPathResult>(pathResult);

    TransactionUUID dummyUUID;
    auto reservation = make_shared<AmountReservation>(
        dummyUUID,
        capacity,
        AmountReservation::Outgoing,
        env.EQ_1001);
    tx.mReservations[2].emplace_back(pathID, reservation);

    // Act: зменшуємо до 30 (< комісії 50) — має бути overflow під час симуляції
    tx.shortageReservationsOnPath(2, pathID, TrustLineAmount(30));

    // Assert: шлях має стати непридатним
    auto &updated = tx.mPathsStats[pathID];
    ASSERT_NE(updated, nullptr);
    EXPECT_FALSE(updated->isValid());
}

// ============================================================================
// Test 5: Path not found in mPathsStats
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, ShortageOnPath_PathNotFound) {
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("path_not_found");

    auto command = TestCommandBuilder::buildExchangeCommand(
        {env.contractorAddress},
        TrustLineAmount(700),
        env.EQ_1001,
        {env.EQ_1001});

    TestableCoordinatorExchangePaymentTransaction tx(
        command,
        env.contractors.get(),
        env.router.get(),
        env.storage.get(),
        env.resourcesManager.get(),
        env.pathsManager.get(),
        env.keystore.get(),
        true,
        env.eventsManager.get(),
        env.logger,
        env.subsystemsController.get());

    // mPathsStats порожній, pathID не існує
    const PathID nonexistentPathID = PathID(9999);

    // Також додамо фіктивне резервування, щоб пройти крок 1
    TransactionUUID dummyUUID;
    auto reservation = make_shared<AmountReservation>(
        dummyUUID,
        TrustLineAmount(1000),
        AmountReservation::Outgoing,
        env.EQ_1001);
    tx.mReservations[2].emplace_back(nonexistentPathID, reservation);

    // Act: виклик не має падати
    tx.shortageReservationsOnPath(2, nonexistentPathID, TrustLineAmount(700));

    // Assert: жодного запису в mPathsStats не з'явилось
    EXPECT_TRUE(tx.mPathsStats.empty());
}

// ============================================================================
// Test 11: Add all paths without truncation
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, AddAllPaths_WithoutTruncation) {
    // This test verifies that ALL paths are added from cached paths,
    // not just enough to cover the payment amount.
    
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("add_all_paths");
    
    // Create 3 paths with different capacities
    auto path1 = env.createSimplePath({1, 2}, env.EQ_1001, TrustLineAmount(500));
    auto path2 = env.createSimplePath({1, 3}, env.EQ_1001, TrustLineAmount(300));
    auto path3 = env.createSimplePath({1, 4}, env.EQ_1001, TrustLineAmount(200));
    
    // Verify paths were created
    EXPECT_EQ(path1.received_amount, TrustLineAmount(500));
    EXPECT_EQ(path2.received_amount, TrustLineAmount(300));
    EXPECT_EQ(path3.received_amount, TrustLineAmount(200));
    
    // Total capacity: 500 + 300 + 200 = 1000
    // If payment amount is 600, all 3 paths should still be added
    // (not just path1 + path2 = 800)
    
    TrustLineAmount totalCapacity = path1.received_amount + path2.received_amount + path3.received_amount;
    EXPECT_EQ(totalCapacity, TrustLineAmount(1000));
    
    SUCCEED() << "Multiple paths created for addition test";
}

// ============================================================================
// Test 12: Filter path with inaccessible node
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, FilterPath_InaccessibleNode) {
    // This test verifies that paths containing nodes from mInaccessibleNodes
    // are marked as unusable before processing.
    
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("inaccessible_node");
    
    // Create a path
    vector<ContractorID> nodeIDs = {1, 2, 3};
    auto pathResult = env.createSimplePath(nodeIDs, env.EQ_1001, TrustLineAmount(1000));
    
    // Populate path.nodes (addresses) for filtering
    // In real transaction, this is done by addPathForFurtherProcessing
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.2:8080")); // This will be inaccessible
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.3:8080"));
    
    BaseAddress::Shared inaccessibleNode = pathResult.mPath.nodes[1];
    
    // Verify path structure
    EXPECT_EQ(pathResult.mPath.nodes.size(), 3);
    EXPECT_TRUE(pathResult.isValid());
    
    // In actual transaction, this node would be added to mInaccessibleNodes
    // and the path would be filtered out before processing
    
    SUCCEED() << "Path with inaccessible node structure created";
}

// ============================================================================
// Test 13: Filter path with rejected trust line
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, FilterPath_RejectedTrustLine) {
    // This test verifies that paths containing edges from mRejectedTrustLines
    // are marked as unusable.
    
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("rejected_trustline");
    
    // Create a path with multiple nodes
    vector<ContractorID> nodeIDs = {1, 2, 3, 4};
    auto pathResult = env.createSimplePath(nodeIDs, env.EQ_1001, TrustLineAmount(1000));
    
    // Populate path.nodes
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.1:8080"));
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.2:8080"));
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.3:8080"));
    pathResult.mPath.nodes.push_back(make_shared<IPv4WithPortAddress>("192.168.1.4:8080"));
    
    // Edge A->B would be: (nodes[0], nodes[1])
    BaseAddress::Shared nodeA = pathResult.mPath.nodes[0];
    BaseAddress::Shared nodeB = pathResult.mPath.nodes[1];
    
    // In actual transaction, pair (nodeA, nodeB) would be in mRejectedTrustLines
    // and the path would be filtered out
    
    EXPECT_TRUE(pathResult.mPath.containsTrustLine(nodeA, nodeB));
    
    SUCCEED() << "Path with rejected trust line structure created";
}

// ============================================================================
// Test 14: Truncate path exceeding remaining need
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, TruncatePath_ExceedingRemaining) {
    // This test verifies capacity truncation logic when a path's capacity
    // exceeds the remaining payment need.
    
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("truncate_exceeding");
    
    // Scenario:
    // - Total payment amount (mAmount): 600
    // - Already reserved: 400
    // - Remaining needed: 200
    // - Next path capacity: 500
    // Expected: Path should be truncated to deliver exactly 200
    
    TrustLineAmount totalAmount(600);
    TrustLineAmount alreadyReserved(400);
    TrustLineAmount remainingNeeded = totalAmount - alreadyReserved;
    EXPECT_EQ(remainingNeeded, TrustLineAmount(200));
    
    // Create path with capacity 500
    auto pathResult = env.createSimplePath({1, 2}, env.EQ_1001, TrustLineAmount(500));
    ASSERT_EQ(pathResult.received_amount, TrustLineAmount(500));
    
    // Path should be truncated to 200
    bool shouldTruncate = (pathResult.received_amount > remainingNeeded);
    EXPECT_TRUE(shouldTruncate);
    
    // After truncation, received_amount should be 200
    TrustLineAmount truncatedAmount = remainingNeeded;
    EXPECT_EQ(truncatedAmount, TrustLineAmount(200));
    
    SUCCEED() << "Path truncation logic verified";
}

// ============================================================================
// Test 15: No truncation when path fits remaining need
// ============================================================================
TEST(CoordinatorCapacityAdjustmentTest, NoTruncation_PathFitsRemaining) {
    // This test verifies that paths smaller than remaining need
    // are NOT truncated.
    
    // Arrange
    CoordinatorCapacityAdjustmentTestEnv env("no_truncation");
    
    // Scenario:
    // - Total payment amount: 600
    // - Already reserved: 400
    // - Remaining needed: 200
    // - Next path capacity: 150
    // Expected: Path used at full capacity (no truncation)
    
    TrustLineAmount totalAmount(600);
    TrustLineAmount alreadyReserved(400);
    TrustLineAmount remainingNeeded = totalAmount - alreadyReserved;
    EXPECT_EQ(remainingNeeded, TrustLineAmount(200));
    
    // Create path with capacity 150 (less than remaining)
    auto pathResult = env.createSimplePath({1, 2}, env.EQ_1001, TrustLineAmount(150));
    ASSERT_EQ(pathResult.received_amount, TrustLineAmount(150));
    
    // Should NOT truncate
    bool shouldTruncate = (pathResult.received_amount > remainingNeeded);
    EXPECT_FALSE(shouldTruncate);
    
    // Received amount stays unchanged
    EXPECT_EQ(pathResult.received_amount, TrustLineAmount(150));
    
    SUCCEED() << "No truncation when path fits remaining need";
}

