#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>

#include "core/paths/ExchangePathsManager.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/logger/Logger.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/crypto/keychain.h"
#include "core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "core/rates/Commission.h"

using namespace testing;
using namespace std;

namespace {
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    // Helper to create test environment
    struct TestEnvironment {
        Logger logger;
        boost::asio::io_context io;
        std::string dbDir;
        std::unique_ptr<StorageHandlerSQLite> storage;
        std::unique_ptr<crypto::Keystore> keystore;
        std::unique_ptr<EventsInterfaceManager> eventsManager;
        std::unique_ptr<ContractorsManager> contractors;
        std::unique_ptr<EquivalentsSubsystemsRouter> router;
        std::unique_ptr<ExchangeRatesManager> ratesManager;
        std::unique_ptr<ExchangePathsManager> pathsManager;

        TestEnvironment(const std::string& testName) {
            dbDir = "build-tests/testdb_sim_" + testName;
            std::filesystem::remove_all(dbDir);

            storage = std::make_unique<StorageHandlerSQLite>(dbDir, "test.db", logger);
            keystore = std::make_unique<crypto::Keystore>(logger);
            {
                auto ioTransaction = storage->beginTransaction();
                keystore->init(ioTransaction);
            }

            eventsManager = std::make_unique<EventsInterfaceManager>(
                vector<pair<string, SerializedEventType>>{},
                vector<pair<string, bool>>{},
                logger);

            vector<pair<string, string>> ownAddrs = {{"ipv4", "127.0.0.1:2000"}};
            contractors = std::make_unique<ContractorsManager>(ownAddrs, storage.get(), logger);

            vector<SerializedEquivalent> gateways;
            router = std::make_unique<EquivalentsSubsystemsRouter>(
                storage.get(),
                keystore.get(),
                contractors.get(),
                eventsManager.get(),
                io,
                gateways,
                logger);

            ratesManager = std::make_unique<ExchangeRatesManager>(io, logger);
            pathsManager = std::make_unique<ExchangePathsManager>(
                io, router.get(), ratesManager.get(), contractors.get(), logger);
        }

        ~TestEnvironment() {
            std::filesystem::remove_all(dbDir);
        }

        void setupTopologyWithCommissions(
            SerializedEquivalent eq,
            const vector<tuple<ContractorID, ContractorID, uint64_t>>& trustLines,
            const vector<tuple<ContractorID, SerializedEquivalent, uint64_t>>& commissions)
        {
            router->initNewEquivalent(eq);
            auto tlm = router->topologyTrustLineManager(eq);

            for (const auto& [from, to, amount] : trustLines) {
                tlm->addTrustLine(make_shared<TopologyTrustLine>(from, to, A(amount)));
            }

            for (const auto& [node, equiv, commAmount] : commissions) {
                auto commission = make_shared<Commission>(commAmount);
                tlm->storeCommission(node, equiv, commission);
            }
        }

        ContractorID registerContractor(const string& address) {
            auto addr = make_shared<IPv4WithPortAddress>(address);
            return router->getOrCreateParticipantID(addr);
        }
    };
}

// Test 1: Forward Simulation - Simple Path (no exchange)
TEST(PathSimulationTest, ForwardSimulateSimplePath)
{
    TestEnvironment env("forward_simple");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3;

    // Setup topology: A -> B (1000) -> C (800)
    env.setupTopologyWithCommissions(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 800}
        },
        {
            {idB, EQ, 10} // Commission at B = 10
        });

    // Create path: A → B → C
    ExchangePath path;
    path.nodes = {idA, idB, idC};
    path.equivalents = {EQ, EQ, EQ};
    path.minCapacity = TrustLineAmount(800);
    path.effectiveExchangeRate = 1.0;
    path.totalCommissions = TrustLineAmount(10);

    // Simulate with 500 units input
    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    double output = env.pathsManager->forwardSimulatePath(path, 500.0, appliedCommissions, edgeCapacity);

    // Expected: 500 - 10 (commission at B) = 490
    EXPECT_NEAR(output, 490.0, 0.1);
    EXPECT_EQ(appliedCommissions.size(), 1);
    EXPECT_TRUE(appliedCommissions.count({idB, EQ}) > 0);
}

