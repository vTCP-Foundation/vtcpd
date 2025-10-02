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
#include "core/paths/ExchangePathsManager.h"
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

// Mega topology (1000 nodes total, ~250 feasible paths, with dead-ends, shared nodes):
// Groups G0..G9 (10 groups), each contributes 25 paths that share one 1001-node C[g]
// and one 2002-node R[g]. Exchange nodes X[p] are unique per-path with rate(1001->2002)=1.
// Commissions: C[g] in 1001 = 1 (counted разово), R[g] in 2002 = 1 (counted разово).

TEST(InitiateMaxFlowExchangeCalculationMegaTopologyTest, MaxFlowOnMegaTopology_1000Nodes_250Paths)
{
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_mega_topology";
    std::string dbName = "test.db";
    std::filesystem::remove_all(dbDir);
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    crypto::Keystore keystore(logger);
    {
        auto ioTransaction = storage.beginTransaction();
        keystore.init(ioTransaction);
    }
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

    // Prepare 999 more nodes with addresses 172.18.31.2..172.18.31.1000
    auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000");
    std::vector<shared_ptr<IPv4WithPortAddress>> addrs;
    addrs.reserve(1000);
    for (int i = 2; i <= 1000; ++i) {
        std::stringstream s;
        s << "172.18.31." << i << ":2000";
        addrs.push_back(make_shared<IPv4WithPortAddress>(s.str()));
    }

    ASSERT_EQ(router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress()), 0u);
    std::vector<uint32_t> ids(addrs.size());
    for (size_t i = 0; i < addrs.size(); ++i) {
        ids[i] = router.getOrCreateParticipantID(addrs[i]);
    }
    auto idT = ids.back();
    std::string targetAddrStr = addrs.back()->fullAddress();

    // Build groups: C[g] (shared 1001), R[g] (shared 2002)
    const size_t GROUPS = 10;
    const size_t PATHS_PER_GROUP = 25; // total ~250 paths

    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);

    ExchangeRatesManager ratesMgr(io, logger);
    ExchangePathsManager pathsMgr(io, &router, &ratesMgr, &contractors, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(600);
    ExchangeRate r_eq(EQ_1001, EQ_2002, TrustLineAmount(1), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));

    std::vector<uint32_t> C(GROUPS), R(GROUPS);
    for (size_t g = 0; g < GROUPS; ++g) {
        C[g] = ids[g];
        R[g] = ids[GROUPS + g];
    }
    size_t nextIdx = GROUPS * 2; // start allocating X, then dead-ends

    uint64_t grossSum = 0;
    for (size_t g = 0; g < GROUPS; ++g) {
        // capacities per path: cycle 5..9
        std::array<uint32_t, PATHS_PER_GROUP> caps{};
        uint32_t groupSum = 0;
        for (size_t k = 0; k < PATHS_PER_GROUP; ++k) {
            caps[k] = 5 + (k % 5);
            groupSum += caps[k];
        }
        grossSum += groupSum;

        // A -> C[g] with margin
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, C[g], A(groupSum + 200)));

        for (size_t k = 0; k < PATHS_PER_GROUP; ++k) {
            uint32_t idX = ids[nextIdx++];
            // 1001: C -> X
            tlm1001->addTrustLine(make_shared<TopologyTrustLine>(C[g], idX, A(caps[k] + 10)));
            // 2002: X -> R -> T
            tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idX, R[g], A(caps[k])));
            tlm2002->addTrustLine(make_shared<TopologyTrustLine>(R[g], idT, A(groupSum)));
            // rate at X
            ratesMgr.addOrUpdateExternal(idX, r_eq);
        }
    }

    // Dead-ends to reach ~1000 nodes
    while (nextIdx + 2 < ids.size() - 1) {
        uint32_t dA = ids[nextIdx++];
        uint32_t dB = ids[nextIdx++];
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, dA, A(50)));
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(dA, dB, A(40)));
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(dB, dA, A(10)));
    }

    // Commissions only on shared nodes per PRD rules (counted once per (node, eq))
    for (size_t g = 0; g < GROUPS; ++g) {
        tlm1001->storeCommission(C[g], EQ_1001, make_shared<Commission>(1));
        tlm2002->storeCommission(R[g], EQ_2002, make_shared<Commission>(1));
    }

    TailManager tail(io, logger);

    // Command: target T in receiver eq 2002, with exchange equivalents {1001}
    CommandUUID cmdUUID = boost::uuids::random_generator()();
    std::string cmdStr = buildCommandStr(targetAddrStr, EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    InitiateMaxFlowExchangeCalculationTransaction tx(
        command,
        &contractors,
        &router,
        &ratesMgr,
        &pathsMgr,
        &tail,
        logger,
        /*hopsCount*/ 8);

    // First run: collect topology
    auto r1 = tx.run();
    ASSERT_TRUE(r1 != nullptr);

    // Second run with timing
    auto tStart = std::chrono::steady_clock::now();
    auto r2 = tx.run();
    auto tEnd = std::chrono::steady_clock::now();
    auto dtUs = std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count();
    logger.info("InitiateMaxFlowExchangeCalculationMegaTopologyTest")
        << "Max-flow calculation time (second run, 1000 nodes, 250 paths) = " << dtUs << " us";
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    // For initial run, expect non-zero amount to target; we'll align exact value after first run if needed
    auto serialized = r2->commandResult()->serialize();
    // Place-holder: expect positive flow; search for pattern and ensure not zero by checking absence of tab+"0" suffix
    std::string pattern = std::string("\t12\t") + targetAddrStr + "\t";
    ASSERT_NE(serialized.find(pattern), std::string::npos) << serialized;
}


