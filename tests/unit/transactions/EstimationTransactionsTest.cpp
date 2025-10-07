#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>

#include "core/transactions/transactions/max_flow_calculation/EstimateReceiveForPaymentAmountTransaction.h"
#include "core/transactions/transactions/max_flow_calculation/EstimatePaymentForReceiveAmountTransaction.h"
#include "core/interface/commands_interface/commands/max_flow_calculation/EstimateReceiveForPaymentAmountCommand.h"
#include "core/interface/commands_interface/commands/max_flow_calculation/EstimatePaymentForReceiveAmountCommand.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/paths/ExchangePathsManager.h"
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

    // Test environment helper
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
            dbDir = "build-tests/testdb_est_" + testName;
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

        ContractorID registerContractor(const string& address) {
            auto addr = make_shared<IPv4WithPortAddress>(address);
            return router->getOrCreateParticipantID(addr);
        }

        void setupTopology(
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

        // Helper to build command string
        string buildReceiveEstimateCommand(const string& targetAddr, uint64_t paymentAmount,
                                          SerializedEquivalent senderEq, SerializedEquivalent receiverEq)
        {
            stringstream ss;
            ss << "12" << '\t' << targetAddr << '\t' << paymentAmount << '\t' << senderEq << '\t' << receiverEq << '\n';
            return ss.str();
        }

        string buildPaymentEstimateCommand(const string& targetAddr, uint64_t receiveAmount,
                                          SerializedEquivalent receiverEq, SerializedEquivalent senderEq)
        {
            stringstream ss;
            ss << "12" << '\t' << targetAddr << '\t' << receiveAmount << '\t' << receiverEq << '\t' << senderEq << '\n';
            return ss.str();
        }
    };

    // Helper to create paths and store them in cache
    void storeMockPath(ExchangePathsManager* manager, PathCacheKey key,
                      const vector<ContractorID>& nodes,
                      const vector<SerializedEquivalent>& equivs,
                      TrustLineAmount optimalFlow,
                      const vector<ExchangeStep>& exchanges = {})
    {
        OptimalPathResult pathResult;
        pathResult.path().ids = nodes;
        pathResult.path().equivalents = equivs;
        pathResult.path().exchangeSteps = exchanges;
        pathResult.path().minCapacity = optimalFlow;
        pathResult.path().effectiveExchangeRate = 1.0;
        pathResult.path().totalCommissions = TrustLineAmount(0);
        pathResult.optimal_flow = optimalFlow;
        pathResult.received_amount = optimalFlow;
        pathResult.effective_exchange_rate = 1.0;
        pathResult.path_efficiency = 1.0;

        if (!exchanges.empty()) {
            double totalRate = 1.0;
            for (const auto& ex : exchanges) {
                double rate = ex.exchangeRate.convert_to<double>() * pow(10.0, ex.exchangeRateShift);
                totalRate *= rate;
            }
            pathResult.path().effectiveExchangeRate = totalRate;
            pathResult.effective_exchange_rate = totalRate;
        }

        manager->storePaths(key, {pathResult});
    }
}

//=============================================================================
// EstimateReceiveForPaymentAmount Transaction Tests
//=============================================================================

// Topology 1: Single-Equivalent Path (No Exchange)
TEST(EstimateReceiveTransactionTest, Topology1_SingleEquivalentPath)
{
    TestEnvironment env("receive_topology1");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3;

    // Setup topology: A -> B (1000), B -> C (800)
    // Commission: B = 10
    env.setupTopology(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 800}
        },
        {
            {idB, EQ, 10}
        });

    // Register target contractor and get its ID
    auto targetAddr = "127.0.0.1:2003";
    ContractorID targetID = env.registerContractor(targetAddr);

    // Store mock path in cache for target contractor
    PathCacheKey key{targetID, EQ, EQ};
    storeMockPath(env.pathsManager.get(), key, {idA, idB, idC}, {EQ, EQ, EQ}, TrustLineAmount(800));

    // Test 1: Payment 510
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 510, EQ, EQ);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 510 - 10 commission = 500
        EXPECT_NE(serialized.find("200\t500"), string::npos) << "Result: " << serialized;
    }

    // Test 2: Payment 2000 (exceeds capacity)
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 2000, EQ, EQ);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (insufficient capacity - cannot consume full payment)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 3: Payment 100
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 100, EQ, EQ);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 100 - 10 commission = 90
        EXPECT_NE(serialized.find("200\t90"), string::npos) << "Result: " << serialized;
    }
}

