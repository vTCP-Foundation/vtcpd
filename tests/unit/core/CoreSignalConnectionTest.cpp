#include <gtest/gtest.h>
#include "../../../src/core/resources/manager/ResourcesManager.h"
#include "../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/common/Types.h"

using namespace std;

/**
 * Test fixture for Core signal connection tests.
 * Tests that RequestExchangePathsResourceSignal is properly connected and
 * correctly passes all parameters to the Core slot that launches
 * FindPathsByMaxFlowExchangeTransaction.
 *
 * Note: This test focuses on signal infrastructure without requiring full Core initialization.
 * It validates the signal chain: signal emission → parameter passing → callback invocation.
 */
class CoreSignalConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create ResourcesManager instance (lightweight, no dependencies)
        manager = make_unique<ResourcesManager>();

        // Create test data matching typical exchange payment scenario
        testUUID = TransactionUUID();
        contractorAddress = make_shared<IPv4WithPortAddress>("172.18.28.5:2000");
        exchangeEquivalents = {1001, 1002, 1003}; // Multiple sender equivalents
        receiverEquivalent = 2002;
    }

    /**
     * Simulates Core's onExchangePathsResourceRequestedSlot behavior.
     * In real Core, this would call TransactionsManager to launch FindPathsByMaxFlowExchangeTransaction.
     * For testing, we capture parameters to verify correct signal propagation.
     */
    struct CallbackCapture {
        bool called = false;
        TransactionUUID capturedUUID;
        BaseAddress::Shared capturedAddress;
        vector<SerializedEquivalent> capturedExchangeEquivs;
        SerializedEquivalent capturedReceiverEquiv;

        void reset() {
            called = false;
            capturedUUID = TransactionUUID();
            capturedAddress.reset();
            capturedExchangeEquivs.clear();
            capturedReceiverEquiv = 0;
        }
    };

    unique_ptr<ResourcesManager> manager;
    TransactionUUID testUUID;
    BaseAddress::Shared contractorAddress;
    vector<SerializedEquivalent> exchangeEquivalents;
    SerializedEquivalent receiverEquivalent;
};

/**
 * Test 1: RequestExchangePathsResourceSignal exists and can be connected.
 * Validates that the signal is declared and follows Boost.Signals2 interface.
 */
TEST_F(CoreSignalConnectionTest, RequestExchangePathsSignal_ExistsAndConnectable) {
    // Assert - signal member exists (compile-time check)
    // Connection succeeds without exceptions
    EXPECT_NO_THROW({
        manager->requestExchangePathsResourceSignal.connect(
            [](const TransactionUUID&, BaseAddress::Shared,
               const vector<SerializedEquivalent>&, const SerializedEquivalent) {
                // Empty handler for connectivity test
            });
    });
}

