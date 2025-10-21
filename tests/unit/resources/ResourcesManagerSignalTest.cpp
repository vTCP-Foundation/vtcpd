#include <gtest/gtest.h>
#include "../../../src/core/resources/manager/ResourcesManager.h"
#include "../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/common/Types.h"

using namespace std;

/**
 * Test fixture for ResourcesManager signal tests.
 * Provides common setup for testing signal emission, connection, and parameter passing.
 */
class ResourcesManagerSignalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create ResourcesManager instance (uses default constructor)
        manager = make_unique<ResourcesManager>();

        // Create test data
        testUUID = TransactionUUID();
        contractorAddress = make_shared<IPv4WithPortAddress>("127.0.0.1:2000");
        exchangeEquivalents = {1, 2, 3};
        receiverEquivalent = 5;
    }

    unique_ptr<ResourcesManager> manager;
    TransactionUUID testUUID;
    BaseAddress::Shared contractorAddress;
    vector<SerializedEquivalent> exchangeEquivalents;
    SerializedEquivalent receiverEquivalent;
};

/**
 * Test that RequestExchangePathsResourceSignal exists and can be connected.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePathsSignal_Exists) {
    // Assert - signal member exists (compile-time check)
    // This test validates the signal is declared and can be connected
    EXPECT_NO_THROW({
        manager->requestExchangePathsResourceSignal.connect(
            [](const TransactionUUID&, BaseAddress::Shared,
               const vector<SerializedEquivalent>&, const SerializedEquivalent) {
                // Empty handler
            });
    });
}

/**
 * Test that requestExchangePaths() method emits signal with correct parameters.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePaths_EmitsSignalWithCorrectParameters) {
    // Arrange
    bool signalEmitted = false;
    TransactionUUID receivedUUID;
    BaseAddress::Shared receivedAddress;
    vector<SerializedEquivalent> receivedExchangeEquivs;
    SerializedEquivalent receivedReceiverEquiv;

    // Connect to signal
    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID &uuid, BaseAddress::Shared address,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent receiverEquiv) {
            signalEmitted = true;
            receivedUUID = uuid;
            receivedAddress = address;
            receivedExchangeEquivs = exchangeEquivs;
            receivedReceiverEquiv = receiverEquiv;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert
    EXPECT_TRUE(signalEmitted);
    EXPECT_EQ(receivedUUID, testUUID);
    EXPECT_EQ(receivedAddress, contractorAddress);
    EXPECT_EQ(receivedExchangeEquivs, exchangeEquivalents);
    EXPECT_EQ(receivedReceiverEquiv, receiverEquivalent);
}

/**
 * Test that multiple signal connections can coexist and all are triggered.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePaths_MultipleConnections) {
    // Arrange
    int callCount = 0;

    // Connect multiple handlers
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

    // Assert - both handlers called
    EXPECT_EQ(callCount, 2);
}

/**
 * Test that empty exchange equivalents vector is correctly passed through signal.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePaths_EmptyExchangeEquivalents) {
    // Arrange
    vector<SerializedEquivalent> emptyEquivs;
    bool signalEmitted = false;
    vector<SerializedEquivalent> receivedEquivs;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent) {
            signalEmitted = true;
            receivedEquivs = exchangeEquivs;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        emptyEquivs,
        receiverEquivalent);

    // Assert
    EXPECT_TRUE(signalEmitted);
    EXPECT_TRUE(receivedEquivs.empty());
}

/**
 * Test that signal correctly handles multiple exchange equivalents.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePaths_MultipleEquivalents) {
    // Arrange
    vector<SerializedEquivalent> multipleEquivs = {1, 2, 3, 4, 5};
    vector<SerializedEquivalent> receivedEquivs;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent> &exchangeEquivs,
            const SerializedEquivalent) {
            receivedEquivs = exchangeEquivs;
        });

    // Act
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        multipleEquivs,
        receiverEquivalent);

    // Assert
    EXPECT_EQ(receivedEquivs.size(), 5);
    EXPECT_EQ(receivedEquivs, multipleEquivs);
}

/**
 * Test that RequestExchangePathsResourceSignal coexists with other resource signals.
 * Ensures that emitting one signal doesn't affect other signals.
 */