// Topology 2: Cross-Equivalent Path with Exchange
TEST(EstimateReceiveTransactionTest, Topology2_CrossEquivalentExchange)
{
    TestEnvironment env("receive_topology2");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;

    // Register target contractor first to get predictable ID
    auto targetAddr = "127.0.0.1:2003";
    ContractorID targetID = env.registerContractor(targetAddr);

    // Use sequential IDs for path nodes
    ContractorID idA = 1, idB = 2, idX = 3;

    // Setup topology: A -> B -> X in EQ1, X -> target in EQ2
    env.setupTopology(EQ1, {{idA, idB, 1000}, {idB, idX, 800}}, {});
    env.setupTopology(EQ2, {{idX, targetID, 2000}}, {});

    // Add exchange rate: EQ1 -> EQ2, rate = 2.0
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(2), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    // Create exchange step
    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(2);
    exchange.exchangeRateShift = 0;
    exchange.minExchangeAmount = TrustLineAmount(0);
    exchange.maxExchangeAmount = TrustLineAmount(0);
    exchange.commission = TrustLineAmount(0);

    // Store path with exchange - nodes: A → B → X → X (exchange) → target
    PathCacheKey key{targetID, EQ1, EQ2};
    OptimalPathResult pathResult;
    pathResult.path().ids = {idA, idB, idX, idX, targetID};
    pathResult.path().equivalents = {EQ1, EQ1, EQ1, EQ2, EQ2};
    pathResult.path().exchangeSteps = {exchange};
    pathResult.path().minCapacity = TrustLineAmount(800);
    pathResult.path().effectiveExchangeRate = 2.0;
    pathResult.path().totalCommissions = TrustLineAmount(0);
    pathResult.optimal_flow = TrustLineAmount(800);
    pathResult.received_amount = TrustLineAmount(1600);
    pathResult.effective_exchange_rate = 2.0;
    pathResult.path_efficiency = 1.0;
    env.pathsManager->storePaths(key, {pathResult});

    // Test 1: Payment 200 in EQ1
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 200, EQ1, EQ2);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 200 * 2.0 = 400 in EQ2
        EXPECT_NE(serialized.find("200\t400"), string::npos) << "Result: " << serialized;
    }

    // Test 2: Payment 1000 in EQ1 (limited by capacity)
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 1000, EQ1, EQ2);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (insufficient capacity - cannot consume full payment)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 3: Payment 400 in EQ1
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 400, EQ1, EQ2);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 400 * 2.0 = 800 in EQ2
        EXPECT_NE(serialized.find("200\t800"), string::npos) << "Result: " << serialized;
    }
}

// Topology 4: Exchange Min/Max Limits
TEST(EstimateReceiveTransactionTest, Topology4_ExchangeLimits)
{
    TestEnvironment env("receive_topology4");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;

    // Register target contractor first
    auto targetAddr = "127.0.0.1:2003";
    ContractorID targetID = env.registerContractor(targetAddr);

    ContractorID idA = 1, idX = 2;

    env.setupTopology(EQ1, {{idA, idX, 1000}}, {});
    env.setupTopology(EQ2, {{idX, targetID, 1500}}, {});

    // Exchange: rate=1.5, min=100, max=500
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(15), -1, expiresAt,
                      TrustLineAmount(100), TrustLineAmount(500));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(15);
    exchange.exchangeRateShift = -1;
    exchange.minExchangeAmount = TrustLineAmount(100);
    exchange.maxExchangeAmount = TrustLineAmount(500);
    exchange.commission = TrustLineAmount(0);

    // Path: A → X → X (exchange) → target
    PathCacheKey key{targetID, EQ1, EQ2};
    OptimalPathResult pathResult;
    pathResult.path().ids = {idA, idX, idX, targetID};
    pathResult.path().equivalents = {EQ1, EQ1, EQ2, EQ2};
    pathResult.path().exchangeSteps = {exchange};
    pathResult.path().minCapacity = TrustLineAmount(1000);
    pathResult.path().effectiveExchangeRate = 1.5;
    pathResult.optimal_flow = TrustLineAmount(1000);
    pathResult.received_amount = TrustLineAmount(1500);
    env.pathsManager->storePaths(key, {pathResult});

    // Test 1: Payment 50 (below min)
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 50, EQ1, EQ2);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (payment below min exchange amount 100)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 2: Payment 600 (above max, path rejected)
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 600, EQ1, EQ2);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (payment exceeds max exchange amount 500)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 3: Payment 200 (within range)
    {
        string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 200, EQ1, EQ2);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

        EstimateReceiveForPaymentAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 200 * 1.5 = 300
        EXPECT_NE(serialized.find("200\t300"), string::npos) << "Result: " << serialized;
    }
}

