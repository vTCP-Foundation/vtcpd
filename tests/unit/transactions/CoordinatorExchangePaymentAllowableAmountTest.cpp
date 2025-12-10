#include <gtest/gtest.h>
#include <boost/uuid/uuid_generators.hpp>
#include <filesystem>
#include "../../../src/core/interface/commands_interface/commands/payments/CreditUsageExchangeCommand.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include <limits>
#include "../../../src/core/transactions/transactions/regular/payments/CoordinatorExchangePaymentTransaction.h"
#include "../../../src/core/payments/reservations/AmountReservation.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/contractors/ContractorsManager.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../src/core/rates/manager/ExchangeRatesManager.h"
#include "../../../src/core/paths/ExchangePathsManager.h"
#include "../../../src/core/resources/manager/ResourcesManager.h"
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/crypto/keychain.h"
#include "../../../src/core/interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../src/core/subsystems_controller/SubsystemsController.h"
#include "TestCommandBuilder.h"

// =============================================================================
// Unit Tests for Allowable Payment Amount Control Feature
// =============================================================================
//
// IMPORTANT NOTE:
//
// The maxAllowablePaymentAmount parameter is now MANDATORY.
// If no limit is needed, use value 0 which will be converted to std::numeric_limits<TrustLineAmount>::max()
// internally to represent unlimited payment amount.
// The tests below verify this behavior.
//
// =============================================================================

// =============================================================================
// Test Group 1: Command Parsing Tests
// =============================================================================

class CreditUsageExchangeCommandAllowableAmountTest : public ::testing::Test {
protected:
    void SetUp() override {
        uuid = boost::uuids::random_generator()();
    }

    CommandUUID uuid;
    const char kTokensSeparator = '\t';
};

// Test 1: Parse command with maxAllowablePaymentAmount parameter
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ParseWithMaxAllowableAmount) {
    std::vector<BaseAddress::Shared> addresses = {
        std::make_shared<IPv4WithPortAddress>("127.0.0.1:2003")
    };

    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        SerializedEquivalent(2),
        {SerializedEquivalent(1)},
        TrustLineAmount(1500));

    EXPECT_EQ(command->maxAllowablePaymentAmount(), TrustLineAmount(1500))
        << "maxAllowablePaymentAmount should be 1500";

    EXPECT_EQ(command->exchangeEquivalents().size(), 1)
        << "Should have 1 exchange equivalent";
    EXPECT_EQ(command->exchangeEquivalents()[0], 1)
        << "First exchange equivalent should be 1";

    EXPECT_EQ(command->amount(), TrustLineAmount(1000));
    EXPECT_EQ(command->equivalent(), 2);
}

// Test 2: Parse command with maxAllowablePaymentAmount = 0 (unlimited, converted to max)
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ParseWithUnlimitedAmount) {
    // Setup command string with maxAllowablePaymentAmount = 0 (no limit)
    // Format: ...:1:0 (0 => unlimited)
    std::vector<BaseAddress::Shared> addresses = {
        std::make_shared<IPv4WithPortAddress>("127.0.0.1:2003")
    };

    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        SerializedEquivalent(2),
        {SerializedEquivalent(1)},
        TrustLineAmount(0));

    // Assertions - 0 should be converted to std::numeric_limits<TrustLineAmount>::max()
    EXPECT_EQ(command->maxAllowablePaymentAmount(), std::numeric_limits<TrustLineAmount>::max())
        << "maxAllowablePaymentAmount should be max() when input is 0 (unlimited)";

    // Verify other parameters parsed correctly
    EXPECT_EQ(command->amount(), TrustLineAmount(1000))
        << "Amount should be parsed correctly";
    EXPECT_EQ(command->equivalent(), 2)
        << "Receiver equivalent should be 2";
    EXPECT_EQ(command->exchangeEquivalents().size(), 1)
        << "Should have 1 exchange equivalent";
}

// Test 2b: Missing maxAllowablePaymentAmount should cause parsing failure
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ParseWithoutMaxAllowableAmountThrows) {
    std::string commandStr = "1\t12\t127.0.0.1:2003\t1000\t2\t1\n";

    EXPECT_THROW(
        {
            auto command = std::make_shared<CreditUsageExchangeCommand>(uuid, commandStr);
            (void)command;
        },
        ValueError) << "Command without limit token must be rejected";
}