// Test 2: Forward Simulation - Exchange Path
TEST(PathSimulationTest, ForwardSimulateWithExchange)
{
    TestEnvironment env("forward_exchange");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;
    ContractorID idA = 1, idB = 2, idX = 3, idC = 4;

    // Setup topology in eq1: A -> B -> X
    env.setupTopologyWithCommissions(EQ1,
        {
            {idA, idB, 1000},
            {idB, idX, 800}
        },
        {});

    // Setup topology in eq2: X -> C
    env.setupTopologyWithCommissions(EQ2,
        {
            {idX, idC, 1500}
        },
        {});

    // Add exchange rate: eq1 -> eq2 at rate 2.0
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(2), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    // Create path: A (eq1) → B (eq1) → X (eq1) → X (eq2) [exchange at X] → C (eq2)
    // Exchange happens when same node changes equivalent
    ExchangePath path;
    path.nodes = {idA, idB, idX, idX, idC};
    path.equivalents = {EQ1, EQ1, EQ1, EQ2, EQ2};
    path.minCapacity = TrustLineAmount(800);
    path.effectiveExchangeRate = 2.0;
    path.totalCommissions = TrustLineAmount(0);

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(2);
    exchange.exchangeRateShift = 0;
    exchange.minExchangeAmount = TrustLineAmount(0);
    exchange.maxExchangeAmount = TrustLineAmount(0);
    exchange.commission = TrustLineAmount(0);
    path.exchangeSteps = {exchange};

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    double output = env.pathsManager->forwardSimulatePath(path, 200.0, appliedCommissions, edgeCapacity);

    // Expected: 200 * 2.0 = 400
    EXPECT_NEAR(output, 400.0, 0.1);
}

// Test 3: Forward Simulation - Exchange Min/Max Limits
TEST(PathSimulationTest, ForwardSimulateExchangeLimits)
{
    TestEnvironment env("forward_limits");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;
    ContractorID idA = 1, idX = 2, idB = 3;

    // Setup topology: A -> X in EQ1, X -> B in EQ2
    env.setupTopologyWithCommissions(EQ1, {{idA, idX, 1000}}, {});
    env.setupTopologyWithCommissions(EQ2, {{idX, idB, 1500}}, {});

    // Exchange: rate=1.5, min=100, max=500
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(15), -1, expiresAt,
                      TrustLineAmount(100), TrustLineAmount(500));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    // Create path: A (eq1) → X (eq1) → X (eq2) [exchange at X] → B (eq2)
    ExchangePath path;
    path.nodes = {idA, idX, idX, idB};
    path.equivalents = {EQ1, EQ1, EQ2, EQ2};
    path.minCapacity = TrustLineAmount(1000);
    path.effectiveExchangeRate = 1.5;

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(15);
    exchange.exchangeRateShift = -1;
    exchange.minExchangeAmount = TrustLineAmount(100);
    exchange.maxExchangeAmount = TrustLineAmount(500);
    exchange.commission = TrustLineAmount(0);
    path.exchangeSteps = {exchange};

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    // Initialize edge capacities
    edgeCapacity[EdgeKey{idA, idX, EQ1}] = 1000.0;
    edgeCapacity[EdgeKey{idX, idB, EQ2}] = 1500.0;

    // Test 1: Below min (50 < 100)
    double output1 = env.pathsManager->forwardSimulatePath(path, 50.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(output1, 0.0, 0.1); // Should be 0 (below min)

    appliedCommissions.clear();
    edgeCapacity.clear();
    edgeCapacity[EdgeKey{idA, idX, EQ1}] = 1000.0;
    edgeCapacity[EdgeKey{idX, idB, EQ2}] = 1500.0;

    // Test 2: Above max (600 > 500) should be rejected
    double output2 = env.pathsManager->forwardSimulatePath(path, 600.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(output2, 0.0, 0.1);
    // Edge capacities remain unchanged because the path was rejected
    const EdgeKey edgeAX{idA, idX, EQ1};
    const EdgeKey edgeXB{idX, idB, EQ2};
    EXPECT_NEAR(edgeCapacity[edgeAX], 1000.0, 0.1);
    EXPECT_NEAR(edgeCapacity[edgeXB], 1500.0, 0.1);

    appliedCommissions.clear();
    edgeCapacity.clear();
    edgeCapacity[EdgeKey{idA, idX, EQ1}] = 1000.0;
    edgeCapacity[EdgeKey{idX, idB, EQ2}] = 1500.0;

    // Test 3: Within range (200 in [100, 500])
    double output3 = env.pathsManager->forwardSimulatePath(path, 200.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(output3, 300.0, 0.1); // 200 * 1.5 = 300
}

// Test 4: Forward Simulation - Capacity Constraint
TEST(PathSimulationTest, ForwardSimulateCapacityConstraint)
{
    TestEnvironment env("forward_capacity");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3;

    // Setup topology: A -> B (1000), B -> C (500)
    env.setupTopologyWithCommissions(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 500} // Bottleneck
        },
        {});

    ExchangePath path;
    path.nodes = {idA, idB, idC};
    path.equivalents = {EQ, EQ, EQ};
    path.minCapacity = TrustLineAmount(500);
    path.effectiveExchangeRate = 1.0;
    path.totalCommissions = TrustLineAmount(0);

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    // Initialize edge capacities
    edgeCapacity[EdgeKey{idA, idB, EQ}] = 1000.0;
    edgeCapacity[EdgeKey{idB, idC, EQ}] = 500.0;

    double output = env.pathsManager->forwardSimulatePath(path, 800.0, appliedCommissions, edgeCapacity);

    // Expected: Limited by B->C capacity = 500
    EXPECT_NEAR(output, 500.0, 0.1);
    EdgeKey bcEdge{idB, idC, EQ};
    EXPECT_NEAR(edgeCapacity[bcEdge], 0.0, 0.1); // Should be exhausted
}

