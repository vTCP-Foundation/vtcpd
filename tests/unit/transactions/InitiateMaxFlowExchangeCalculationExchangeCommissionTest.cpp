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

// Test case for exchange node commission handling
// Topology: A->B(3000) B->C(2500) in eq 1001; C->D(200) D->E(500) in eq 2002
// Commission: C has commission in both eq 1001 and 2002 (exchange node)
// Exchange: C has rate 5, shift -2 (= 0.05) from eq 1001 to eq 2002
// Expected: 125 in eq 2002 (2500 * 0.05 = 125, no transit commissions for exchange node)

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

TEST(InitiateMaxFlowExchangeCalculationExchangeCommissionTest, ExchangeNodeCommissionsIgnored)
{
    // Setup
    Logger logger;
    boost::asio::io_context io;

    std::string dbDir = "build-tests/testdb_exchange_commission";
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
    auto addrC = make_shared<IPv4WithPortAddress>("172.18.28.3:2000"); // C (exchange node)
    auto addrD = make_shared<IPv4WithPortAddress>("172.18.28.4:2000"); // D
    auto addrE = make_shared<IPv4WithPortAddress>("172.18.28.5:2000"); // E (target)

    // Assign participant IDs
    auto idA = router.getOrCreateParticipantID(contractors.selfContractor()->mainAddress()); // 0
    auto idB = router.getOrCreateParticipantID(addrB); // 1
    auto idC = router.getOrCreateParticipantID(addrC); // 2
    auto idD = router.getOrCreateParticipantID(addrD); // 3  
    auto idE = router.getOrCreateParticipantID(addrE); // 4

    // Build topology in equivalent 1001: A->B(3000) B->C(2500)
    auto tlm1001 = router.topologyTrustLineManager(EQ_1001);
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idA, idB, A(3000)));
    tlm1001->addTrustLine(make_shared<TopologyTrustLine>(idB, idC, A(2500)));

    // Build topology in equivalent 2002: C->D(200) D->E(500)
    auto tlm2002 = router.topologyTrustLineManager(EQ_2002);
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idC, idD, A(200)));
    tlm2002->addTrustLine(make_shared<TopologyTrustLine>(idD, idE, A(500)));

    // Add commissions to C (exchange node) in both equivalents
    // These should NOT be applied as transit commissions since C is exchange node
    auto commissionC1001 = make_shared<Commission>(50);
    tlm1001->storeCommission(idC, EQ_1001, commissionC1001);
    
    auto commissionC2002 = make_shared<Commission>(25); 
    tlm2002->storeCommission(idC, EQ_2002, commissionC2002);

    // Exchange rate at node C: amount=5, shift=-2 (rate=0.05) from 1001->2002
    ExchangeRatesManager ratesMgr(io, logger);
    auto expiresAt = utc_now() + boost::posix_time::seconds(300);
    ExchangeRate rate(EQ_1001, EQ_2002, TrustLineAmount(5), /*shift*/ -2, expiresAt,
                      TrustLineAmount(0), TrustLineAmount(0));
    ratesMgr.addOrUpdateExternal(idC, rate);

    TailManager tail(io, logger);

    // Build command targeting E in equivalent 2002, exchanging from equivalent 1001
    CommandUUID cmdUUID = boost::uuids::random_generator()();
    std::string cmdStr = buildCommandStr("172.18.28.5:2000", EQ_2002, {EQ_1001});
    auto command = make_shared<InitiateMaxFlowExchangeCalculationCommand>(cmdUUID, cmdStr);

    // Create transaction
    InitiateMaxFlowExchangeCalculationTransaction tx(
        command,
        &contractors,
        &router,
        &ratesMgr,
        &tail,
        logger,
        /*hopsCount*/ 6);

    // Run the transaction
    auto r1 = tx.run(); // Setup phase
    ASSERT_TRUE(r1 != nullptr);

    auto r2 = tx.run(); // Apply custom logic
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_TRUE(r2->commandResult() != nullptr);

    // Check result
    auto serialized = r2->commandResult()->serialize();
    
    // Expected flow calculation:
    // - Max flow on B->C edge: 2500 in eq 1001
    // - After exchange: 2500 * 0.05 = 125 in eq 2002
    // - C is exchange node, so NO transit commissions applied
    // - Flow should not exceed edge capacity (2500)
    
    std::string expectTuple = std::string("\t12\t") + "172.18.28.5:2000" + "\t125";
    EXPECT_NE(serialized.find(expectTuple), std::string::npos) 
        << "Expected 125 but got: " << serialized;
        
    std::cout << "Test result: " << serialized << std::endl;
}
