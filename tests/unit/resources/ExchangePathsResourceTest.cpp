#include <gtest/gtest.h>
#include "../../../src/core/resources/resources/ExchangePathsResource.h"
#include "../../../src/core/resources/resources/BaseResource.h"
#include "../../../src/core/transactions/transactions/base/TransactionUUID.h"

using namespace std;

/**
 * Test fixture for ExchangePathsResource tests.
 * Provides common setup for testing resource creation, UUID handling, and type identification.
 */
class ExchangePathsResourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test transaction UUID
        testUUID = TransactionUUID();
    }

    TransactionUUID testUUID;
};

/**
 * Test that constructor correctly initializes ExchangePathsResource with transaction UUID.
 */
TEST_F(ExchangePathsResourceTest, Constructor_InitializesWithTransactionUUID) {
    // Arrange & Act
    ExchangePathsResource resource(testUUID);

    // Assert
    EXPECT_EQ(resource.transactionUUID(), testUUID);
}

/**
 * Test that transactionUUID() getter returns correct value.
 */
TEST_F(ExchangePathsResourceTest, TransactionUUIDGetter_ReturnsCorrectValue) {
    // Arrange
    ExchangePathsResource resource(testUUID);

    // Act
    const TransactionUUID &retrievedUUID = resource.transactionUUID();

    // Assert
    EXPECT_EQ(retrievedUUID, testUUID);
    // Verify it returns a reference to the same object
    EXPECT_EQ(&retrievedUUID, &resource.transactionUUID());
}

/**
 * Test that resource type is correctly identified as ExchangePaths.
 */
TEST_F(ExchangePathsResourceTest, ResourceType_CorrectlyIdentified) {
    // Arrange
    ExchangePathsResource resource(testUUID);

    // Act
    BaseResource::ResourceType resourceType = resource.type();

    // Assert
    EXPECT_EQ(resourceType, BaseResource::ExchangePaths);
}

/**
 * Test that ExchangePathsResource correctly inherits from BaseResource.
 */
TEST_F(ExchangePathsResourceTest, InheritsFromBaseResource) {
    // Arrange & Act
    ExchangePathsResource resource(testUUID);
    BaseResource *basePtr = &resource;

    // Assert - can be used as BaseResource
    EXPECT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr->type(), BaseResource::ExchangePaths);
}

/**
 * Test that multiple resource instances maintain independent UUIDs.
 */
TEST_F(ExchangePathsResourceTest, MultipleInstances_IndependentUUIDs) {
    // Arrange
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    // Act
    ExchangePathsResource resource1(uuid1);
    ExchangePathsResource resource2(uuid2);

    // Assert
    EXPECT_EQ(resource1.transactionUUID(), uuid1);
    EXPECT_EQ(resource2.transactionUUID(), uuid2);
}

/**
 * Test that ExchangePathsResource works correctly with shared pointers.
 */
TEST_F(ExchangePathsResourceTest, SharedPointer_WorksCorrectly) {
    // Arrange & Act
    auto resourcePtr = make_shared<ExchangePathsResource>(testUUID);

    // Assert
    EXPECT_NE(resourcePtr, nullptr);
    EXPECT_EQ(resourcePtr->transactionUUID(), testUUID);
    EXPECT_EQ(resourcePtr->type(), BaseResource::ExchangePaths);
}

/**
 * Test that ExchangePathsResource::Shared typedef works correctly.
 */
TEST_F(ExchangePathsResourceTest, SharedTypedef_WorksCorrectly) {
    // Arrange & Act
    ExchangePathsResource::Shared resourcePtr = make_shared<ExchangePathsResource>(testUUID);

    // Assert
    EXPECT_NE(resourcePtr, nullptr);
    EXPECT_EQ(resourcePtr->transactionUUID(), testUUID);
    EXPECT_EQ(resourcePtr->type(), BaseResource::ExchangePaths);
}

/**
 * Test that BaseResource::Shared can hold ExchangePathsResource.
 */
TEST_F(ExchangePathsResourceTest, BaseResourceShared_CanHoldExchangePathsResource) {
    // Arrange & Act
    BaseResource::Shared baseResourcePtr = make_shared<ExchangePathsResource>(testUUID);

    // Assert
    EXPECT_NE(baseResourcePtr, nullptr);
    EXPECT_EQ(baseResourcePtr->type(), BaseResource::ExchangePaths);
    EXPECT_EQ(baseResourcePtr->transactionUUID(), testUUID);
}

/**
 * Test that resource type can be verified when accessed through BaseResource pointer.
 * Note: dynamic_cast is not available because BaseResource is not polymorphic,
 * so we use type() method for runtime type identification.
 */
TEST_F(ExchangePathsResourceTest, TypeIdentification_ThroughBaseResourcePointer) {
    // Arrange
    BaseResource::Shared baseResourcePtr = make_shared<ExchangePathsResource>(testUUID);

    // Act - use type() method instead of dynamic_cast
    BaseResource::ResourceType resourceType = baseResourcePtr->type();

    // Assert
    EXPECT_EQ(resourceType, BaseResource::ExchangePaths);
    EXPECT_EQ(baseResourcePtr->transactionUUID(), testUUID);
}

