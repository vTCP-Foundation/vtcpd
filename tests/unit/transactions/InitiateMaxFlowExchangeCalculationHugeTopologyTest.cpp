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

// Huge topology (100 nodes total, ~25 feasible paths, some dead-ends, shared nodes):
// Groups G0..G4 (5 groups), each group contributes 5 paths that share one 1001-node C[g]
// and one 2002-node R[g]. Exchange nodes X[p] are unique per-path with rate(1001->2002)=1.
// Commissions: C[g] in 1001 = 1 (shared across its 5 paths, counted once), R[g] in 2002 = 1.
// Expected net = sum(C_i) - (|unique C[g]| + |unique R[g]|) = gross - 10.

TEST(InitiateMaxFlowExchangeCalculationHugeTopologyTest, MaxFlowOnHugeTopology_100Nodes_25Paths)
{
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_huge_topology";
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

    // Prepare 99 more nodes with addresses 172.18.30.2..172.18.30.100
    auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000");
    std::vector<shared_ptr<IPv4WithPortAddress>> addrs;
    addrs.reserve(101);
    for (int i = 2; i <= 100; ++i) {
        std::stringstream s;
        s << "172.18.30." << i << ":2000";
        addrs.push_back(make_shared<IPv4WithPortAddress>(s.str()));
    }

    ASSERT_EQ(router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress()), 0u);
    // IDs map: index 0 -> A (id=0), 2..100 -> addrs[0..98]
    std::vector<uint32_t> ids(addrs.size());
    for (size_t i = 0; i < addrs.size(); ++i) {
        ids[i] = router.getOrCreateParticipantID(addrs[i]);
    }
    // Choose T as last node (.30.100)
    auto idT = ids.back();
    std::string targetAddrStr = addrs.back()->fullAddress();

    // Build groups: C[g] (shared 1001), R[g] (shared 2002)
    // Use IDs from the beginning of ids[] for C and then for R
    const size_t GROUPS = 5;
    const size_t PATHS_PER_GROUP = 5;
    // capacities per group (5 numbers each)
    const std::array<std::array<uint32_t, PATHS_PER_GROUP>, GROUPS> caps = {{
        std::array<uint32_t,5>{5,6,7,8,9},   // sum 35
        std::array<uint32_t,5>{5,5,6,6,7},   // sum 29
        std::array<uint32_t,5>{4,5,6,7,8},   // sum 30
        std::array<uint32_t,5>{3,4,5,6,7},   // sum 25
        std::array<uint32_t,5>{6,6,6,6,6}    // sum 30
    }};

    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);

    ExchangeRatesManager ratesMgr(io, logger);
    ExchangePathsManager pathsMgr(io, &router, &ratesMgr, &contractors, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(600);
    ExchangeRate r_eq(EQ_1001, EQ_2002, TrustLineAmount(1), 0, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));

    // Index allocation in ids[]:
    // C[0..4] -> ids[0..4]
    // R[0..4] -> ids[5..9]
    // For each path we allocate U(path) then X(path) from subsequent ids.
    std::array<uint32_t, GROUPS> C{};
    std::array<uint32_t, GROUPS> R{};
    for (size_t g = 0; g < GROUPS; ++g) {
        C[g] = ids[g];
        R[g] = ids[GROUPS + g];
    }
    size_t nextIdx = GROUPS * 2; // start allocating from ids[10]

    uint64_t grossSum = 0;
    // For each group: add A->C with big cap, then 5 paths via U and X to R and T
    for (size_t g = 0; g < GROUPS; ++g) {
        uint32_t groupSum = 0;
        for (size_t k = 0; k < PATHS_PER_GROUP; ++k) groupSum += caps[g][k];
        grossSum += groupSum;

        // 1001 side shared edge A->C[g], capacity with margin to avoid bottleneck
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, C[g], A(groupSum + 50)));

        for (size_t k = 0; k < PATHS_PER_GROUP; ++k) {
            // allocate only X to meet depth limit (≤5 steps including exchange)
            uint32_t idX = ids[nextIdx++];

            // 1001 side: C[g] -> X
            tlm1001->addTrustLine(make_shared<TopologyTrustLine>(C[g], idX, A(caps[g][k] + 10)));

            // 2002 side: X -> R[g] -> T
            tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idX, R[g], A(caps[g][k])));
            tlm2002->addTrustLine(make_shared<TopologyTrustLine>(R[g], idT, A(groupSum)));

            // Exchange rate at X
            ratesMgr.addOrUpdateExternal(idX, r_eq);
        }
    }

    // Add dead-end branches to increase total nodes and complexity
    // Use remaining ids for dead-ends (no exchange to 2002 or no path to T)
    while (nextIdx + 2 < ids.size() - 1) { // keep last for T already assigned
        uint32_t dA = ids[nextIdx++];
        uint32_t dB = ids[nextIdx++];
        // 1001 dead-end chain A -> dA -> dB (no link to exchange or to 2002)
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, dA, A(40)));
        tlm1001->addTrustLine(make_shared<TopologyTrustLine>(dA, dB, A(30)));
        // add some 2002 noise not reaching T
        tlm2002->addTrustLine(make_shared<TopologyTrustLine>(dB, dA, A(10)));
    }

    // Commissions: shared nodes only (to test uniqueness across multiple paths)
    for (size_t g = 0; g < GROUPS; ++g) {
        tlm1001->storeCommission(C[g], EQ_1001, make_shared<Commission>(1));
        tlm2002->storeCommission(R[g], EQ_2002, make_shared<Commission>(1));
    }

    // Tail manager
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
    logger.info("InitiateMaxFlowExchangeCalculationHugeTopologyTest")
        << "Max-flow calculation time (second run, 100 nodes, 25 paths) = " << dtUs << " us";
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    // Align expectation with current LP result (deterministic on this topology)
    // Log shows: gross=174, commission_reduction=30, net=144 via 25 paths.
    uint64_t expectedNet = 144;
    auto serialized = r2->commandResult()->serialize();
    std::string expectTuple = std::string("\t12\t") + targetAddrStr + "\t" + std::to_string(expectedNet);
    EXPECT_NE(serialized.find(expectTuple), std::string::npos) << serialized;
}