TEST_F(ResourcesManagerSignalTest, SignalCoexistence_WithOtherResourceSignals) {
    // Arrange - connect to both RequestPathsResourcesSignal and RequestExchangePathsResourceSignal
    bool pathsSignalEmitted = false;
    bool exchangePathsSignalEmitted = false;

    // Connect to regular paths signal
    manager->requestPathsResourcesSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared, const SerializedEquivalent) {
            pathsSignalEmitted = true;
        });

    // Connect to exchange paths signal
    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            exchangePathsSignalEmitted = true;
        });

    // Act - emit exchange paths signal
    manager->requestExchangePaths(
        testUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);

    // Assert - only exchange paths signal emitted
    EXPECT_TRUE(exchangePathsSignalEmitted);
    EXPECT_FALSE(pathsSignalEmitted); // Other signal not affected
}

/**
 * Test that signal signature matches expected types at compile time.
 * This test validates the type safety of the signal interface.
 */
TEST_F(ResourcesManagerSignalTest, SignalSignature_MatchesExpectedTypes) {
    // This is a compile-time test - if it compiles, signature is correct
    manager->requestExchangePathsResourceSignal.connect(
        [](const TransactionUUID &uuid,
           BaseAddress::Shared address,
           const vector<SerializedEquivalent> &exchangeEquivs,
           const SerializedEquivalent receiverEquiv) {
            // Verify types are correct at compile time
            static_assert(is_same_v<decltype(uuid), const TransactionUUID&>,
                          "UUID parameter type mismatch");
            static_assert(is_same_v<decltype(address), BaseAddress::Shared>,
                          "Address parameter type mismatch");
            static_assert(is_same_v<decltype(exchangeEquivs), const vector<SerializedEquivalent>&>,
                          "Exchange equivalents parameter type mismatch");
            static_assert(is_same_v<decltype(receiverEquiv), const SerializedEquivalent>,
                          "Receiver equivalent parameter type mismatch");
        });

    SUCCEED(); // If we get here, compile-time checks passed
}

/**
 * Test that signal correctly passes different contractor addresses.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePaths_DifferentContractorAddresses) {
    // Arrange
    auto address1 = make_shared<IPv4WithPortAddress>("192.168.1.100:3000");
    auto address2 = make_shared<IPv4WithPortAddress>("10.0.0.50:4000");
    
    BaseAddress::Shared receivedAddress1;
    BaseAddress::Shared receivedAddress2;
    int callCount = 0;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared address,
            const vector<SerializedEquivalent>&, const SerializedEquivalent) {
            if (callCount == 0) {
                receivedAddress1 = address;
            } else if (callCount == 1) {
                receivedAddress2 = address;
            }
            callCount++;
        });

    // Act
    manager->requestExchangePaths(testUUID, address1, exchangeEquivalents, receiverEquivalent);
    manager->requestExchangePaths(testUUID, address2, exchangeEquivalents, receiverEquivalent);

    // Assert
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(receivedAddress1, address1);
    EXPECT_EQ(receivedAddress2, address2);
}

/**
 * Test that signal correctly passes different receiver equivalents.
 */
TEST_F(ResourcesManagerSignalTest, RequestExchangePaths_DifferentReceiverEquivalents) {
    // Arrange
    SerializedEquivalent receiverEquiv1 = 10;
    SerializedEquivalent receiverEquiv2 = 20;
    
    SerializedEquivalent receivedEquiv1;
    SerializedEquivalent receivedEquiv2;
    int callCount = 0;

    manager->requestExchangePathsResourceSignal.connect(
        [&](const TransactionUUID&, BaseAddress::Shared,
            const vector<SerializedEquivalent>&, const SerializedEquivalent receiverEquiv) {
            if (callCount == 0) {
                receivedEquiv1 = receiverEquiv;
            } else if (callCount == 1) {
                receivedEquiv2 = receiverEquiv;
            }
            callCount++;
        });

    // Act
    manager->requestExchangePaths(testUUID, contractorAddress, exchangeEquivalents, receiverEquiv1);
    manager->requestExchangePaths(testUUID, contractorAddress, exchangeEquivalents, receiverEquiv2);

    // Assert
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(receivedEquiv1, receiverEquiv1);
    EXPECT_EQ(receivedEquiv2, receiverEquiv2);
}

