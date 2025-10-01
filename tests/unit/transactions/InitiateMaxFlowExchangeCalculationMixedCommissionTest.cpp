#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>

#include "core/transactions/transactions/max_flow_calculation/InitiateMaxFlowExchangeCalculationTransaction.h"
#include <boost/uuid/uuid_generators.hpp>
#include "core/interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowExchangeCalculationCommand.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/network/communicator/internal/incoming/TailManager.h"
#include "core/logger/Logger.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/crypto/keychain.h"
#include "core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "core/rates/Commission.h"

using namespace testing;

// Test case for mixed commission handling with edge capacity constraints
// Topology: A->B(300) B->C(200) C->D(250) in eq 1001; C->D(250) D->E(500) in eq 2002
// Commission: B has 10 in eq 1001, C has 20 in eq 1001
// Exchange: D has rate 1:1 from eq 1001 to eq 2002
// Expected: 180 in eq 2002
// Reasoning:
// - A can send up to 300, but B->C is limited to 200
// - B can take 10 commission "on top" since A->B=300 > B->C=200+10
// - C cannot take 20 commission "on top" since B->C=200 < C->D=180+20
// - So flow: A->B(210) -> B commission(10) -> B->C(200) -> C commission(20) -> C->D(180) -> exchange(180) -> D->E(180)

namespace {

    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    std::string buildCommandStr(const std::string &targetIPv4WithPort,
                                SerializedEquivalent receiverEq,
                                const std::vector<SerializedEquivalent> &exchangeEqs)
    {
        std::stringstream ss;
        ss << "1" << '\t';
        ss << "12" << '\t' << targetIPv4WithPort << '\t';
        ss << receiverEq;
        for (auto eq : exchangeEqs) {
            ss << '\t' << eq;
        }
        ss << '\n';
        return ss.str();
    }
}

TEST(InitiateMaxFlowExchangeCalculationMixedCommissionTest, MaxFlowWithMixedCommissionConstraintsIs180)
{
    // Setup
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_mixed_commission";
    std::string dbName = "test.db";
    std::filesystem::remove_all(dbDir);
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    crypto::Keystore keystore(logger);
    {
        auto ioTransaction = storage.beginTransaction();
        keystore.init(ioTransaction);
    }
    EventsInterfaceManager eventsManager({}, {}, logger);

    // Self address: A = 172.18.28.1:2000
    vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
    ContractorsManager contractors(ownAddrs, &storage, logger);

    vector<SerializedEquivalent> gateways;
    EquivalentsSubsystemsRouter router(
        &storage,
        &keystore,
        &contractors,
        &eventsManager,
        io,
        gateways,
        logger);

    // Add equivalents
    const SerializedEquivalent EQ_1001 = 1001;
    const SerializedEquivalent EQ_2002 = 2002;
    router.initNewEquivalent(EQ_1001);
    router.initNewEquivalent(EQ_2002);

    // Create participant addresses
    auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000"); // A (self)
    auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000"); // B
    auto addrC = make_shared<IPv4WithPortAddress>("172.18.28.3:2000"); // C
    auto addrD = make_shared<IPv4WithPortAddress>("172.18.28.4:2000"); // D (exchange node)
    auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000"); // E (target)

    // Assign participant IDs
    auto idA = router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress()); // 0
    auto idB = router.getOrCreateParticipantID(addrB); // 2
    auto idC = router.getOrCreateParticipantID(addrC); // 4
    auto idD = router.getOrCreateParticipantID(addrD); // 3  
    auto idE = router.getOrCreateParticipantID(addrE); // 1

    // Build topology in equivalent 1001: A->B(300) B->C(200) C->D(250)
    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idA, idB, A(300)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(200)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(250)));

    // Build topology in equivalent 2002: D->E(500)
    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(500)));

    // Set commissions
    // B has commission 10 in equivalent 1001
    auto commissionB = make_shared<Commission>(10);
    tlm1001->storeCommission(idB, EQ_1001, commissionB);
    // C has commission 20 in equivalent 1001  
    auto commissionC = make_shared<Commission>(20);
    tlm1001->storeCommission(idC, EQ_1001, commissionC);

    ExchangeRatesManager ratesManager(io, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ_1001, EQ_2002, TrustLineAmount(1), /*shift*/ 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    ratesManager.addOrUpdateExternal(idD, rate);

    TailManager tailManager(io, logger);

    CommandUUID cmdUUID = boost::uuids::random_generator()();
    auto cmdStr = buildCommandStr("172.18.28.5:2000", EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    InitiateMaxFlowExchangeCalculationTransaction transaction(
        command,
        &contractors,
        &router,
        &ratesManager,
        &tailManager,
        logger,
        6);

    auto r1 = transaction.run();
    ASSERT_TRUE(r1 != nullptr);

    auto r2 = transaction.run();
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    auto serialized = r2->commandResult()->serialize();

    std::string expectTuple = std::string("\t12\t") + "172.18.28.5:2000" + "\t180";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos)
        << "Expected 180 but got: " << serialized;
}