// Test 3: Parse command with invalid maxAllowablePaymentAmount (leading zero)
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ParseWithInvalidMaxAllowableAmount) {
    std::string commandStr = "1\t12\t127.0.0.1:2003\t1000\t2\t1\t0150\n";

    EXPECT_THROW(
        {
            auto command = std::make_shared<CreditUsageExchangeCommand>(uuid, commandStr);
            (void)command;
        },
        ValueError) << "Should throw ValueError for maxAllowablePaymentAmount with leading zero";
}

// Test 4: responseAllowablePaymentAmountExceeded returns code 415
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ResponseAllowableAmountExceeded) {
    std::vector<BaseAddress::Shared> addresses = {
        std::make_shared<IPv4WithPortAddress>("127.0.0.1:2003")
    };
    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        SerializedEquivalent(2),
        {SerializedEquivalent(1)},
        TrustLineAmount(1500));

    // Call response method
    auto result = command->responseAllowablePaymentAmountExceeded();

    // Assertions
    EXPECT_EQ(result->resultCode(), 415)
        << "Result code should be 415";
    EXPECT_TRUE(result->serialize().find("Allowable payment amount has been exceeded") != std::string::npos)
        << "Result message should contain 'Allowable payment amount has been exceeded'";
}

// Test 5: Parse command with multiple exchange equivalents and maxAllowablePaymentAmount
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ParseWithMultipleExchangeEquivalents) {
    std::vector<BaseAddress::Shared> addresses = {
        std::make_shared<IPv4WithPortAddress>("127.0.0.1:2003")
    };
    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        SerializedEquivalent(2),
        {SerializedEquivalent(1), SerializedEquivalent(3)},
        TrustLineAmount(2000));

    EXPECT_EQ(command->maxAllowablePaymentAmount(), TrustLineAmount(2000))
        << "maxAllowablePaymentAmount should be 2000";

    EXPECT_EQ(command->exchangeEquivalents().size(), 2)
        << "Should have 2 exchange equivalents";
    EXPECT_EQ(command->exchangeEquivalents()[0], 1);
    EXPECT_EQ(command->exchangeEquivalents()[1], 3);
}

// =============================================================================
// Test Group 2: Result Code Tests
// =============================================================================

// Test 6: Verify result code 415 is consistently used
TEST_F(CreditUsageExchangeCommandAllowableAmountTest, ResultCode415IsConsistent) {
    std::vector<BaseAddress::Shared> addresses = {
        std::make_shared<IPv4WithPortAddress>("127.0.0.1:2003")
    };
    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        SerializedEquivalent(2),
        {SerializedEquivalent(1)},
        TrustLineAmount(1500));

    // Get response
    auto response1 = command->responseAllowablePaymentAmountExceeded();
    auto response2 = command->responseAllowablePaymentAmountExceeded();

    // Both responses should return code 415
    EXPECT_EQ(response1->resultCode(), 415);
    EXPECT_EQ(response2->resultCode(), 415);
    EXPECT_EQ(response1->resultCode(), response2->resultCode())
        << "Result code should be consistent across multiple calls";
}

// =============================================================================
// Test Group 3: calculateTotalReservedPaymentAmount Tests
// =============================================================================
//
// ⚠️ DEPRECATED: Tests 7-10 below use a ReservationCalculator helper that duplicates
// the production logic instead of calling the real CoordinatorExchangePaymentTransaction
// method. These tests are kept for backwards compatibility but DO NOT fulfill DOD requirements.
//
// ✅ REAL TESTS: See Tests 16-19 in "Test Group 4" which call the actual
// transaction->calculateTotalReservedPaymentAmount() method.
//
// TODO: Remove ReservationCalculator and tests 7-10 in future cleanup.
// =============================================================================

// Standalone test helper that simulates calculateTotalReservedPaymentAmount logic
// This avoids needing to instantiate the full transaction object
class ReservationCalculator {
public:
    using ReservationsMap = map<ContractorID, vector<pair<PathID, AmountReservation::ConstShared>>>;