// Test 5: Inverse Simulation - Simple Path
TEST(PathSimulationTest, InverseSimulateSimplePath)
{
    TestEnvironment env("inverse_simple");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3;

    // Setup topology: A -> B (1000) -> C (800)
    env.setupTopologyWithCommissions(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 800}
        },
        {
            {idB, EQ, 10} // Commission at B = 10
        });

    ExchangePath path;
    path.nodes = {idA, idB, idC};
    path.equivalents = {EQ, EQ, EQ};
    path.minCapacity = TrustLineAmount(800);
    path.effectiveExchangeRate = 1.0;
    path.totalCommissions = TrustLineAmount(10);

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    // Target: 490 at C, need to calculate required input at A
    double requiredInput = env.pathsManager->inverseSimulatePath(path, 490.0, appliedCommissions, edgeCapacity);

    // Expected: 490 + 10 (commission) = 500
    EXPECT_NEAR(requiredInput, 500.0, 0.1);
}

// Test 6: Inverse Simulation - Exchange Path
TEST(PathSimulationTest, InverseSimulateWithExchange)
{
    TestEnvironment env("inverse_exchange");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;
    ContractorID idA = 1, idB = 2, idX = 3, idC = 4;

    // Setup topology: A -> B -> X in EQ1, X -> C in EQ2
    env.setupTopologyWithCommissions(EQ1, {{idA, idB, 1000}, {idB, idX, 800}}, {});
    env.setupTopologyWithCommissions(EQ2, {{idX, idC, 1500}}, {});

    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(2), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    // Path: A (eq1) → B (eq1) → X (eq1) → X (eq2) [exchange] → C (eq2)
    ExchangePath path;
    path.nodes = {idA, idB, idX, idX, idC};
    path.equivalents = {EQ1, EQ1, EQ1, EQ2, EQ2};
    path.minCapacity = TrustLineAmount(800);
    path.effectiveExchangeRate = 2.0;

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(2);
    exchange.exchangeRateShift = 0;
    exchange.minExchangeAmount = TrustLineAmount(0);
    exchange.maxExchangeAmount = TrustLineAmount(0);
    exchange.commission = TrustLineAmount(0);
    path.exchangeSteps = {exchange};

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    // Target: 400 in eq2 at C
    double requiredInput = env.pathsManager->inverseSimulatePath(path, 400.0, appliedCommissions, edgeCapacity);

    // Expected: 400 / 2.0 = 200 in eq1
    EXPECT_NEAR(requiredInput, 200.0, 0.1);
}