TEST(InitiateMaxFlowExchangeCalculationMixedCommissionTest, MaxFlowWithMixedCommissionConstraintsIs200)
{
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_mixed_commission_case2";
    std::string dbName = "test.db";
    std::filesystem::remove_all(dbDir);
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    crypto::Keystore keystore(logger);
    {
        auto ioTransaction = storage.beginTransaction();
        keystore.init(ioTransaction);
    }
    EventsInterfaceManager eventsManager({}, {}, logger);

    vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
    ContractorsManager contractors(ownAddrs, &storage, logger);

    vector<SerializedEquivalent> gateways;
    EquivalentsSubsystemsRouter router(
        &storage,
        &keystore,
        &contractors,
        &eventsManager,
        io,
        gateways,
        logger);

    const SerializedEquivalent EQ_1001 = 1001;
    const SerializedEquivalent EQ_2002 = 2002;
    router.initNewEquivalent(EQ_1001);
    router.initNewEquivalent(EQ_2002);

    auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000");
    auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000");
    auto addrC = make_shared<IPv4WithPortAddress>("172.18.28.3:2000");
    auto addrD = make_shared<IPv4WithPortAddress>("172.18.28.4:2000");
    auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000");

    auto idA = router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress());
    auto idB = router.getOrCreateParticipantID(addrB);
    auto idC = router.getOrCreateParticipantID(addrC);
    auto idD = router.getOrCreateParticipantID(addrD);
    auto idE = router.getOrCreateParticipantID(addrE);

    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idA, idB, A(300)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(500)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(250)));

    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(200)));

    auto commissionB = make_shared<Commission>(10);
    tlm1001->storeCommission(idB, EQ_1001, commissionB);
    auto commissionC = make_shared<Commission>(20);
    tlm1001->storeCommission(idC, EQ_1001, commissionC);

    ExchangeRatesManager ratesManager(io, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ_1001, EQ_2002, TrustLineAmount(1), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    ratesManager.addOrUpdateExternal(idD, rate);

    TailManager tailManager(io, logger);

    CommandUUID cmdUUID = boost::uuids::random_generator()();
    auto cmdStr = buildCommandStr("172.18.28.5:2000", EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    InitiateMaxFlowExchangeCalculationTransaction transaction(
        command,
        &contractors,
        &router,
        &ratesManager,
        &tailManager,
        logger,
        6);

    auto r1 = transaction.run();
    ASSERT_TRUE(r1 != nullptr);

    auto r2 = transaction.run();
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    auto serialized = r2->commandResult()->serialize();

    std::string expectTuple = std::string("\t12\t") + "172.18.28.5:2000" + "\t200";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos)
        << "Expected 200 but got: " << serialized;
}