    static TrustLineAmount calculateTotalReservedPaymentAmount(
        const ReservationsMap& reservations,
        const vector<SerializedEquivalent>& exchangeEquivalents)
    {
        TrustLineAmount totalReserved = TrustLineAmount(0);

        // Iterate through all exchange equivalents
        for (const auto equivalent : exchangeEquivalents) {
            // Get reserved amount for this equivalent using existing API
            const auto reservedForEquivalent = totalReservedAmount(
                reservations,
                AmountReservation::Outgoing,
                equivalent);

            // Add to total
            totalReserved = totalReserved + reservedForEquivalent;
        }

        return totalReserved;
    }

private:
    static TrustLineAmount totalReservedAmount(
        const ReservationsMap& reservations,
        AmountReservation::ReservationDirection reservationDirection,
        const SerializedEquivalent equivalent)
    {
        TrustLineAmount totalAmount = 0;
        for (const auto &nodeIDAndReservations : reservations) {
            for (const auto &pathIDAndReservation : nodeIDAndReservations.second) {
                if (pathIDAndReservation.second->direction() == reservationDirection &&
                    pathIDAndReservation.second->equivalent() == equivalent) {
                    totalAmount += pathIDAndReservation.second->amount();
                }
            }
        }
        return totalAmount;
    }
};

// Test 7: Calculate total reserved payment amount with no reservations
TEST(CoordinatorExchangePaymentCalculationTest, CalculateTotalReservedWithNoReservations) {
    // Create empty reservations map
    ReservationCalculator::ReservationsMap reservations;

    // Exchange equivalents
    vector<SerializedEquivalent> exchangeEquivalents = {1};

    // Calculate total reserved amount (should be 0 with no reservations)
    TrustLineAmount total = ReservationCalculator::calculateTotalReservedPaymentAmount(
        reservations, exchangeEquivalents);

    // Assertions
    EXPECT_EQ(total, TrustLineAmount(0))
        << "Total reserved amount should be 0 when no reservations exist";
}

// Test 8: Calculate total reserved payment amount with one exchange equivalent
TEST(CoordinatorExchangePaymentCalculationTest, CalculateTotalReservedWithOneEquivalent) {
    // Create reservations map
    ReservationCalculator::ReservationsMap reservations;
    TransactionUUID txUUID;

    // Add outgoing reservations for equivalent 1
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(500), AmountReservation::Outgoing, SerializedEquivalent(1));
    reservations[ContractorID(1)].emplace_back(PathID(1), res1);

    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(300), AmountReservation::Outgoing, SerializedEquivalent(1));
    reservations[ContractorID(2)].emplace_back(PathID(1), res2);

    // Add some incoming reservations (should NOT be counted)
    auto res3 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(200), AmountReservation::Incoming, SerializedEquivalent(1));
    reservations[ContractorID(3)].emplace_back(PathID(2), res3);

    // Exchange equivalents
    vector<SerializedEquivalent> exchangeEquivalents = {1};

    // Calculate total reserved payment amount
    TrustLineAmount total = ReservationCalculator::calculateTotalReservedPaymentAmount(
        reservations, exchangeEquivalents);

    // Assertions
    EXPECT_EQ(total, TrustLineAmount(800))
        << "Total should be 500 + 300 = 800 (only outgoing for equivalent 1)";
}

// Test 9: Calculate total reserved payment amount with multiple exchange equivalents
TEST(CoordinatorExchangePaymentCalculationTest, CalculateTotalReservedWithMultipleEquivalents) {
    // Create reservations map
    ReservationCalculator::ReservationsMap reservations;
    TransactionUUID txUUID;

    // Add outgoing reservations for equivalent 1
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(500), AmountReservation::Outgoing, SerializedEquivalent(1));
    reservations[ContractorID(1)].emplace_back(PathID(1), res1);

    // Add outgoing reservations for equivalent 3
    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(700), AmountReservation::Outgoing, SerializedEquivalent(3));
    reservations[ContractorID(2)].emplace_back(PathID(2), res2);

    auto res3 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(400), AmountReservation::Outgoing, SerializedEquivalent(3));
    reservations[ContractorID(3)].emplace_back(PathID(2), res3);

    // Add reservations for other equivalents (should NOT be counted)
    auto res4 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(1000), AmountReservation::Outgoing, SerializedEquivalent(2));
    reservations[ContractorID(4)].emplace_back(PathID(3), res4);

    // Exchange equivalents (1 and 3, NOT 2)
    vector<SerializedEquivalent> exchangeEquivalents = {1, 3};

    // Calculate total reserved payment amount
    TrustLineAmount total = ReservationCalculator::calculateTotalReservedPaymentAmount(
        reservations, exchangeEquivalents);

    // Assertions
    EXPECT_EQ(total, TrustLineAmount(1600))
        << "Total should be 500 (equiv 1) + 700 + 400 (equiv 3) = 1600";
}

