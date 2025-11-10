#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define private public
#define protected public
#include "../../../src/core/transactions/transactions/regular/payments/CoordinatorExchangePaymentTransaction.h"
#undef private
#undef protected

#include "TestCommandBuilder.h"
#include "../../../src/core/contractors/Contractor.h"
#include "../../../src/core/contractors/ContractorsManager.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/crypto/keychain.h"
#include "../../../src/core/equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../src/core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/paths/ExchangePathsManager.h"
#include "../../../src/core/rates/manager/ExchangeRatesManager.h"
#include "../../../src/core/resources/manager/ResourcesManager.h"
#include "../../../src/core/subsystems_controller/SubsystemsController.h"
#include "../../../src/core/topology/TopologyTrustLine.h"
#include "../../../src/core/topology/manager/TopologyTrustLinesManager.h"
#include "../../../src/core/trust_lines/TrustLine.h"
#include "../../../src/core/transactions/transactions/regular/payments/base/PathReservation.h"

#include <boost/asio/io_context.hpp>

using namespace std;

namespace {

constexpr SerializedEquivalent kTestEquivalent = 7101;

inline ConstSharedTrustLineAmount makeAmount(const TrustLineAmount value)
{
    return make_shared<TrustLineAmount>(value);
}

struct PathRebuildTestEnv {
    Logger logger;
    boost::asio::io_context io;
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

    PathRebuildTestEnv(const string &testName)
    {
        dbDir = string("build-tests/testdb_path_rebuild_") + testName;
        std::filesystem::remove_all(dbDir);

        storage = make_unique<StorageHandlerSQLite>(dbDir, "test.db", logger);
        keystore = make_unique<crypto::Keystore>(logger);
        {
            auto ioTransaction = storage->beginTransaction();
            keystore->init(ioTransaction);
        }

        eventsManager = make_unique<EventsInterfaceManager>(
            vector<pair<string, SerializedEventType>>{},
            vector<pair<string, bool>>{},
            logger);

        vector<pair<string, string>> ownAddresses = {{"ipv4", "127.0.0.1:2000"}};
        contractors = make_unique<ContractorsManager>(ownAddresses, storage.get(), logger);

        vector<SerializedEquivalent> gateways;
        router = make_unique<EquivalentsSubsystemsRouter>(
            storage.get(),
            keystore.get(),
            contractors.get(),
            eventsManager.get(),
            io,
            gateways,
            logger);
        router->initNewEquivalent(kTestEquivalent);

        ratesManager = make_unique<ExchangeRatesManager>(io, logger);
        pathsManager = make_unique<ExchangePathsManager>(
            io,
            router.get(),
            ratesManager.get(),
            contractors.get(),
            logger);

        resourcesManager = make_unique<ResourcesManager>();
        subsystemsController = make_unique<SubsystemsController>(logger);
    }

    ~PathRebuildTestEnv()
    {
        std::filesystem::remove_all(dbDir);
    }
};

class CoordinatorExchangePaymentPathRebuildingTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        env = make_unique<PathRebuildTestEnv>(testInfo->name());

        contractorAddress = make_shared<IPv4WithPortAddress>("172.18.10.5:5005");
        command = TestCommandBuilder::buildExchangeCommand(
            {contractorAddress},
            TrustLineAmount(1000),
            kTestEquivalent,
            vector<SerializedEquivalent>{kTestEquivalent});