TEST(InitiateMaxFlowExchangeCalculationMixedCommissionTest, MaxFlowRespectsSharedEdgeCapacitiesIs125)
{
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_edge_capacity";
    std::string dbName = "test.db";
    std::filesystem::remove_all(dbDir);
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    crypto::Keystore keystore(logger);
    {
        auto ioTransaction = storage.beginTransaction();
        keystore.init(ioTransaction);
    }
    EventsInterfaceManager eventsManager({}, {}, logger);

    vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
    ContractorsManager contractors(ownAddrs, &storage, logger);

    vector<SerializedEquivalent> gateways;
    EquivalentsSubsystemsRouter router(
        &storage,
        &keystore,
        &contractors,
        &eventsManager,
        io,
        gateways,
        logger);

    const SerializedEquivalent EQ_1001 = 1001;
    const SerializedEquivalent EQ_2002 = 2002;
    router.initNewEquivalent(EQ_1001);
    router.initNewEquivalent(EQ_2002);

    auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000");
    auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000");
    auto addrC = make_shared<IPv4WithPortAddress>("172.18.28.3:2000");
    auto addrD = make_shared<IPv4WithPortAddress>("172.18.28.4:2000");
    auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000");

    auto idA = router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress());
    auto idB = router.getOrCreateParticipantID(addrB);
    auto idC = router.getOrCreateParticipantID(addrC);
    auto idD = router.getOrCreateParticipantID(addrD);
    auto idE = router.getOrCreateParticipantID(addrE);

    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idA, idB, A(3000)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(2500)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(2000)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(5000)));

    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(250)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(200)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(500)));

    ExchangeRatesManager ratesManager(io, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rateC(EQ_1001, EQ_2002, TrustLineAmount(5), /*shift*/ -2, expiresAt,
                       TrustLineAmount(0), TrustLineAmount(0));
    ExchangeRate rateD(EQ_1001, EQ_2002, TrustLineAmount(5), /*shift*/ -2, expiresAt,
                       TrustLineAmount(0), TrustLineAmount(0));
    ratesManager.addOrUpdateExternal(idC, rateC);
    ratesManager.addOrUpdateExternal(idD, rateD);

    TailManager tailManager(io, logger);

    CommandUUID cmdUUID = boost::uuids::random_generator()();
    auto cmdStr = buildCommandStr("172.18.28.5:2000", EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    InitiateMaxFlowExchangeCalculationTransaction transaction(
        command,
        &contractors,
        &router,
        &ratesManager,
        &tailManager,
        logger,
        6);

    auto r1 = transaction.run();
    ASSERT_TRUE(r1 != nullptr);

    auto r2 = transaction.run();
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    auto serialized = r2->commandResult()->serialize();

    std::string expectTuple = std::string("\t12\t") + "172.18.28.5:2000" + "\t125";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos)
        << "Expected 125 but got: " << serialized;
}

TEST(InitiateMaxFlowExchangeCalculationMixedCommissionTest, MaxFlowWithSingleTransitCommissionAppliedOnceIs990)
{
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_transit_commission_once";
    std::string dbName = "test.db";
    std::filesystem::remove_all(dbDir);
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    crypto::Keystore keystore(logger);
    {
        auto ioTransaction = storage.beginTransaction();
        keystore.init(ioTransaction);
    }
    EventsInterfaceManager eventsManager({}, {}, logger);

    vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.31.1:2000"}};
    ContractorsManager contractors(ownAddrs, &storage, logger);

    vector<SerializedEquivalent> gateways;
    EquivalentsSubsystemsRouter router(
        &storage,
        &keystore,
        &contractors,
        &eventsManager,
        io,
        gateways,
        logger);

    const SerializedEquivalent EQ_2002 = 2002;
    router.initNewEquivalent(EQ_2002);

    auto addrA = make_shared<IPv4WithPortAddress>("172.18.31.1:2000");
    auto addrB = make_shared<IPv4WithPortAddress>("172.18.31.2:2000");
    auto addrC = make_shared<IPv4WithPortAddress>("172.18.31.3:2000");
    auto addrD = make_shared<IPv4WithPortAddress>("172.18.31.4:2000");
    auto addrE = make_shared<IPv4WithPortAddress>("172.18.31.5:2000");
    auto addrF = make_shared<IPv4WithPortAddress>("172.18.31.6:2000");

    auto idA = router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress());
    auto idB = router.getOrCreateParticipantID(addrB);
    auto idC = router.getOrCreateParticipantID(addrC);
    auto idD = router.getOrCreateParticipantID(addrD);
    auto idE = router.getOrCreateParticipantID(addrE);
    auto idF = router.getOrCreateParticipantID(addrF);

    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idA, idB, A(1000)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idB, idF, A(500)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(800)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC, idF, A(200)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idB, idD, A(700)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(300)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idE, idF, A(900)));

    auto commissionB = make_shared<Commission>(10);
    tlm2002->storeCommission(idB, EQ_2002, commissionB);

    ExchangeRatesManager ratesManager(io, logger);

    TailManager tailManager(io, logger);

    CommandUUID cmdUUID = boost::uuids::random_generator()();
    auto cmdStr = buildCommandStr("172.18.31.6:2000", EQ_2002, {});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    InitiateMaxFlowExchangeCalculationTransaction transaction(
        command,
        &contractors,
        &router,
        &ratesManager,
        &tailManager,
        logger,
        6);

    auto r1 = transaction.run();
    ASSERT_TRUE(r1 != nullptr);

    auto r2 = transaction.run();
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    auto serialized = r2->commandResult()->serialize();

    std::string expectTuple = std::string("\t12\t") + "172.18.31.6:2000" + "\t990";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos)
        << "Expected 990 but got: " << serialized;
}