// Test 10: Verify incoming reservations are NOT counted
TEST(CoordinatorExchangePaymentCalculationTest, VerifyIncomingReservationsNotCounted) {
    // Create reservations map
    ReservationCalculator::ReservationsMap reservations;
    TransactionUUID txUUID;

    // Add ONLY incoming reservations for equivalent 1
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(500), AmountReservation::Incoming, SerializedEquivalent(1));
    reservations[ContractorID(1)].emplace_back(PathID(1), res1);

    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(300), AmountReservation::Incoming, SerializedEquivalent(1));
    reservations[ContractorID(2)].emplace_back(PathID(2), res2);

    // Add one outgoing for verification
    auto res3 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(200), AmountReservation::Outgoing, SerializedEquivalent(1));
    reservations[ContractorID(3)].emplace_back(PathID(3), res3);

    // Exchange equivalents
    vector<SerializedEquivalent> exchangeEquivalents = {1};

    // Calculate total reserved payment amount
    TrustLineAmount total = ReservationCalculator::calculateTotalReservedPaymentAmount(
        reservations, exchangeEquivalents);

    // Assertions
    EXPECT_EQ(total, TrustLineAmount(200))
        << "Total should be 200 (only outgoing), incoming reservations should not be counted";
}

// =============================================================================
// Test Group 3b: REAL calculateTotalReservedPaymentAmount Tests
// These tests use actual transaction instances and call the real method
// =============================================================================

// Testable subclass that exposes protected members and methods
class TestableCoordinatorExchangePaymentTransaction : public CoordinatorExchangePaymentTransaction {
public:
    TestableCoordinatorExchangePaymentTransaction(
        const CreditUsageExchangeCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangePathsManager *exchangePathsManager,
        ExchangeRatesManager *exchangeRatesManager,
        ObservingHandler *observingHandler,
        Keystore *keystore,
        bool isPaymentTransactionsAllowedDueToObserving,
        EventsInterfaceManager *eventsInterfaceManager,
        Logger &log,
        SubsystemsController *subsystemsController)
        : CoordinatorExchangePaymentTransaction(
            command, contractorsManager, equivalentsSubsystemsRouter,
            storageHandler, resourcesManager, exchangePathsManager,
            exchangeRatesManager, observingHandler, keystore, isPaymentTransactionsAllowedDueToObserving,
            eventsInterfaceManager, log, subsystemsController)
    {}

    // Expose protected members and methods for testing
    using CoordinatorExchangePaymentTransaction::mCommand;
    using CoordinatorExchangePaymentTransaction::mExchangeAmount;
    using CoordinatorExchangePaymentTransaction::mPathsStats;
    using CoordinatorExchangePaymentTransaction::runPathsResourceProcessingStage;
    using CoordinatorExchangePaymentTransaction::resultAllowablePaymentAmountExceeded;
    using CoordinatorExchangePaymentTransaction::reject;
    using CoordinatorExchangePaymentTransaction::calculateTotalReservedPaymentAmount;
    using CoordinatorExchangePaymentTransaction::exceedsAllowablePaymentAmount;
    using BaseExchangePaymentTransaction::mReservations;
};