// Error Case: No Cached Paths
TEST(EstimateReceiveTransactionTest, ErrorCase_NoCachedPaths)
{
    TestEnvironment env("receive_error_no_paths");

    const SerializedEquivalent EQ = 1001;

    env.router->initNewEquivalent(EQ);

    auto targetAddr = "127.0.0.1:2003";
    env.registerContractor(targetAddr);

    string cmdStr = env.buildReceiveEstimateCommand(targetAddr, 100, EQ, EQ);
    CommandUUID cmdUUID = boost::uuids::random_generator()();
    auto command = make_shared<EstimateReceiveForPaymentAmountCommand>(cmdUUID, cmdStr);

    EstimateReceiveForPaymentAmountTransaction tx(
        command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

    auto result = tx.run();
    ASSERT_TRUE(result != nullptr);
    ASSERT_TRUE(result->commandResult() != nullptr);

    auto serialized = result->commandResult()->serialize();
    // Expected: Error 462 (no cached paths)
    EXPECT_NE(serialized.find("462"), string::npos) << "Result: " << serialized;
}

//=============================================================================
// EstimatePaymentForReceiveAmount Transaction Tests
//=============================================================================

// Topology 1: Single-Equivalent Path (No Exchange)
TEST(EstimatePaymentTransactionTest, Topology1_SingleEquivalentPath)
{
    TestEnvironment env("payment_topology1");

    const SerializedEquivalent EQ = 1001;
    ContractorID idA = 1, idB = 2, idC = 3;

    env.setupTopology(EQ,
        {
            {idA, idB, 1000},
            {idB, idC, 800}
        },
        {
            {idB, EQ, 10}
        });

    // Register target contractor
    auto targetAddr = "127.0.0.1:2003";
    ContractorID targetID = env.registerContractor(targetAddr);

    PathCacheKey key{targetID, EQ, EQ};
    storeMockPath(env.pathsManager.get(), key, {idA, idB, idC}, {EQ, EQ, EQ}, TrustLineAmount(800));

    // Test 1: Receive 500
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 500, EQ, EQ);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 500 + 10 commission = 510
        EXPECT_NE(serialized.find("200\t510"), string::npos) << "Result: " << serialized;
    }

    // Test 2: Receive 900 (insufficient)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 900, EQ, EQ);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (max deliverable ~790)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 3: Receive 790 (exactly at capacity)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 790, EQ, EQ);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 790 + 10 commission = 800
        EXPECT_NE(serialized.find("200\t800"), string::npos) << "Result: " << serialized;
    }
}

// Topology 2: Cross-Equivalent Path with Exchange
TEST(EstimatePaymentTransactionTest, Topology2_CrossEquivalentExchange)
{
    TestEnvironment env("payment_topology2");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;

    // Register target contractor first
    auto targetAddr = "127.0.0.1:2003";
    ContractorID targetID = env.registerContractor(targetAddr);

    ContractorID idA = 1, idB = 2, idX = 3;

    env.setupTopology(EQ1, {{idA, idB, 1000}, {idB, idX, 800}}, {});
    env.setupTopology(EQ2, {{idX, targetID, 2000}}, {});

    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(2), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(2);
    exchange.exchangeRateShift = 0;
    exchange.minExchangeAmount = TrustLineAmount(0);
    exchange.maxExchangeAmount = TrustLineAmount(0);
    exchange.commission = TrustLineAmount(0);

    // Path: A → B → X → X (exchange) → target
    PathCacheKey key{targetID, EQ1, EQ2};
    OptimalPathResult pathResult;
    pathResult.path().ids = {idA, idB, idX, idX, targetID};
    pathResult.path().equivalents = {EQ1, EQ1, EQ1, EQ2, EQ2};
    pathResult.path().exchangeSteps = {exchange};
    pathResult.path().minCapacity = TrustLineAmount(800);
    pathResult.path().effectiveExchangeRate = 2.0;
    pathResult.optimal_flow = TrustLineAmount(800);
    pathResult.received_amount = TrustLineAmount(1600);
    env.pathsManager->storePaths(key, {pathResult});

    // Test 1: Receive 400 in EQ2
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 400, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 400 / 2.0 = 200 in EQ1
        EXPECT_NE(serialized.find("200\t200"), string::npos) << "Result: " << serialized;
    }

    // Test 2: Receive 2000 in EQ2 (insufficient)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 2000, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (max deliverable 1600)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 3: Receive 1600 in EQ2 (exactly at capacity)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 1600, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 1600 / 2.0 = 800 in EQ1
        EXPECT_NE(serialized.find("200\t800"), string::npos) << "Result: " << serialized;
    }
}