/**
 * Test 2: Signal emission launches callback with correct transaction UUID.
 * Simulates Core slot receiving request from CoordinatorExchangePaymentTransaction.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_PassesCorrectTransactionUUID) {
    // Arrange
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedUUID = uuid;
        });

    // Act - ResourcesManager emits signal (called by CoordinatorExchangePaymentTransaction)
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Callback invoked with correct UUID
    ASSERT_TRUE(capture.called) << "Signal callback not invoked";
    EXPECT_EQ(capture.capturedUUID, testUUID)
        << "Transaction UUID not correctly passed through signal";
}

/**
 * Test 3: Signal emission passes correct contractor address.
 * Validates address parameter reaches Core slot for transaction creation.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_PassesCorrectContractorAddress) {
    // Arrange
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedAddress = address;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Address correctly passed
    ASSERT_TRUE(capture.called);
    EXPECT_EQ(capture.capturedAddress, contractorAddress)
        << "Contractor address not correctly passed";
    EXPECT_EQ(capture.capturedAddress->fullAddress(), "172.18.28.5:2000")
        << "Address content mismatch";
}

/**
 * Test 4: Signal emission passes correct exchange equivalents vector.
 * Validates all sender equivalents reach Core slot for path collection.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_PassesCorrectExchangeEquivalents) {
    // Arrange
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedExchangeEquivs = exchangeEquivs;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Exchange equivalents correctly passed
    ASSERT_TRUE(capture.called);
    EXPECT_EQ(capture.capturedExchangeEquivs, exchangeEquivalents)
        << "Exchange equivalents vector not correctly passed";
    EXPECT_EQ(capture.capturedExchangeEquivs.size(), 3)
        << "Exchange equivalents size mismatch";
    EXPECT_EQ(capture.capturedExchangeEquivs[0], 1001);
    EXPECT_EQ(capture.capturedExchangeEquivs[1], 1002);
    EXPECT_EQ(capture.capturedExchangeEquivs[2], 1003);
}

/**
 * Test 5: Signal emission passes correct receiver equivalent.
 * Validates receiver equivalent (mEquivalent from coordinator) reaches Core slot.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_PassesCorrectReceiverEquivalent) {
    // Arrange
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedReceiverEquiv = receiverEquiv;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Receiver equivalent correctly passed
    ASSERT_TRUE(capture.called);
    EXPECT_EQ(capture.capturedReceiverEquiv, receiverEquivalent)
        << "Receiver equivalent not correctly passed";
    EXPECT_EQ(capture.capturedReceiverEquiv, 2002);
}

/**
 * Test 6: Signal passes all parameters correctly in single emission.
 * Comprehensive validation of complete parameter chain.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_PassesAllParametersCorrectly) {
    // Arrange
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedUUID = uuid;
            capture.capturedAddress = address;
            capture.capturedExchangeEquivs = exchangeEquivs;
            capture.capturedReceiverEquiv = receiverEquiv;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - All parameters correct
    ASSERT_TRUE(capture.called) << "Callback not invoked";
    EXPECT_EQ(capture.capturedUUID, testUUID);
    EXPECT_EQ(capture.capturedAddress, contractorAddress);
    EXPECT_EQ(capture.capturedExchangeEquivs, exchangeEquivalents);
    EXPECT_EQ(capture.capturedReceiverEquiv, receiverEquivalent);
}

/**
 * Test 7: Signal supports multiple connections (Core + other subscribers).
 * Validates Boost.Signals2 multi-subscriber capability.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_SupportsMultipleConnections) {
    // Arrange
    int callCount = 0;

    // Connect two handlers (simulates Core + potential monitoring/logging)
    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            callCount++;
        });

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            callCount++;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Both handlers called
    EXPECT_EQ(callCount, 2) << "Multiple signal connections not all invoked";
}

/**
 * Test 8: Signal handles empty exchange equivalents vector.
 * Edge case: coordinator requests paths for zero equivalents (should not happen but must not crash).
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_HandlesEmptyExchangeEquivalents) {
    // Arrange
    vector<SerializedEquivalent> emptyEquivs;
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedExchangeEquivs = exchangeEquivs;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        emptyEquivs,
        receiverEquivalent);

    // Assert - Empty vector correctly passed
    ASSERT_TRUE(capture.called);
    EXPECT_TRUE(capture.capturedExchangeEquivs.empty())
        << "Empty exchange equivalents not correctly passed";
}

/**
 * Test 9: Signal handles single exchange equivalent.
 * Common case: exchange payment with one sender equivalent.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_HandlesSingleExchangeEquivalent) {
    // Arrange
    vector<SerializedEquivalent> singleEquiv = {1001};
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedExchangeEquivs = exchangeEquivs;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        singleEquiv,
        receiverEquivalent);

    // Assert
    ASSERT_TRUE(capture.called);
    EXPECT_EQ(capture.capturedExchangeEquivs.size(), 1);
    EXPECT_EQ(capture.capturedExchangeEquivs[0], 1001);
}

/**
 * Test 10: Signal handles maximum exchange equivalents (5 as per PRD 06 limit).
 * Validates upper bound of exchange equivalents vector.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_HandlesMaximumExchangeEquivalents) {
    // Arrange - PRD 06 specifies max 5 exchange equivalents
    vector<SerializedEquivalent> maxEquivs = {1001, 1002, 1003, 1004, 1005};
    CallbackCapture capture;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            capture.called = true;
            capture.capturedExchangeEquivs = exchangeEquivs;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        maxEquivs,
        receiverEquivalent);

    // Assert
    ASSERT_TRUE(capture.called);
    EXPECT_EQ(capture.capturedExchangeEquivs.size(), 5)
        << "Maximum exchange equivalents not correctly passed";
    EXPECT_EQ(capture.capturedExchangeEquivs, maxEquivs);
}

/**
 * Test 11: Signal correctly differentiates between different contractor addresses.
 * Validates address parameter independence across multiple signal emissions.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_DifferentiatesContractorAddresses) {
    // Arrange
    auto address1 = make_shared<IPv4WithPortAddress>("192.168.1.100:3000");
    auto address2 = make_shared<IPv4WithPortAddress>("10.0.0.50:4000");
    
    vector<BaseAddress::Shared> capturedAddresses;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared address,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            capturedAddresses.push_back(address);
        });

    // Act - Emit signal twice with different addresses
    manager->requestExchangePaths(testUUID, address1, exchangeEquivalents, receiverEquivalent);
    manager->requestExchangePaths(testUUID, address2, exchangeEquivalents, receiverEquivalent);

    // Assert - Both addresses captured correctly
    ASSERT_EQ(capturedAddresses.size(), 2);
    EXPECT_EQ(capturedAddresses[0], address1);
    EXPECT_EQ(capturedAddresses[1], address2);
    EXPECT_NE(capturedAddresses[0], capturedAddresses[1])
        << "Different addresses should not be equal";
}

/**
 * Test 12: Signal correctly differentiates between different receiver equivalents.
 * Validates receiver equivalent parameter independence.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_DifferentiatesReceiverEquivalents) {
    // Arrange
    SerializedEquivalent receiver1 = 2001;
    SerializedEquivalent receiver2 = 2002;
    
    vector<SerializedEquivalent> capturedReceivers;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent receiverEquiv) {
            capturedReceivers.push_back(receiverEquiv);
        });

    // Act
    manager->requestExchangePaths(testUUID, contractorAddress, exchangeEquivalents, receiver1);
    manager->requestExchangePaths(testUUID, contractorAddress, exchangeEquivalents, receiver2);

    // Assert
    ASSERT_EQ(capturedReceivers.size(), 2);
    EXPECT_EQ(capturedReceivers[0], receiver1);
    EXPECT_EQ(capturedReceivers[1], receiver2);
}

/**
 * Test 13: Signal signature matches Core::onExchangePathsResourceRequestedSlot expectations.
 * Compile-time validation of parameter types.
 */