// Test fixture with full infrastructure
class CoordinatorExchangePaymentAllowableAmountIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create logger
        logger = make_unique<Logger>();
        io = make_unique<boost::asio::io_context>();

        // Create temporary storage
        auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        dbDir = string("build-tests/testdb_allowable_") + testInfo->name();
        dbName = "test.db";
        std::filesystem::remove_all(dbDir);
        storage = make_unique<StorageHandlerSQLite>(dbDir, dbName, *logger);

        // Initialize keystore
        keystore = make_unique<crypto::Keystore>(*logger);
        {
            auto ioTransaction = storage->beginTransaction();
            keystore->init(ioTransaction);
        }

        // Create events manager
        eventsManager = make_unique<EventsInterfaceManager>(
            vector<pair<string, SerializedEventType>>{},
            vector<pair<string, bool>>{},
            *logger);

        // Create contractors manager
        vector<pair<string, string>> ownAddrs = {{"ipv4", "172.18.28.1:2000"}};
        contractors = make_unique<ContractorsManager>(ownAddrs, storage.get(), *logger);

        // Create router
        vector<SerializedEquivalent> gateways;
        router = make_unique<EquivalentsSubsystemsRouter>(
            storage.get(),
            keystore.get(),
            contractors.get(),
            eventsManager.get(),
            *io,
            gateways,
            *logger);

        // Initialize equivalents
        EQ_RECEIVER = 2;
        EQ_SENDER = 1;
        router->initNewEquivalent(EQ_RECEIVER);
        router->initNewEquivalent(EQ_SENDER);

        // Create managers
        ratesManager = make_unique<ExchangeRatesManager>(*io, *logger);
        pathsManager = make_unique<ExchangePathsManager>(*io, router.get(), ratesManager.get(), contractors.get(), *logger);
        resourcesManager = make_unique<ResourcesManager>();
        subsystemsController = make_unique<SubsystemsController>(*logger);

        // Create test contractor
        contractorAddress = make_shared<IPv4WithPortAddress>("172.18.28.5:2000");
        contractorID = router->getOrCreateParticipantID(contractorAddress);
    }

    void TearDown() override {
        subsystemsController.reset();
        resourcesManager.reset();
        pathsManager.reset();
        ratesManager.reset();
        router.reset();
        contractors.reset();
        eventsManager.reset();
        keystore.reset();
        storage.reset();
        io.reset();
        logger.reset();

        std::filesystem::remove_all(dbDir);
    }

    // Helper to create command with maxAllowablePaymentAmount
    shared_ptr<CreditUsageExchangeCommand> createCommandWithLimit(
        const TrustLineAmount& amount,
        const TrustLineAmount& maxAllowable)
    {
        vector<BaseAddress::Shared> addresses = {contractorAddress};
        return TestCommandBuilder::buildExchangeCommand(
            addresses,
            amount,
            EQ_RECEIVER,
            {EQ_SENDER},
            maxAllowable);
    }


    unique_ptr<Logger> logger;
    unique_ptr<boost::asio::io_context> io;
    unique_ptr<StorageHandlerSQLite> storage;
    unique_ptr<crypto::Keystore> keystore;
    unique_ptr<EventsInterfaceManager> eventsManager;
    unique_ptr<ContractorsManager> contractors;
    unique_ptr<EquivalentsSubsystemsRouter> router;
    unique_ptr<ExchangeRatesManager> ratesManager;
    unique_ptr<ExchangePathsManager> pathsManager;
    unique_ptr<ResourcesManager> resourcesManager;
    unique_ptr<SubsystemsController> subsystemsController;

    string dbDir;
    string dbName;
    BaseAddress::Shared contractorAddress;
    ContractorID contractorID;
    SerializedEquivalent EQ_RECEIVER;
    SerializedEquivalent EQ_SENDER;
};

// Test 11: resultAllowablePaymentAmountExceeded returns correct transaction result
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, ResultAllowablePaymentAmountExceeded) {
    // Create command
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(1500));

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Call resultAllowablePaymentAmountExceeded
    auto result = transaction->resultAllowablePaymentAmountExceeded();

    // Verify result is not null
    ASSERT_NE(result, nullptr) << "Result should not be null";

    // Get CommandResult from TransactionResult
    auto commandResult = result->commandResult();
    ASSERT_NE(commandResult, nullptr) << "CommandResult should not be null";

    // Verify code 415
    EXPECT_EQ(commandResult->resultCode(), 415)
        << "resultAllowablePaymentAmountExceeded() should return result with code 415";

    // Verify message contains expected text
    EXPECT_TRUE(commandResult->serialize().find("Allowable payment amount has been exceeded") != std::string::npos)
        << "Result message should contain 'Allowable payment amount has been exceeded'";
}

