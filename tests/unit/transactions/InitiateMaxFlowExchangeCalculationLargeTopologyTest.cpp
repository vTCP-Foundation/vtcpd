#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/transactions/transactions/max_flow_calculation/InitiateMaxFlowExchangeCalculationTransaction.h"
#include <boost/uuid/uuid_generators.hpp>
#include "core/interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowExchangeCalculationCommand.h"
#include "core/contractors/ContractorsManager.h"
#include "core/contractors/addresses/IPv4WithPortAddress.h"
#include "core/equivalents/EquivalentsSubsystemsRouter.h"
#include "core/rates/manager/ExchangeRatesManager.h"
#include "core/logger/Logger.h"
#include "core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "core/crypto/keychain.h"
#include "core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "core/network/communicator/internal/incoming/TailManager.h"
#include "core/rates/Commission.h"

using namespace testing;

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

// Schematic (addresses mapped to letters for readability):
// A(self=172.18.28.1:2000)
// T(target=172.18.29.20:2000)
// П'ять шляхів A -> ... -> T (обмін на X-вузлах; rate 1.0 1001->2002):
// P1: A -1001-> N2 -1001-> X1 ==exchange(1.0)=> 2002 -> R1 -2002-> T  (cap = 50)
// P2: A -1001-> N4 -1001-> X2 ==exchange(1.0)=> 2002 -> R2 -2002-> T  (cap = 40)
// P3: A -1001-> N4 -1001-> N6 -1001-> X3 ==exchange(1.0)=> 2002 -> T (cap = 30)
// P4: A -1001-> N7 -1001-> X1 ==exchange(1.0)=> 2002 -> R3 -2002-> T  (cap = 25)
// P5: A -1001-> N8 -1001-> N9 -1001-> X2 ==exchange(1.0)=> 2002 -> R4 -2002-> T (cap = 35)
// Dead-ends and commission nodes (not on feasible paths):
//    A -1001-> D1 -1001-> D2 (no exchange to 2002)
//    Nodes D1, D2, D3 may have commissions in 1001 and 2002
// За новою логікою (LP + транзитні комісії раз на пару (вузол, еквівалент),
// обмінні вузли без комісій):
// gross (LP) = 126, unique commissions sum = 10 ⇒ net = 116