        transaction = make_unique<CoordinatorExchangePaymentTransaction>(
            command,
            env->contractors.get(),
            env->router.get(),
            env->storage.get(),
            env->resourcesManager.get(),
            env->pathsManager.get(),
            env->ratesManager.get(),
            env->keystore.get(),
            true,
            env->eventsManager.get(),
            env->logger,
            env->subsystemsController.get());
    }

    void TearDown() override
    {
        transaction.reset();
        env.reset();
    }

    ContractorID registerNode(const BaseAddress::Shared &address)
    {
        if (!address) {
            throw std::invalid_argument("registerNode requires non-null address");
        }

        // Coordinator logic expects every participant to exist both in the router and
        // in ContractorsManager. Production code keeps them in sync, but the unit test
        // fixture constructs topology nodes manually, so we need to simulate that sync.
        auto contractorID = env->contractors->contractorIDByAddress(address);
        if (contractorID == ContractorsManager::kNotFoundContractorID) {
            auto ioTransaction = env->storage->beginTransaction();
            env->contractors->createContractor(
                ioTransaction,
                vector<BaseAddress::Shared>{address});
        }

        return env->router->getOrCreateParticipantID(address);
    }

    TopologyTrustLinesManager* topologyManager(
        const SerializedEquivalent equivalent = kTestEquivalent)
    {
        return env->router->topologyTrustLineManager(equivalent);
    }

    void addTrustLine(
        const SerializedEquivalent equivalent,
        const BaseAddress::Shared &source,
        const BaseAddress::Shared &target,
        const TrustLineAmount amount)
    {
        const auto sourceID = registerNode(source);
        const auto targetID = registerNode(target);
        addTopologyEdge(equivalent, sourceID, targetID, amount);
    }

    void addTopologyEdge(
        const SerializedEquivalent equivalent,
        ContractorID source,
        ContractorID target,
        const TrustLineAmount amount)
    {
        auto manager = env->router->topologyTrustLineManager(equivalent);
        manager->addTrustLine(make_shared<TopologyTrustLine>(source, target, makeAmount(amount)));
    }

    OptimalPathResult buildLinearPath(
        const vector<ContractorID> &ids,
        const vector<BaseAddress::Shared> &nodes,
        const TrustLineAmount amount,
        bool markReserved) const
    {
        OptimalPathResult result;
        result.optimal_flow = amount;
        result.received_amount = amount;
        result.mMaxPathFlow = amount;
        result.mMaxPathReceivedAmount = amount;
        result.mIsValid = true;
        result.mPath.ids = ids;
        result.mPath.nodes = nodes;
        result.mPath.equivalents.assign(ids.size(), kTestEquivalent);

        const size_t intermediates = nodes.size() >= 2 ? nodes.size() - 2 : 0;
        result.mIntermediateNodesStates.assign(
            intermediates,
            markReserved ? OptimalPathResult::ReservationApproved
                         : OptimalPathResult::ReservationRequestDoesntSent);
        return result;
    }

    PaymentNodeID registerPaymentParticipant(const BaseAddress::Shared &address)
    {
        const auto paymentNodeID = mNextPaymentNodeId++;
        transaction->mPaymentNodesIds[address->fullAddress()] = paymentNodeID;
        transaction->mPaymentParticipants[paymentNodeID] = make_shared<Contractor>(
            vector<BaseAddress::Shared>{address});
        return paymentNodeID;
    }

    void attachIncomingReservation(
        const BaseAddress::Shared &address,
        const PathID pathID,
        const TrustLineAmount amount,
        const SerializedEquivalent equivalent = kTestEquivalent)
    {
        registerPaymentParticipant(address);
        PathReservation reservation(pathID, makeAmount(amount), equivalent, PathReservation::Incoming);
        transaction->mNodesFinalAmountsConfiguration[address->fullAddress()].push_back(reservation);
    }

    void seedPathStats(
        const PathID pathID,
        const vector<ContractorID> &ids,
        const vector<BaseAddress::Shared> &nodes,
        const TrustLineAmount amount,
        bool markReserved = false,
        bool valid = true,
        bool enqueue = false)
    {
        transaction->mPathsStats[pathID] = make_unique<OptimalPathResult>(
            buildLinearPath(ids, nodes, amount, markReserved));
        if (!valid) {
            transaction->mPathsStats[pathID]->setUnusable();
        }
        if (enqueue) {
            transaction->mPathIDs.push_back(pathID);
        }
    }

    TopologyTrustLineWithPtr* findEdge(
        const SerializedEquivalent equivalent,
        const ContractorID source,
        const ContractorID target)
    {
        auto edges = topologyManager(equivalent)->trustLinePtrsSet(source);
        for (auto edge : edges) {
            if (edge != nullptr && edge->topologyTrustLine()->targetID() == target) {
                return edge;
            }
        }
        return nullptr;
    }

    static set<PathID> snapshotPathIDs(
        const map<PathID, unique_ptr<OptimalPathResult>> &paths)
    {
        set<PathID> snapshot;
        for (const auto &entry : paths) {
            snapshot.insert(entry.first);
        }
        return snapshot;
    }

    vector<PathID> collectNewPathIDs(const set<PathID> &before) const
    {
        vector<PathID> newIDs;
        for (const auto &entry : transaction->mPathsStats) {
            if (before.find(entry.first) == before.end()) {
                newIDs.push_back(entry.first);
            }
        }
        return newIDs;
    }

    void openTrustLine(
        const ContractorID nodeID,
        const TrustLineAmount &outgoingAmount)
    {
        auto trustLines = env->router->trustLinesManager(kTestEquivalent);
        if (!trustLines->trustLineIsPresent(nodeID)) {
            auto ioTransaction = env->storage->beginTransaction();
            trustLines->open(nodeID, ioTransaction);
        }
        trustLines->setOutgoing(nodeID, outgoingAmount);
        trustLines->setTrustLineState(nodeID, TrustLine::Active);
        trustLines->setIsOwnKeysPresent(nodeID, true);
        trustLines->setIsContractorKeysPresent(nodeID, true);
    }

    unique_ptr<PathRebuildTestEnv> env;
    BaseAddress::Shared contractorAddress;
    CreditUsageExchangeCommand::Shared command;
    unique_ptr<CoordinatorExchangePaymentTransaction> transaction;
    PaymentNodeID mNextPaymentNodeId = 1000;
};

} // namespace

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, removesOutgoingEdgesForInaccessibleNodes) {
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.0.2:10002");
    auto neighborNode = make_shared<IPv4WithPortAddress>("10.0.0.3:10003");

    const auto inaccessibleID = registerNode(inaccessibleNode);
    const auto neighborID = registerNode(neighborNode);

    addTopologyEdge(kTestEquivalent, inaccessibleID, neighborID, 400);

    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    EXPECT_TRUE(topologyManager()->trustLinePtrsSet(inaccessibleID).empty());
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, removesIncomingEdgesForInaccessibleNodes) {
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.0.4:10004");
    auto spectatorNode = make_shared<IPv4WithPortAddress>("10.0.0.5:10005");

    const auto inaccessibleID = registerNode(inaccessibleNode);
    const auto spectatorID = registerNode(spectatorNode);

    addTopologyEdge(kTestEquivalent, spectatorID, inaccessibleID, 250);

    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    EXPECT_EQ(findEdge(kTestEquivalent, spectatorID, inaccessibleID), nullptr);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, removesRejectedTrustLinesAcrossEquivalents) {
    auto sourceAddress = make_shared<IPv4WithPortAddress>("10.0.0.5:10005");
    auto targetAddress = make_shared<IPv4WithPortAddress>("10.0.0.6:10006");

    const auto sourceID = registerNode(sourceAddress);
    const auto targetID = registerNode(targetAddress);

    addTopologyEdge(kTestEquivalent, sourceID, targetID, 500);
    const auto alternateEquivalent = kTestEquivalent + 1;
    env->router->initNewEquivalent(alternateEquivalent);
    transaction->mCommand->mExchangeEquivalents.push_back(alternateEquivalent);
    addTopologyEdge(alternateEquivalent, sourceID, targetID, 700);

    transaction->mRejectedTrustLines = {{sourceAddress, targetAddress}};
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    EXPECT_EQ(findEdge(kTestEquivalent, sourceID, targetID), nullptr);
    EXPECT_EQ(findEdge(alternateEquivalent, sourceID, targetID), nullptr);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, handlesCombinedInaccessibleNodesAndRejectedTrustLines) {
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.0.7:10007");
    auto unaffectedNode = make_shared<IPv4WithPortAddress>("10.0.0.8:10008");
    auto safeNode = make_shared<IPv4WithPortAddress>("10.0.0.9:10009");
    auto targetNode = make_shared<IPv4WithPortAddress>("10.0.0.10:10010");

    const auto inaccessibleID = registerNode(inaccessibleNode);
    const auto unaffectedID = registerNode(unaffectedNode);
    const auto safeID = registerNode(safeNode);
    const auto targetID = registerNode(targetNode);

    addTopologyEdge(kTestEquivalent, inaccessibleID, targetID, 400);
    addTopologyEdge(kTestEquivalent, safeID, inaccessibleID, 200);
    addTopologyEdge(kTestEquivalent, safeID, targetID, 600);
    addTopologyEdge(kTestEquivalent, unaffectedID, safeID, 350);

    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mRejectedTrustLines = {{unaffectedNode, safeNode}};
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    EXPECT_TRUE(topologyManager()->trustLinePtrsSet(inaccessibleID).empty());
    EXPECT_EQ(findEdge(kTestEquivalent, unaffectedID, safeID), nullptr);
    EXPECT_NE(findEdge(kTestEquivalent, safeID, targetID), nullptr);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, appliesPartialIncomingReservationsToTopology) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto previousHop = make_shared<IPv4WithPortAddress>("10.0.0.7:10007");
    auto targetHop = make_shared<IPv4WithPortAddress>("10.0.0.8:10008");

    const auto prevID = registerNode(previousHop);
    const auto targetID = registerNode(targetHop);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(kTestEquivalent, prevID, targetID, 1000);

    const PathID pathID = PathID(11);
    vector<BaseAddress::Shared> nodes = {coordinatorNode, previousHop, targetHop, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        prevID,
        targetID,
        contractorID};

    transaction->mPathsStats[pathID] = make_unique<OptimalPathResult>(
        buildLinearPath(ids, nodes, TrustLineAmount(900), false));

    const PaymentNodeID targetPaymentID = 100;
    transaction->mPaymentNodesIds[targetHop->fullAddress()] = targetPaymentID;
    transaction->mPaymentParticipants[targetPaymentID] = make_shared<Contractor>(
        vector<BaseAddress::Shared>{targetHop});

    const TrustLineAmount reservedChunk = 400;
    PathReservation reservation(
        pathID,
        makeAmount(reservedChunk),
        kTestEquivalent,
        PathReservation::Incoming);
    transaction->mNodesFinalAmountsConfiguration[targetHop->fullAddress()] = {reservation};
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    auto manager = env->router->topologyTrustLineManager(kTestEquivalent);
    auto outgoing = manager->trustLinePtrsSet(prevID);
    ASSERT_EQ(outgoing.size(), 1);
    auto edge = *outgoing.begin();
    auto remaining = edge->topologyTrustLine()->freeAmount();
    EXPECT_EQ(*remaining, TrustLineAmount(600));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, saturatesHopWhenReservationEqualsCapacity) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto previousHop = make_shared<IPv4WithPortAddress>("10.0.0.9:10009");
    auto targetHop = make_shared<IPv4WithPortAddress>("10.0.0.10:10010");

    const auto prevID = registerNode(previousHop);
    const auto targetID = registerNode(targetHop);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(kTestEquivalent, prevID, targetID, 300);

    const PathID pathID = PathID(12);
    vector<BaseAddress::Shared> nodes = {coordinatorNode, previousHop, targetHop, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        prevID,
        targetID,
        contractorID};

    transaction->mPathsStats[pathID] = make_unique<OptimalPathResult>(
        buildLinearPath(ids, nodes, TrustLineAmount(300), false));

    const PaymentNodeID targetPaymentID = 200;
    transaction->mPaymentNodesIds[targetHop->fullAddress()] = targetPaymentID;
    transaction->mPaymentParticipants[targetPaymentID] = make_shared<Contractor>(
        vector<BaseAddress::Shared>{targetHop});

    PathReservation reservation(
        pathID,
        makeAmount(300),
        kTestEquivalent,
        PathReservation::Incoming);
    transaction->mNodesFinalAmountsConfiguration[targetHop->fullAddress()] = {reservation};
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    auto manager = env->router->topologyTrustLineManager(kTestEquivalent);
    auto outgoing = manager->trustLinePtrsSet(prevID);
    ASSERT_EQ(outgoing.size(), 1);
    auto edge = *outgoing.begin();
    auto remaining = edge->topologyTrustLine()->freeAmount();
    EXPECT_EQ(*remaining, TrustLineAmount(0));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, ignoresOutgoingReservationsAndAvoidsDuplicates) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto prevA = make_shared<IPv4WithPortAddress>("10.0.0.11:10011");
    auto hopA = make_shared<IPv4WithPortAddress>("10.0.0.12:10012");
    auto prevB = make_shared<IPv4WithPortAddress>("10.0.0.13:10013");
    auto hopB = make_shared<IPv4WithPortAddress>("10.0.0.14:10014");

    const auto prevAId = registerNode(prevA);
    const auto hopAId = registerNode(hopA);
    const auto prevBId = registerNode(prevB);
    const auto hopBId = registerNode(hopB);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        prevAId,
        1000);
    addTopologyEdge(kTestEquivalent, prevAId, hopAId, 1000);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        prevBId,
        1000);
    addTopologyEdge(kTestEquivalent, prevBId, hopBId, 1000);

    const PathID pathA = 21;
    const PathID pathB = 22;
    vector<BaseAddress::Shared> nodesTemplate = {
        coordinatorNode,
        prevA,
        hopA,
        contractorAddress};
    vector<ContractorID> idsTemplate = {
        TopologyTrustLinesManager::kCurrentNodeID,
        prevAId,
        hopAId,
        contractorID};
    seedPathStats(pathA, idsTemplate, nodesTemplate, TrustLineAmount(900));

    nodesTemplate[1] = prevB;
    nodesTemplate[2] = hopB;
    idsTemplate[1] = prevBId;
    idsTemplate[2] = hopBId;
    seedPathStats(pathB, idsTemplate, nodesTemplate, TrustLineAmount(900));

    attachIncomingReservation(hopA, pathA, TrustLineAmount(100));
    PathReservation outgoing(pathA, makeAmount(50), kTestEquivalent, PathReservation::Outgoing);
    transaction->mNodesFinalAmountsConfiguration[hopA->fullAddress()].push_back(outgoing);

    attachIncomingReservation(hopB, pathB, TrustLineAmount(200));
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    auto edgeA = findEdge(kTestEquivalent, prevAId, hopAId);
    ASSERT_NE(edgeA, nullptr);
    EXPECT_EQ(*edgeA->topologyTrustLine()->freeAmount(), TrustLineAmount(900));

    auto edgeB = findEdge(kTestEquivalent, prevBId, hopBId);
    ASSERT_NE(edgeB, nullptr);
    EXPECT_EQ(*edgeB->topologyTrustLine()->freeAmount(), TrustLineAmount(800));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, skipsReservationsWhenPreviousHopMissing) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto firstHop = make_shared<IPv4WithPortAddress>("10.0.0.15:10015");

    const auto firstHopID = registerNode(firstHop);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        firstHopID,
        500);
    addTopologyEdge(kTestEquivalent, firstHopID, contractorID, 500);

    const PathID pathID = 30;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, firstHop, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        firstHopID,
        contractorID};
    seedPathStats(pathID, ids, nodes, TrustLineAmount(500));

    attachIncomingReservation(coordinatorNode, pathID, TrustLineAmount(200));
    transaction->mExchangePathsManager = nullptr;

    transaction->buildPathsAgain();

    auto edge = findEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        firstHopID);
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(*edge->topologyTrustLine()->freeAmount(), TrustLineAmount(500));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, rebuildsPathsAfterExcludingInaccessibleNode) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.1.1:11001");
    auto alternativeNode = make_shared<IPv4WithPortAddress>("10.0.1.2:11002");

    const auto badID = registerNode(inaccessibleNode);
    const auto altID = registerNode(alternativeNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        600);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 600);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        altID,
        800);
    addTopologyEdge(kTestEquivalent, altID, contractorID, 800);

    const PathID existingPath = 40;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(existingPath, ids, nodes, TrustLineAmount(500));

    transaction->mInaccessibleNodes = {inaccessibleNode};

    auto before = snapshotPathIDs(transaction->mPathsStats);
    transaction->buildPathsAgain();

    auto newIDs = collectNewPathIDs(before);
    ASSERT_FALSE(newIDs.empty());
    auto *rebuilt = transaction->mPathsStats[newIDs.front()].get();
    ASSERT_NE(rebuilt, nullptr);

    bool containsBad = false;
    bool containsAlternative = false;
    for (const auto &node : rebuilt->path().nodes) {
        if (node && node->fullAddress() == inaccessibleNode->fullAddress()) {
            containsBad = true;
        }
        if (node && node->fullAddress() == alternativeNode->fullAddress()) {
            containsAlternative = true;
        }
    }
    EXPECT_FALSE(containsBad);
    EXPECT_TRUE(containsAlternative);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, rebuildsPathsAfterExcludingRejectedTrustLine) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto rejectedHop = make_shared<IPv4WithPortAddress>("10.0.1.3:11003");
    auto alternativeHop = make_shared<IPv4WithPortAddress>("10.0.1.4:11004");

    const auto rejectedID = registerNode(rejectedHop);
    const auto alternativeID = registerNode(alternativeHop);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        rejectedID,
        500);
    addTopologyEdge(kTestEquivalent, rejectedID, contractorID, 500);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        alternativeID,
        900);
    addTopologyEdge(kTestEquivalent, alternativeID, contractorID, 900);

    const PathID existingPath = 41;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, rejectedHop, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        rejectedID,
        contractorID};
    seedPathStats(existingPath, ids, nodes, TrustLineAmount(400));

    transaction->mRejectedTrustLines = {{rejectedHop, contractorAddress}};

    auto before = snapshotPathIDs(transaction->mPathsStats);
    transaction->buildPathsAgain();

    auto newIDs = collectNewPathIDs(before);
    ASSERT_FALSE(newIDs.empty());
    auto *rebuilt = transaction->mPathsStats[newIDs.front()].get();
    ASSERT_NE(rebuilt, nullptr);

    bool containsRejected = false;
    bool containsAlternative = false;
    for (const auto &node : rebuilt->path().nodes) {
        if (node && node->fullAddress() == rejectedHop->fullAddress()) {
            containsRejected = true;
        }
        if (node && node->fullAddress() == alternativeHop->fullAddress()) {
            containsAlternative = true;
        }
    }
    EXPECT_FALSE(containsRejected);
    EXPECT_TRUE(containsAlternative);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, rebuildsNothingWhenAlternativesAreMissing) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.1.5:11005");

    const auto badID = registerNode(inaccessibleNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        300);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 300);

    const PathID existingPath = 42;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(existingPath, ids, nodes, TrustLineAmount(300));

    transaction->mInaccessibleNodes = {inaccessibleNode};

    const auto beforeSize = transaction->mPathsStats.size();
    transaction->buildPathsAgain();

    EXPECT_EQ(transaction->mPathsStats.size(), beforeSize);
    EXPECT_NE(transaction->mPathsStats.find(existingPath), transaction->mPathsStats.end());
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, appliesIncomingReservationsBeforeRunningMaxFlow) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto intermediateNode = make_shared<IPv4WithPortAddress>("10.0.1.6:11006");

    const auto intermediateID = registerNode(intermediateNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        intermediateID,
        1000);
    addTopologyEdge(kTestEquivalent, intermediateID, contractorID, 1000);

    const PathID pathID = 43;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, intermediateNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        intermediateID,
        contractorID};
    seedPathStats(pathID, ids, nodes, TrustLineAmount(1000));

    attachIncomingReservation(contractorAddress, pathID, TrustLineAmount(300));

    auto before = snapshotPathIDs(transaction->mPathsStats);
    transaction->buildPathsAgain();

    auto newIDs = collectNewPathIDs(before);
    ASSERT_FALSE(newIDs.empty());
    auto *rebuilt = transaction->mPathsStats[newIDs.front()].get();
    ASSERT_NE(rebuilt, nullptr);
    EXPECT_EQ(rebuilt->received_amount, TrustLineAmount(700));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, capacityHelperAggregatesOnlyEligiblePaths) {
    transaction->mPathsStats.clear();
    transaction->mPathIDs.clear();

    const PathID processedPath = 1;
    const PathID futureValidPath = 2;
    const PathID invalidPath = 3;
    const PathID processedFuturePath = 4;

    auto intermediate = make_shared<IPv4WithPortAddress>("10.0.0.16:10016");
    const auto intermediateID = registerNode(intermediate);
    vector<BaseAddress::Shared> nodes = {
        env->contractors->selfContractor()->mainAddress(),
        intermediate,
        contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        intermediateID,
        transaction->mContractorID};

    transaction->mPathsStats[processedPath] = make_unique<OptimalPathResult>(
        buildLinearPath(ids, nodes, TrustLineAmount(300), true));

    auto future = buildLinearPath(ids, nodes, TrustLineAmount(450), false);
    transaction->mPathsStats[futureValidPath] = make_unique<OptimalPathResult>(future);

    auto invalid = buildLinearPath(ids, nodes, TrustLineAmount(500), false);
    invalid.setUnusable();
    transaction->mPathsStats[invalidPath] = make_unique<OptimalPathResult>(invalid);

    auto processed = buildLinearPath(ids, nodes, TrustLineAmount(600), true);
    transaction->mPathsStats[processedFuturePath] = make_unique<OptimalPathResult>(processed);

    transaction->mCurrentAmountReservingPathIdentifier = processedPath;
    transaction->mAmount = TrustLineAmount(1200);

    const auto totalCapacity = transaction->calculateTotalPathCapacityForReceive();
    EXPECT_EQ(totalCapacity, TrustLineAmount(450));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, capacityHelperReturnsZeroWhenTargetSatisfied) {
    transaction->mPathsStats.clear();
    transaction->mPathIDs.clear();

    auto intermediate = make_shared<IPv4WithPortAddress>("10.0.0.17:10017");
    const auto intermediateID = registerNode(intermediate);
    vector<BaseAddress::Shared> nodes = {
        env->contractors->selfContractor()->mainAddress(),
        intermediate,
        contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        intermediateID,
        transaction->mContractorID};

    const PathID pathID = 15;
    transaction->mPathsStats[pathID] = make_unique<OptimalPathResult>(
        buildLinearPath(ids, nodes, TrustLineAmount(500), true));

    transaction->mCurrentAmountReservingPathIdentifier = pathID;
    transaction->mAmount = TrustLineAmount(500);

    EXPECT_EQ(transaction->calculateTotalPathCapacityForReceive(), TrustLineAmount(0));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, capacityHelperIgnoresInvalidPaths) {
    transaction->mPathsStats.clear();

    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto intermediate = make_shared<IPv4WithPortAddress>("10.0.0.18:10018");
    const auto intermediateID = registerNode(intermediate);
    const auto contractorID = transaction->mContractorID;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, intermediate, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        intermediateID,
        contractorID};

    const PathID invalidPath = 50;
    seedPathStats(invalidPath, ids, nodes, TrustLineAmount(300), false, false);

    const PathID validPath = 51;
    seedPathStats(validPath, ids, nodes, TrustLineAmount(400));

    transaction->mCurrentAmountReservingPathIdentifier = 0;
    transaction->mAmount = TrustLineAmount(1000);

    EXPECT_EQ(transaction->calculateTotalPathCapacityForReceive(), TrustLineAmount(400));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, capacityHelperIgnoresPathsWithProcessedLastNode) {
    transaction->mPathsStats.clear();

    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    const auto contractorID = transaction->mContractorID;
    auto midNode = make_shared<IPv4WithPortAddress>("10.0.1.7:11007");
    const auto midID = registerNode(midNode);

    vector<BaseAddress::Shared> nodes = {coordinatorNode, midNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        midID,
        contractorID};

    const PathID processedPath = 60;
    seedPathStats(processedPath, ids, nodes, TrustLineAmount(500), true);

    const PathID pendingPath = 61;
    seedPathStats(pendingPath, ids, nodes, TrustLineAmount(650));

    transaction->mCurrentAmountReservingPathIdentifier = 0;
    transaction->mAmount = TrustLineAmount(1000);

    EXPECT_EQ(transaction->calculateTotalPathCapacityForReceive(), TrustLineAmount(650));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, capacityHelperReturnsZeroWhenNoPathsPresent) {
    transaction->mPathsStats.clear();
    transaction->mPathIDs.clear();
    transaction->mCurrentAmountReservingPathIdentifier = 0;
    transaction->mAmount = TrustLineAmount(1000);

    EXPECT_EQ(transaction->calculateTotalPathCapacityForReceive(), TrustLineAmount(0));
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, tryProcessNextPathRejectsWhenRebuiltCapacityIsTooSmall) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.2.1:12001");
    auto alternativeNode = make_shared<IPv4WithPortAddress>("10.0.2.2:12002");

    const auto badID = registerNode(inaccessibleNode);
    const auto altID = registerNode(alternativeNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        200);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 200);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        altID,
        150);
    addTopologyEdge(kTestEquivalent, altID, contractorID, 150);

    const PathID processedPath = 70;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(processedPath, ids, nodes, TrustLineAmount(200), true);

    transaction->mCurrentAmountReservingPathIdentifier = processedPath;
    transaction->mPathIDs.clear();
    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mAmount = TrustLineAmount(500);
    transaction->mPreviousInaccessibleNodesCount = 0;
    transaction->mPreviousRejectedTrustLinesCount = 0;
    transaction->mDirectPathIsAlreadyProcessed = true;

    auto result = transaction->tryProcessNextPath();

    ASSERT_NE(result, nullptr);
    ASSERT_NE(result->commandResult(), nullptr);
    EXPECT_EQ(result->commandResult()->resultCode(), 412);
    EXPECT_EQ(transaction->mPreviousInaccessibleNodesCount, 0u);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, tryProcessNextPathContinuesWhenCapacityIsSufficient) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.2.3:12003");
    auto alternativeNode = make_shared<IPv4WithPortAddress>("10.0.2.4:12004");

    const auto badID = registerNode(inaccessibleNode);
    const auto altID = registerNode(alternativeNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        200);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 200);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        altID,
        900);
    addTopologyEdge(kTestEquivalent, altID, contractorID, 900);

    const PathID processedPath = 71;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(processedPath, ids, nodes, TrustLineAmount(200), true);

    transaction->mCurrentAmountReservingPathIdentifier = processedPath;
    transaction->mPathIDs.clear();
    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mAmount = TrustLineAmount(400);
    transaction->mPreviousInaccessibleNodesCount = 0;
    transaction->mDirectPathIsAlreadyProcessed = true;

    transaction->mTestShortCircuitAfterCapacityValidation = true;
    auto before = snapshotPathIDs(transaction->mPathsStats);

    auto result = transaction->tryProcessNextPath();

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->commandResult() == nullptr || result->commandResult()->resultCode() != 412);
    EXPECT_EQ(transaction->mPreviousInaccessibleNodesCount, transaction->mInaccessibleNodes.size());
    EXPECT_EQ(transaction->mCurrentAmountReservingPathIdentifier, processedPath);
    EXPECT_FALSE(transaction->mDirectPathIsAlreadyProcessed);

    auto newIDs = collectNewPathIDs(before);
    ASSERT_FALSE(newIDs.empty());
    EXPECT_EQ(transaction->mPathIDs.size(), newIDs.size());
    EXPECT_EQ(*transaction->mPathIDs.cbegin(), newIDs.front());
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, tryProcessNextPathHandlesExactCapacityMatches) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.2.5:12005");
    auto alternativeNode = make_shared<IPv4WithPortAddress>("10.0.2.6:12006");

    const auto badID = registerNode(inaccessibleNode);
    const auto altID = registerNode(alternativeNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        200);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 200);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        altID,
        600);
    addTopologyEdge(kTestEquivalent, altID, contractorID, 600);

    const PathID processedPath = 72;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(processedPath, ids, nodes, TrustLineAmount(200), true);

    transaction->mCurrentAmountReservingPathIdentifier = processedPath;
    transaction->mPathIDs.clear();
    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mAmount = TrustLineAmount(600);
    transaction->mPreviousInaccessibleNodesCount = 0;

    transaction->mTestShortCircuitAfterCapacityValidation = true;
    auto before = snapshotPathIDs(transaction->mPathsStats);

    auto result = transaction->tryProcessNextPath();

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->commandResult() == nullptr || result->commandResult()->resultCode() != 412);
    auto newIDs = collectNewPathIDs(before);
    ASSERT_FALSE(newIDs.empty());
    ASSERT_FALSE(transaction->mPathIDs.empty());
    EXPECT_EQ(transaction->mCurrentAmountReservingPathIdentifier, processedPath);
    EXPECT_EQ(*transaction->mPathIDs.cbegin(), newIDs.front());
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, endToEndRebuildContinuesAfterInaccessibleNode) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.3.1:13001");
    auto alternativeNode = make_shared<IPv4WithPortAddress>("10.0.3.2:13002");

    const auto badID = registerNode(inaccessibleNode);
    const auto altID = registerNode(alternativeNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        400);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 400);
    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        altID,
        700);
    addTopologyEdge(kTestEquivalent, altID, contractorID, 700);

    const PathID reservedPath = 80;
    vector<BaseAddress::Shared> directNodes = {coordinatorNode, contractorAddress};
    vector<ContractorID> directIds = {
        TopologyTrustLinesManager::kCurrentNodeID,
        contractorID};
    seedPathStats(reservedPath, directIds, directNodes, TrustLineAmount(300), true);

    const PathID processedPath = 81;
    vector<BaseAddress::Shared> currentNodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> currentIds = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(processedPath, currentIds, currentNodes, TrustLineAmount(200));

    transaction->mCurrentAmountReservingPathIdentifier = processedPath;
    transaction->mPathIDs.clear();
    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mAmount = TrustLineAmount(1000);
    transaction->mPreviousInaccessibleNodesCount = 0;
    transaction->mDirectPathIsAlreadyProcessed = true;

    transaction->mTestShortCircuitAfterCapacityValidation = true;
    auto before = snapshotPathIDs(transaction->mPathsStats);

    auto result = transaction->tryProcessNextPath();

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->commandResult() == nullptr || result->commandResult()->resultCode() != 412);
    EXPECT_EQ(transaction->mPreviousInaccessibleNodesCount, transaction->mInaccessibleNodes.size());
    EXPECT_EQ(transaction->mRebuildingAttemptsCount, 1u);
    EXPECT_FALSE(transaction->mDirectPathIsAlreadyProcessed);

    auto newIDs = collectNewPathIDs(before);
    ASSERT_FALSE(newIDs.empty());
    const auto newPathID = newIDs.front();
    auto *newPath = transaction->mPathsStats[newPathID].get();
    ASSERT_NE(newPath, nullptr);

    bool containsBad = false;
    bool containsAlternative = false;
    for (const auto &node : newPath->path().nodes) {
        if (node && node->fullAddress() == inaccessibleNode->fullAddress()) {
            containsBad = true;
        }
        if (node && node->fullAddress() == alternativeNode->fullAddress()) {
            containsAlternative = true;
        }
    }
    EXPECT_FALSE(containsBad);
    EXPECT_TRUE(containsAlternative);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, endToEndFailsWhenNoAlternativesExist) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.3.3:13003");

    const auto badID = registerNode(inaccessibleNode);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        500);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 500);

    const PathID processedPath = 82;
    vector<BaseAddress::Shared> nodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> ids = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(processedPath, ids, nodes, TrustLineAmount(500));

    transaction->mCurrentAmountReservingPathIdentifier = processedPath;
    transaction->mPathIDs.clear();
    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mAmount = TrustLineAmount(800);

    auto result = transaction->tryProcessNextPath();

    ASSERT_NE(result, nullptr);
    ASSERT_NE(result->commandResult(), nullptr);
    EXPECT_EQ(result->commandResult()->resultCode(), 412);
    EXPECT_EQ(transaction->mRebuildingAttemptsCount, 1u);
}