// Topology 4: Exchange Min/Max Limits
TEST(EstimatePaymentTransactionTest, Topology4_ExchangeLimits)
{
    TestEnvironment env("payment_topology4");

    const SerializedEquivalent EQ1 = 1001;
    const SerializedEquivalent EQ2 = 2002;

    // Register target contractor first
    auto targetAddr = "127.0.0.1:2003";
    ContractorID targetID = env.registerContractor(targetAddr);

    ContractorID idA = 1, idX = 2;

    env.setupTopology(EQ1, {{idA, idX, 1000}}, {});
    env.setupTopology(EQ2, {{idX, targetID, 1500}}, {});

    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ1, EQ2, TrustLineAmount(15), -1, expiresAt,
                      TrustLineAmount(100), TrustLineAmount(500));
    env.ratesManager->addOrUpdateExternal(idX, rate);

    ExchangeStep exchange;
    exchange.nodeID = idX;
    exchange.fromEquivalent = EQ1;
    exchange.toEquivalent = EQ2;
    exchange.exchangeRate = TrustLineAmount(15);
    exchange.exchangeRateShift = -1;
    exchange.minExchangeAmount = TrustLineAmount(100);
    exchange.maxExchangeAmount = TrustLineAmount(500);
    exchange.commission = TrustLineAmount(0);

    // Path: A → X → X (exchange) → target
    PathCacheKey key{targetID, EQ1, EQ2};
    OptimalPathResult pathResult;
    pathResult.path().ids = {idA, idX, idX, targetID};
    pathResult.path().equivalents = {EQ1, EQ1, EQ2, EQ2};
    pathResult.path().exchangeSteps = {exchange};
    pathResult.path().minCapacity = TrustLineAmount(1000);
    pathResult.path().effectiveExchangeRate = 1.5;
    pathResult.optimal_flow = TrustLineAmount(1000);
    pathResult.received_amount = TrustLineAmount(1500);
    env.pathsManager->storePaths(key, {pathResult});

    // Test 1: Receive 50 in EQ2 (would require < min)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 50, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (50/1.5 ≈ 33 < min 100)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }

    // Test 2: Receive 600 in EQ2 (requires 400 input, within max 500)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 600, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 600 / 1.5 = 400 in EQ1
        EXPECT_NE(serialized.find("200\t400"), string::npos) << "Result: " << serialized;
    }

    // Test 3: Receive 750 in EQ2 (exactly at max)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 750, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: 750 / 1.5 = 500 in EQ1
        EXPECT_NE(serialized.find("200\t500"), string::npos) << "Result: " << serialized;
    }

    // Test 4: Receive 800 in EQ2 (exceeds max)
    {
        string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 800, EQ2, EQ1);
        CommandUUID cmdUUID = boost::uuids::random_generator()();
        auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

        EstimatePaymentForReceiveAmountTransaction tx(
            command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

        auto result = tx.run();
        ASSERT_TRUE(result != nullptr);
        ASSERT_TRUE(result->commandResult() != nullptr);

        auto serialized = result->commandResult()->serialize();
        // Expected: Error 412 (800/1.5 ≈ 533 > max 500)
        EXPECT_NE(serialized.find("412"), string::npos) << "Result: " << serialized;
    }
}

// Error Case: No Cached Paths
TEST(EstimatePaymentTransactionTest, ErrorCase_NoCachedPaths)
{
    TestEnvironment env("payment_error_no_paths");

    const SerializedEquivalent EQ = 1001;

    env.router->initNewEquivalent(EQ);

    auto targetAddr = "127.0.0.1:2003";
    env.registerContractor(targetAddr);

    string cmdStr = env.buildPaymentEstimateCommand(targetAddr, 100, EQ, EQ);
    CommandUUID cmdUUID = boost::uuids::random_generator()();
    auto command = make_shared<EstimatePaymentForReceiveAmountCommand>(cmdUUID, cmdStr);

    EstimatePaymentForReceiveAmountTransaction tx(
        command, env.contractors.get(), env.router.get(), env.pathsManager.get(), env.logger);

    auto result = tx.run();
    ASSERT_TRUE(result != nullptr);
    ASSERT_TRUE(result->commandResult() != nullptr);

    auto serialized = result->commandResult()->serialize();
    // Expected: Error 462 (no cached paths)
    EXPECT_NE(serialized.find("462"), string::npos) << "Result: " << serialized;
}