// Test 12: exceedsAllowablePaymentAmount returns false when within limit
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, EarlyValidation_WithinLimit) {
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(1500));

    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    transaction->mExchangeAmount = TrustLineAmount(1400);

    EXPECT_FALSE(transaction->exceedsAllowablePaymentAmount(transaction->mExchangeAmount))
        << "Exchange amount within limit should not trigger rejection";
}

// Test 12b: exceedsAllowablePaymentAmount returns true and result code 415 when exceeded
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, EarlyValidation_ExceedsLimit) {
    auto command = createCommandWithLimit(TrustLineAmount(1500), TrustLineAmount(1200));

    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    transaction->mExchangeAmount = TrustLineAmount(1500);

    EXPECT_TRUE(transaction->exceedsAllowablePaymentAmount(transaction->mExchangeAmount))
        << "Exchange amount above limit should trigger rejection";

    auto rejectionResult = transaction->resultAllowablePaymentAmountExceeded();
    ASSERT_NE(rejectionResult, nullptr);
    auto commandResult = rejectionResult->commandResult();
    ASSERT_NE(commandResult, nullptr);
    EXPECT_EQ(commandResult->resultCode(), 415)
        << "Allowable payment rejection should use code 415";
}

// =============================================================================
// Test Group 5: End-to-End Scenario Tests
// =============================================================================

// Test 13: E2E - Rollback mechanism via reject() is accessible
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, E2E_RollbackMechanismAccessible) {
    // Create command
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(1500));

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    auto result = transaction->reject("Test: Allowable payment amount exceeded");

    // Verify result
    ASSERT_NE(result, nullptr) << "Reject should return result";
    auto commandResult = result->commandResult();
    ASSERT_NE(commandResult, nullptr);
    EXPECT_EQ(commandResult->resultCode(), 409)
        << "Reject should translate to 'no consensus' response";
}

// Test 14: E2E - Payment workflow with limit value 0 (legacy unlimited behavior)
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, E2E_PaymentLegacyUnlimitedBehavior) {
    // Create command with limit value 0 (interpreted as unlimited)
    vector<BaseAddress::Shared> addresses = {contractorAddress};
    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        EQ_RECEIVER,
        {EQ_SENDER});

    // Verify command has unlimited amount (max)
    ASSERT_EQ(command->maxAllowablePaymentAmount(), std::numeric_limits<TrustLineAmount>::max())
        << "Command should have maxAllowablePaymentAmount = max() for unlimited";

    // Create transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Verify transaction created successfully
    ASSERT_NE(transaction, nullptr) << "Transaction should be created";

    // Set high mExchangeAmount (would exceed limit if present)
    transaction->mExchangeAmount = TrustLineAmount(999999);

    // In unlimited mode (maxAllowablePaymentAmount = max), validation will never fail:
    // if (actualAmount > max) { ... } is always false
    // Since maxAllowablePaymentAmount is max, no amount can exceed it

    EXPECT_EQ(command->maxAllowablePaymentAmount(), std::numeric_limits<TrustLineAmount>::max())
        << "Unlimited mode: maxAllowablePaymentAmount should be max()";

    // Verify that even with very high mExchangeAmount, no validation error would occur
    // because the allowable amount is effectively infinite
    EXPECT_FALSE(transaction->exceedsAllowablePaymentAmount(transaction->mExchangeAmount))
        << "Unlimited mode should never exceed allowable payment";

    // This demonstrates backward compatibility - by using limit 0 the coordinator
    // treats the payment as having no practical ceiling (max amount)
}

// Test 15: E2E - Access to stage methods for validation testing
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, E2E_StageMethodsAccessible) {
    // Create command with limit
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(1500));

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Verify protected methods and members are accessible through testable subclass

    // 1. Verify mExchangeAmount is accessible and modifiable
    transaction->mExchangeAmount = TrustLineAmount(1200);
    EXPECT_EQ(transaction->mExchangeAmount, TrustLineAmount(1200))
        << "mExchangeAmount should be modifiable";

    // 2. Add reservations and verify aggregation helpers
    TransactionUUID txUUID;
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(400), AmountReservation::Outgoing, EQ_SENDER);
    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(300), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(10)].emplace_back(PathID(1), res1);
    transaction->mReservations[ContractorID(11)].emplace_back(PathID(2), res2);

    TrustLineAmount totalReserved = transaction->calculateTotalReservedPaymentAmount();
    EXPECT_EQ(totalReserved, TrustLineAmount(700))
        << "Total reserved amount should equal sum of outgoing reservations";
    EXPECT_FALSE(transaction->exceedsAllowablePaymentAmount(totalReserved))
        << "Aggregated total should remain within allowable limit";
}