TEST_F(CoordinatorExchangePaymentPathRebuildingTest, endToEndSupportsSequentialRebuildAttempts) {
    auto coordinatorNode = env->contractors->selfContractor()->mainAddress();
    auto inaccessibleNode = make_shared<IPv4WithPortAddress>("10.0.3.4:13004");
    auto firstAlternative = make_shared<IPv4WithPortAddress>("10.0.3.5:13005");
    auto bridgeNode = make_shared<IPv4WithPortAddress>("10.0.3.6:13006");
    auto secondAlternative = make_shared<IPv4WithPortAddress>("10.0.3.7:13007");

    const auto badID = registerNode(inaccessibleNode);
    const auto firstAltID = registerNode(firstAlternative);
    const auto bridgeID = registerNode(bridgeNode);
    const auto secondAltID = registerNode(secondAlternative);
    const auto contractorID = transaction->mContractorID;

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        200);
    addTopologyEdge(kTestEquivalent, badID, contractorID, 200);

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        firstAltID,
        400);
    addTopologyEdge(kTestEquivalent, firstAltID, bridgeID, 400);
    addTopologyEdge(kTestEquivalent, bridgeID, contractorID, 400);

    addTopologyEdge(
        kTestEquivalent,
        TopologyTrustLinesManager::kCurrentNodeID,
        secondAltID,
        600);
    addTopologyEdge(kTestEquivalent, secondAltID, contractorID, 600);

    const PathID initialPath = 90;
    vector<BaseAddress::Shared> initialNodes = {coordinatorNode, inaccessibleNode, contractorAddress};
    vector<ContractorID> initialIds = {
        TopologyTrustLinesManager::kCurrentNodeID,
        badID,
        contractorID};
    seedPathStats(initialPath, initialIds, initialNodes, TrustLineAmount(200));

    transaction->mCurrentAmountReservingPathIdentifier = initialPath;
    transaction->mPathIDs.clear();
    transaction->mInaccessibleNodes = {inaccessibleNode};
    transaction->mAmount = TrustLineAmount(800);

    transaction->mTestShortCircuitAfterCapacityValidation = true;
    auto firstBefore = snapshotPathIDs(transaction->mPathsStats);

    auto firstResult = transaction->tryProcessNextPath();
    ASSERT_NE(firstResult, nullptr);
    ASSERT_TRUE(firstResult->commandResult() == nullptr || firstResult->commandResult()->resultCode() != 412);
    auto firstNewIDs = collectNewPathIDs(firstBefore);
    ASSERT_FALSE(firstNewIDs.empty());
    const auto firstRebuiltPathID = firstNewIDs.front();

    transaction->mRejectedTrustLines = {{firstAlternative, bridgeNode}};
    transaction->mPathIDs.clear();
    transaction->mCurrentAmountReservingPathIdentifier = firstRebuiltPathID;

    auto secondBefore = snapshotPathIDs(transaction->mPathsStats);
    auto secondResult = transaction->tryProcessNextPath();
    ASSERT_NE(secondResult, nullptr);
    ASSERT_TRUE(secondResult->commandResult() == nullptr || secondResult->commandResult()->resultCode() != 412);
    EXPECT_EQ(transaction->mRebuildingAttemptsCount, 2u);

    auto secondNewIDs = collectNewPathIDs(secondBefore);
    ASSERT_FALSE(secondNewIDs.empty());
    const auto finalPathID = secondNewIDs.back();
    auto *finalPath = transaction->mPathsStats[finalPathID].get();
    ASSERT_NE(finalPath, nullptr);

    bool containsBad = false;
    bool containsFirstAlternative = false;
    bool containsSecondAlternative = false;
    for (const auto &node : finalPath->path().nodes) {
        if (!node) {
            continue;
        }
        if (node->fullAddress() == inaccessibleNode->fullAddress()) {
            containsBad = true;
        }
        if (node->fullAddress() == firstAlternative->fullAddress() ||
            node->fullAddress() == bridgeNode->fullAddress()) {
            containsFirstAlternative = true;
        }
        if (node->fullAddress() == secondAlternative->fullAddress()) {
            containsSecondAlternative = true;
        }
    }

    EXPECT_FALSE(containsBad);
    EXPECT_FALSE(containsFirstAlternative);
    EXPECT_TRUE(containsSecondAlternative);
}