TEST(InitiateMaxFlowExchangeCalculationLargeTopologyTest, MaxFlowIs116OnLargeTopology)
{
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_large_topology";
    std::string dbName = "test.db";
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    crypto::Keystore keystore(logger);
    keystore.init();
    EventsInterfaceManager eventsManager({}, {}, logger);

    // Self address A
    vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
    ContractorsManager contractors(ownAddrs, &storage, logger);

    // Router
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

    // Define 19 more nodes (2..20)
    auto addrA  = make_shared<IPv4WithPortAddress>("172.18.28.1:2000");
    auto addr2  = make_shared<IPv4WithPortAddress>("172.18.29.2:2000");  // N2
    auto addr3  = make_shared<IPv4WithPortAddress>("172.18.29.3:2000");  // N3
    auto addr4  = make_shared<IPv4WithPortAddress>("172.18.29.4:2000");  // N4
    auto addr5  = make_shared<IPv4WithPortAddress>("172.18.29.5:2000");  // N5
    auto addr6  = make_shared<IPv4WithPortAddress>("172.18.29.6:2000");  // N6
    auto addr7  = make_shared<IPv4WithPortAddress>("172.18.29.7:2000");  // N7
    auto addr8  = make_shared<IPv4WithPortAddress>("172.18.29.8:2000");  // N8
    auto addr9  = make_shared<IPv4WithPortAddress>("172.18.29.9:2000");  // N9
    auto addr10 = make_shared<IPv4WithPortAddress>("172.18.29.10:2000"); // X1
    auto addr11 = make_shared<IPv4WithPortAddress>("172.18.29.11:2000"); // X2
    auto addr12 = make_shared<IPv4WithPortAddress>("172.18.29.12:2000"); // X3
    auto addr13 = make_shared<IPv4WithPortAddress>("172.18.29.13:2000"); // R1
    auto addr14 = make_shared<IPv4WithPortAddress>("172.18.29.14:2000"); // R2
    auto addr15 = make_shared<IPv4WithPortAddress>("172.18.29.15:2000"); // R3
    auto addr16 = make_shared<IPv4WithPortAddress>("172.18.29.16:2000"); // R4
    auto addr17 = make_shared<IPv4WithPortAddress>("172.18.29.17:2000"); // D1
    auto addr18 = make_shared<IPv4WithPortAddress>("172.18.29.18:2000"); // D2
    auto addr19 = make_shared<IPv4WithPortAddress>("172.18.29.19:2000"); // D3
    auto addr20 = make_shared<IPv4WithPortAddress>("172.18.29.20:2000"); // T

    ASSERT_EQ(router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress()), 0u);
    auto id2  = router.getOrCreateParticipantID(addr2);
    auto id3  = router.getOrCreateParticipantID(addr3);
    auto id4  = router.getOrCreateParticipantID(addr4);
    auto id5  = router.getOrCreateParticipantID(addr5);
    auto id6  = router.getOrCreateParticipantID(addr6);
    auto id7  = router.getOrCreateParticipantID(addr7);
    auto id8  = router.getOrCreateParticipantID(addr8);
    auto id9  = router.getOrCreateParticipantID(addr9);
    auto id10 = router.getOrCreateParticipantID(addr10);
    auto id11 = router.getOrCreateParticipantID(addr11);
    auto id12 = router.getOrCreateParticipantID(addr12);
    auto id13 = router.getOrCreateParticipantID(addr13);
    auto id14 = router.getOrCreateParticipantID(addr14);
    auto id15 = router.getOrCreateParticipantID(addr15);
    auto id16 = router.getOrCreateParticipantID(addr16);
    auto id17 = router.getOrCreateParticipantID(addr17);
    auto id18 = router.getOrCreateParticipantID(addr18);
    auto id19 = router.getOrCreateParticipantID(addr19);
    auto id20 = router.getOrCreateParticipantID(addr20);

    // Build topology in 1001 (source side)
    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    // P1: A->N2(80), N2->X1(70)
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0,   id2,  A(80)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id2, id10, A(70)));
    // P2: A->N4(90), N4->X2(90)
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0,   id4,  A(90)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id4, id11, A(90)));
    // P3: A->N4 (shared with P2), N4->N6(30), N6->X3(100)
    //     cap(P3) = min( A->N4=90, N4->N6=30, N6->X3=100 ) = 30
    //     (вузол N4 спільний у P2 та P3)
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id4, id6,  A(30)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id6, id12, A(100)));
    // P4: Route kept as potential dead-end (no direct link to X1 to avoid duplicate sink-edge usage)
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0,   id7,  A(100)));
    // P5: Route kept as potential dead-end (no link to X2 to avoid duplicate sink-edge usage)
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0,   id8,  A(100)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id8, id9,  A(100)));
    // Dead-ends: A->D1(60), D1->D2(50), D2->D3(40) [no exchange]
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0,    id17, A(60)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id17, id18, A(50)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(id18, id19, A(40)));

    // Build topology in 2002 (receiver side)
    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    // P1: X1->R1(60), R1->T(50)
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(id10, id13, A(60)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(id13, id20, A(50)));
    // P2: X2->R2(40), R2->T(40)
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(id11, id14, A(40)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(id14, id20, A(40)));
    // P3: X3->T(30)
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(id12, id20, A(30)));
    // P4/P5 receiver edges removed to avoid duplicate use of the same sink edges via X1/X2
    // Dead-end edges in 2002 near commission nodes
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(id19, id18, A(10)));

    // Add external exchange rates at X1, X2, X3: 1001 -> 2002, rate = 1.0
    ExchangeRatesManager ratesMgr(io, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate r_eq(EQ_1001, EQ_2002, TrustLineAmount(1), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    ratesMgr.addOrUpdateExternal(id10, r_eq);
    ratesMgr.addOrUpdateExternal(id11, r_eq);
    ratesMgr.addOrUpdateExternal(id12, r_eq);

    // Commissions on nodes along valid paths (include both equivalents where applicable)
    // Exchange nodes (both equivalents): X1(id10), X2(id11), X3(id12)
    tlm1001->storeCommission(id10, EQ_1001, make_shared<Commission>(1));
    tlm2002->storeCommission(id10, EQ_2002, make_shared<Commission>(1));
    tlm1001->storeCommission(id11, EQ_1001, make_shared<Commission>(1));
    tlm2002->storeCommission(id11, EQ_2002, make_shared<Commission>(1));
    tlm1001->storeCommission(id12, EQ_1001, make_shared<Commission>(1));
    tlm2002->storeCommission(id12, EQ_2002, make_shared<Commission>(1));

    // Intermediate non-exchange nodes on active paths
    // In 1001: N4(id4)=2, N2(id2)=1, N6(id6)=1
    tlm1001->storeCommission(id4,  EQ_1001, make_shared<Commission>(2));
    tlm1001->storeCommission(id2,  EQ_1001, make_shared<Commission>(1));
    tlm1001->storeCommission(id6,  EQ_1001, make_shared<Commission>(1));
    // In 2002: R1(id13)=3, R2(id14)=3
    tlm2002->storeCommission(id13, EQ_2002, make_shared<Commission>(3));
    tlm2002->storeCommission(id14, EQ_2002, make_shared<Commission>(3));

    // Keep some commissions on non-used nodes (do not affect optimal net)
    tlm1001->storeCommission(id17, EQ_1001, make_shared<Commission>(10));
    tlm2002->storeCommission(id17, EQ_2002, make_shared<Commission>(5));
    tlm1001->storeCommission(id18, EQ_1001, make_shared<Commission>(5));
    tlm2002->storeCommission(id18, EQ_2002, make_shared<Commission>(10));
    tlm1001->storeCommission(id19, EQ_1001, make_shared<Commission>(5));
    tlm2002->storeCommission(id19, EQ_2002, make_shared<Commission>(5));

    // Tail manager
    TailManager tail(io, logger);

    // Command: target T in receiver eq 2002, with exchange equivalents {1001}
    CommandUUID cmdUUID = boost::uuids::random_generator()();
    std::string cmdStr = buildCommandStr("172.18.29.20:2000", EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    InitiateMaxFlowExchangeCalculationTransaction tx(
        command,
        &contractors,
        &router,
        &ratesMgr,
        &tail,
        logger,
        /*hopsCount*/ 6);

    // First run: collect topology
    auto r1 = tx.run();
    ASSERT_TRUE(r1 != nullptr);

    // Second run: process and compute LP (measure time)
    auto tStart = std::chrono::steady_clock::now();
    auto r2 = tx.run();
    auto tEnd = std::chrono::steady_clock::now();
    auto dtUs = std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count();
    logger.info("InitiateMaxFlowExchangeCalculationLargeTopologyTest")
        << "Max-flow calculation time (second run) = " << dtUs << " us";
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    auto serialized = r2->commandResult()->serialize();
    // Враховано нову логіку і оновлені обмеження: net=116
    std::string expectTuple = std::string("\t12\t") + "172.18.29.20:2000" + "\t116";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos) << serialized;
}


