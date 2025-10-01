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

using namespace testing;

// This test reproduces the topology printed in operations.log (lines 150-166),
// builds the exact participants map, trust lines in equivalents 1001 and 2002,
// and the external exchange rate at node C, then runs applyCustomLogic and
// verifies that the maximal receivable flow equals 200 in equivalent 2002.

namespace {
    // Helpers to create shared amount easily
    inline ConstSharedTrustLineAmount A(uint64_t v) {
        return make_shared<TrustLineAmount>(v);
    }

    // Build command string following parser format used in command tests
    std::string buildCommandStr(const std::string &targetIPv4WithPort,
                                SerializedEquivalent receiverEq,
                                const std::vector<SerializedEquivalent> &exchangeEqs)
    {
        std::stringstream ss;
        // contractorsCount
        ss << "1" << '\t';
        // addressType=12 (IPv4_IncludingPort), address
        ss << "12" << '\t' << targetIPv4WithPort << '\t';
        // receiver equivalent
        ss << receiverEq;
        for (auto eq : exchangeEqs) {
            ss << '\t' << eq;
        }
        ss << '\n';
        return ss.str();
    }
}

TEST(InitiateMaxFlowExchangeCalculationApplyLogicTest, MaxFlowIs200ForGivenTopology)
{
    // Logger and IO
    Logger logger;
    boost::asio::io_context io;

    // Temporary storage handler (in-workspace path)
    std::string dbDir = "build-tests/testdb_applylogic";
    std::string dbName = "test.db";
    std::filesystem::remove_all(dbDir);
    StorageHandlerSQLite storage(dbDir, dbName, logger);

    // Keystore and events manager (minimal)
    crypto::Keystore keystore(logger);
    {
        auto ioTransaction = storage.beginTransaction();
        keystore.init(ioTransaction);
    }
    EventsInterfaceManager eventsManager({}, {}, logger);

    // Self address: A = 172.18.28.1:2000
    vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
    ContractorsManager contractors(ownAddrs, &storage, logger);

    // Router with no initial equivalents (will add below)
    vector<SerializedEquivalent> gateways; // none
    EquivalentsSubsystemsRouter router(
        &storage,
        &keystore,
        &contractors,
        &eventsManager,
        io,
        gateways,
        logger);

    // Add equivalents 1001 (sender’s) and 2002 (receiver’s)
    const SerializedEquivalent EQ_1001 = 1001;
    const SerializedEquivalent EQ_2002 = 2002;
    router.initNewEquivalent(EQ_1001);
    router.initNewEquivalent(EQ_2002);

    // Participants mapping from operations.log
    // contractorID=0 -> 172.18.28.1:2000 (A, self)
    // contractorID=1 -> 172.18.28.5:2000 (E)
    // contractorID=2 -> 172.18.28.2:2000 (B)
    // contractorID=3 -> 172.18.28.4:2000 (D)
    // contractorID=4 -> 172.18.28.3:2000 (C)
    auto addrA = make_shared<IPv4WithPortAddress>("172.18.28.1:2000");
    auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000");
    auto addrB = make_shared<IPv4WithPortAddress>("172.18.28.2:2000");
    auto addrD = make_shared<IPv4WithPortAddress>("172.18.28.4:2000");
    auto addrC = make_shared<IPv4WithPortAddress>("172.18.28.3:2000");

    // Ensure IDs are assigned as in the log
    // Self (A) is pre-inserted as 0 by router’s constructor
    ASSERT_EQ(router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress()), 0u);
    auto idE = router.getOrCreateParticipantID(addrE); // 1
    auto idB = router.getOrCreateParticipantID(addrB); // 2
    auto idD = router.getOrCreateParticipantID(addrD); // 3
    auto idC = router.getOrCreateParticipantID(addrC); // 4

    ASSERT_EQ(idE, 1u);
    ASSERT_EQ(idB, 2u);
    ASSERT_EQ(idD, 3u);
    ASSERT_EQ(idC, 4u);

    // Build topology (free amounts) per the test scenario
    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    // 1001: A(0)->B(2)=3000, B(2)->C(4)=2500, C(4)->D(3)=2000; (D->E=5000 optional)
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(0, idB, A(3000)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(2500)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(2000)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(5000)));

    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    // 2002: B(2)->C(4)=250, C(4)->D(3)=200, D(3)->E(1)=500
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(250)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(200)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(500)));

    // Exchange rate at node C (contractorID=4): 1001 -> 2002 with rate 0.5
    ExchangeRatesManager ratesMgr(io, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ_1001, EQ_2002, TrustLineAmount(5), /*shift*/ -1, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    ratesMgr.addOrUpdateExternal(idC, rate);

    // Tail manager (not used directly in this test’s path)
    TailManager tail(io, logger);

    // Build command: contractor E, receiver equivalent 2002, exchange equivalents {1001}
    CommandUUID cmdUUID = boost::uuids::random_generator()();
    std::string cmdStr = buildCommandStr("172.18.28.5:2000", EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    // Create the transaction
    InitiateMaxFlowExchangeCalculationTransaction tx(
        command,
        &contractors,
        &router,
        &ratesMgr,
        &tail,
        logger,
        /*hopsCount*/ 6);

    // First run() sets up contractor IDs (sendRequestForCollectingTopology)
    auto r1 = tx.run();
    ASSERT_TRUE(r1 != nullptr);

    // Second run() processes and enters CustomLogic, returning final result
    auto r2 = tx.run();
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    // Serialized output contains max flow amounts string
    auto serialized = r2->commandResult()->serialize();
    // Expect it to contain address tuple with amount 200:
    // Format: <UUID>\t200\t<count>[\t<addrType>\t<addr>\t<amount>...]\n
    std::string expectTuple = std::string("\t12\t") + "172.18.28.5:2000" + "\t200";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos) << serialized;
}