TEST_F(CoreSignalConnectionTest, SignalSignature_MatchesCoreSlotExpectations) {
    // This is primarily a compile-time test
    // If it compiles, the signature matches Core's slot requirements
    manager->requestExchangePathsResourceSignal.connect(
        [](const TransactionUUID &uuid,
           BaseAddress::Shared address,
           const vector<SerializedEquivalent> &exchangeEquivs,
           const SerializedEquivalent receiverEquiv) {
            // Verify types match Core::onExchangePathsResourceRequestedSlot signature
            static_assert(is_same_v<decltype(uuid), const TransactionUUID&>,
                          "UUID parameter type mismatch with Core slot");
            static_assert(is_same_v<decltype(address), BaseAddress::Shared>,
                          "Address parameter type mismatch with Core slot");
            static_assert(is_same_v<decltype(exchangeEquivs), const vector<SerializedEquivalent>&>,
                          "Exchange equivalents parameter type mismatch with Core slot");
            static_assert(is_same_v<decltype(receiverEquiv), const SerializedEquivalent>,
                          "Receiver equivalent parameter type mismatch with Core slot");
        });

    SUCCEED() << "Signal signature matches Core slot expectations";
}

/**
 * Test 14: Signal emission is synchronous (callback completes before requestExchangePaths returns).
 * Validates that Core slot executes immediately, not deferred.
 */
TEST_F(CoreSignalConnectionTest, SignalEmission_IsSynchronous) {
    // Arrange
    bool callbackCompleted = false;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            callbackCompleted = true;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Callback completed before requestExchangePaths returned
    EXPECT_TRUE(callbackCompleted)
        << "Signal emission should be synchronous (callback completes immediately)";
}

/**
 * Test 15: Signal coexists with other ResourcesManager signals without interference.
 * Validates signal independence in ResourcesManager.
 */
TEST_F(CoreSignalConnectionTest, SignalCoexistence_WithOtherResourceManagerSignals) {
    // Arrange - Connect to both RequestPathsResourcesSignal and RequestExchangePathsResourceSignal
    bool pathsSignalEmitted = false;
    bool exchangePathsSignalEmitted = false;

    // Connect to regular paths signal (single-equivalent)
    manager->requestPathsResourcesSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared, const SerializedEquivalent) {
            pathsSignalEmitted = true;
        });

    // Connect to exchange paths signal (multi-equivalent)
    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            exchangePathsSignalEmitted = true;
        });

    // Act - Emit only exchange paths signal
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - Only exchange paths signal emitted, paths signal unaffected
    EXPECT_TRUE(exchangePathsSignalEmitted);
    EXPECT_FALSE(pathsSignalEmitted)
        << "Other signals should not be affected by exchange paths signal emission";
}