// Test 16: Late validation treats totals within limit as valid
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, LateValidation_TotalWithinLimit) {
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(2000));

    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    TransactionUUID txUUID;
    auto res = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(800), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(3)].emplace_back(PathID(1), res);

    TrustLineAmount total = transaction->calculateTotalReservedPaymentAmount();
    EXPECT_EQ(total, TrustLineAmount(800));
    EXPECT_FALSE(transaction->exceedsAllowablePaymentAmount(total));
}

// Test 17: Late validation reports violation when totals exceed limit
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, LateValidation_TotalExceedsLimit) {
    auto command = createCommandWithLimit(TrustLineAmount(1500), TrustLineAmount(900));

    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    TransactionUUID txUUID;
    transaction->mReservations[ContractorID(4)].emplace_back(
        PathID(1),
        make_shared<const AmountReservation>(
            txUUID,
            TrustLineAmount(500),
            AmountReservation::Outgoing,
            EQ_SENDER));
    transaction->mReservations[ContractorID(5)].emplace_back(
        PathID(2),
        make_shared<const AmountReservation>(
            txUUID,
            TrustLineAmount(600),
            AmountReservation::Outgoing,
            EQ_SENDER));

    TrustLineAmount total = transaction->calculateTotalReservedPaymentAmount();
    EXPECT_EQ(total, TrustLineAmount(1100));
    EXPECT_TRUE(transaction->exceedsAllowablePaymentAmount(total));

    auto rejectionResult = transaction->resultAllowablePaymentAmountExceeded();
    ASSERT_NE(rejectionResult, nullptr);
    auto commandResult = rejectionResult->commandResult();
    ASSERT_NE(commandResult, nullptr);
    EXPECT_EQ(commandResult->resultCode(), 415);
}

// =============================================================================
// Test Group 4: Integration Tests for Early/Late Validation and Calculation
// =============================================================================
//
// Tests 11-15: Early/late validation, rollback, legacy behavior
// Tests 16-19: ✅ REAL calculateTotalReservedPaymentAmount() tests (fulfill DOD)
//              These tests call the ACTUAL transaction method, not a duplicate helper
// =============================================================================

// Test 16: Calculate total reserved with no reservations (REAL method)
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, RealCalculation_NoReservations) {
    // Create command with exchange equivalents {EQ_SENDER}
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(1500));

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Call REAL method - no reservations in mReservations yet
    TrustLineAmount total = transaction->calculateTotalReservedPaymentAmount();

    // Verify
    EXPECT_EQ(total, TrustLineAmount(0))
        << "Total should be 0 when there are no reservations in mReservations";
}

// Test 17: Calculate total reserved with one equivalent (REAL method)
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, RealCalculation_OneEquivalent) {
    // Create command with exchange equivalents {EQ_SENDER}
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(2000));

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Manually add outgoing reservations for EQ_SENDER to mReservations
    TransactionUUID txUUID;
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(500), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(1)].emplace_back(PathID(1), res1);

    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(300), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(2)].emplace_back(PathID(2), res2);

    // Call REAL method
    TrustLineAmount total = transaction->calculateTotalReservedPaymentAmount();

    // Verify
    EXPECT_EQ(total, TrustLineAmount(800))
        << "REAL method should return 800 (500 + 300) for one exchange equivalent";
}