// Test 7: Inverse Simulation - Exchange Limits
TEST(PathSimulationTest, InverseSimulateExchangeLimits)
{
    TestEnvironment env("inverse_limits");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;
    ContractorID idA = 1, idX = 2, idB = 3;

    env.setupTopologyWithCommissions(EQ1, {{idA, idX, 1000}}, {});
    env.setupTopologyWithCommissions(EQ2, {{idX, idB, 1500}}, {});

    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(15), -1, expiresAt,
                      TrustLineAmount(100), TrustLineAmount(500));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    // Path: A (eq1) → X (eq1) → X (eq2) [exchange] → B (eq2)
    ExchangePath path;
    path.nodes = {idA, idX, idX, idB};
    path.equivalents = {EQ1, EQ1, EQ2, EQ2};
    path.minCapacity = TrustLineAmount(1000);
    path.effectiveExchangeRate = 1.5;

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(15);
    exchange.exchangeRateShift = -1;
    exchange.minExchangeAmount = TrustLineAmount(100);
    exchange.maxExchangeAmount = TrustLineAmount(500);
    exchange.commission = TrustLineAmount(0);
    path.exchangeSteps = {exchange};

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    // Initialize edge capacities
    edgeCapacity[EdgeKey{idA, idX, EQ1}] = 1000.0;
    edgeCapacity[EdgeKey{idX, idB, EQ2}] = 1500.0;

    // Test 1: Target below min threshold (50 / 1.5 ≈ 33 < 100)
    double input1 = env.pathsManager->inverseSimulatePath(path, 50.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(input1, 0.0, 0.1);

    appliedCommissions.clear();
    edgeCapacity.clear();
    edgeCapacity[EdgeKey{idA, idX, EQ1}] = 1000.0;
    edgeCapacity[EdgeKey{idX, idB, EQ2}] = 1500.0;

    // Test 2: Target within range (300 / 1.5 = 200)
    double input2 = env.pathsManager->inverseSimulatePath(path, 300.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(input2, 200.0, 0.1);

    appliedCommissions.clear();
    edgeCapacity.clear();
    edgeCapacity[EdgeKey{idA, idX, EQ1}] = 1000.0;
    edgeCapacity[EdgeKey{idX, idB, EQ2}] = 1500.0;

    // Test 3: Target above max (900 / 1.5 = 600 > 500) should be rejected
    double input3 = env.pathsManager->inverseSimulatePath(path, 900.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(input3, 0.0, 0.1);
}

// Test 8: Inverse Simulation - Capacity Constraint
TEST(PathSimulationTest, InverseSimulateCapacityConstraint)
{
    TestEnvironment env("inverse_capacity");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3;

    env.setupTopologyWithCommissions(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 500} // Max capacity
        },
        {});

    ExchangePath path;
    path.nodes = {idA, idB, idC};
    path.equivalents = {EQ, EQ, EQ};
    path.minCapacity = TrustLineAmount(500);
    path.effectiveExchangeRate = 1.0;

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;
    edgeCapacity[EdgeKey{idB, idC, EQ}] = 500.0;

    // Target: 600 (exceeds capacity of 500)
    double requiredInput = env.pathsManager->inverseSimulatePath(path, 600.0, appliedCommissions, edgeCapacity);

    // Expected: 0 (cannot achieve target)
    EXPECT_NEAR(requiredInput, 0.0, 0.1);
}

// Test 9: Commission Charge Once - Multi-Path
TEST(PathSimulationTest, CommissionChargedOnceAcrossPaths)
{
    TestEnvironment env("commission_once");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3, idD = 4;

    // Setup topology: A -> B -> C and A -> B -> D (B is shared)
    env.setupTopologyWithCommissions(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 500},
            {idB, idD, 500}
        },
        {
            {idB, EQ, 10} // Commission at B
        });

    // Path 1: A -> B -> C
    ExchangePath path1;
    path1.nodes = {idA, idB, idC};
    path1.equivalents = {EQ, EQ, EQ};
    path1.minCapacity = TrustLineAmount(500);
    path1.effectiveExchangeRate = 1.0;
    path1.totalCommissions = TrustLineAmount(10);

    // Path 2: A -> B -> D (shares B)
    ExchangePath path2;
    path2.nodes = {idA, idB, idD};
    path2.equivalents = {EQ, EQ, EQ};
    path2.minCapacity = TrustLineAmount(500);
    path2.effectiveExchangeRate = 1.0;
    path2.totalCommissions = TrustLineAmount(10);

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeCapacity;

    // Simulate path 1 with 100 units
    double output1 = env.pathsManager->forwardSimulatePath(path1, 100.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(output1, 90.0, 0.1); // 100 - 10 commission
    EXPECT_EQ(appliedCommissions.size(), 1);

    // Simulate path 2 with 100 units (commission should NOT be deducted again)
    double output2 = env.pathsManager->forwardSimulatePath(path2, 100.0, appliedCommissions, edgeCapacity);
    EXPECT_NEAR(output2, 100.0, 0.1); // No commission deducted
    EXPECT_EQ(appliedCommissions.size(), 1); // Still only 1 commission recorded
}