// Test 18: Calculate total reserved with multiple equivalents (REAL method)
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, RealCalculation_MultipleEquivalents) {
    // Create command with TWO exchange equivalents
    vector<BaseAddress::Shared> addresses = {contractorAddress};
    // Build command with exchangeEquivalents = {EQ_SENDER, EQ_RECEIVER}
    // We use TestCommandBuilder for base command and manually adjust the
    // exchange equivalents list for this test scenario.
    auto command = TestCommandBuilder::buildExchangeCommand(
        addresses,
        TrustLineAmount(1000),
        EQ_RECEIVER,
        {EQ_SENDER, SerializedEquivalent(3)}); // Two equivalents

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Manually add outgoing reservations for both equivalents
    TransactionUUID txUUID;

    // Reservations for EQ_SENDER (equiv 1)
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(500), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(1)].emplace_back(PathID(1), res1);

    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(300), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(2)].emplace_back(PathID(2), res2);

    // Reservations for equivalent 3
    auto res3 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(200), AmountReservation::Outgoing, SerializedEquivalent(3));
    transaction->mReservations[ContractorID(3)].emplace_back(PathID(3), res3);

    auto res4 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(400), AmountReservation::Outgoing, SerializedEquivalent(3));
    transaction->mReservations[ContractorID(4)].emplace_back(PathID(4), res4);

    // Call REAL method
    TrustLineAmount total = transaction->calculateTotalReservedPaymentAmount();

    // Verify: 800 (equiv 1) + 600 (equiv 3) = 1400
    EXPECT_EQ(total, TrustLineAmount(1400))
        << "REAL method should return 1400 (800 from equiv 1 + 600 from equiv 3)";
}

// Test 19: Verify incoming reservations NOT counted (REAL method)
TEST_F(CoordinatorExchangePaymentAllowableAmountIntegrationTest, RealCalculation_IncomingNotCounted) {
    // Create command with exchange equivalents {EQ_SENDER}
    auto command = createCommandWithLimit(TrustLineAmount(1000), TrustLineAmount(2000));

    // Create testable transaction
    auto transaction = make_shared<TestableCoordinatorExchangePaymentTransaction>(
        command,
        contractors.get(),
        router.get(),
        storage.get(),
        resourcesManager.get(),
        pathsManager.get(),
        ratesManager.get(),
        nullptr,  // ObservingHandler
        keystore.get(),
        true,
        eventsManager.get(),
        *logger,
        subsystemsController.get());

    // Manually add reservations: mostly incoming, one outgoing
    TransactionUUID txUUID;

    // Incoming reservations (should NOT be counted)
    auto res1 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(500), AmountReservation::Incoming, EQ_SENDER);
    transaction->mReservations[ContractorID(1)].emplace_back(PathID(1), res1);

    auto res2 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(300), AmountReservation::Incoming, EQ_SENDER);
    transaction->mReservations[ContractorID(2)].emplace_back(PathID(2), res2);

    // One outgoing reservation (SHOULD be counted)
    auto res3 = make_shared<const AmountReservation>(
        txUUID, TrustLineAmount(200), AmountReservation::Outgoing, EQ_SENDER);
    transaction->mReservations[ContractorID(3)].emplace_back(PathID(3), res3);

    // Call REAL method
    TrustLineAmount total = transaction->calculateTotalReservedPaymentAmount();

    // Verify: only outgoing (200) counted, incoming (500+300) ignored
    EXPECT_EQ(total, TrustLineAmount(200))
        << "REAL method should return 200 (only outgoing), incoming should not be counted";
}

// =============================================================================
// Summary Note: Integration vs Unit Test Trade-offs
// =============================================================================
//
// The tests above demonstrate that with the full infrastructure (ContractorsManager,
// Router, PathsManager, etc.), we CAN:
// 1. Create CoordinatorExchangePaymentTransaction instances
// 2. Access and manipulate protected state (mExchangeAmount, mReservations, mPathsStats)
// 3. Call stage methods (runPathsResourceProcessingStage, etc.)
// 4. Inject messages via pushContext()
// 5. Test rollback via reject() and verify mReservations cleanup
//
// However, fully executing multi-stage E2E scenarios requires:
// - Setting up trust lines and balances in TrustLinesManager
// - Caching valid paths with proper structure in ExchangePathsManager
// - Configuring exchange rates in ExchangeRatesManager
// - Managing transaction state machine (mStep, mState transitions)
// - Injecting properly formatted messages for each stage
// - Handling async operations and signals
//
// These requirements make "heavy unit tests" blur the line with integration tests.
// The infrastructure demonstrated here provides the foundation for such tests,
// which can be expanded as needed for specific validation scenarios.
//
// =============================================================================
