#include <gtest/gtest.h>

#include "core/network/rpc/responses/GetBlockNumberRpcResponse.h"


// Test constructor with success status
TEST(GetBlockNumberRpcResponseTest, ConstructorWithSuccessStatus)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const BlockNumber blockNumber = 12345;

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::Success, blockNumber);

    EXPECT_EQ(response.transactionUUID(), uuid);
    EXPECT_EQ(response.status(), RpcResponseStatus::Success);
    EXPECT_EQ(response.blockNumber(), blockNumber);
    EXPECT_TRUE(response.isSuccess());
    EXPECT_TRUE(response.errorMessage().empty());
}

// Test constructor with error status
TEST(GetBlockNumberRpcResponseTest, ConstructorWithErrorStatus)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string errorMessage = "Connection failed";

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::NetworkError, 0, errorMessage);

    EXPECT_EQ(response.status(), RpcResponseStatus::NetworkError);
    EXPECT_EQ(response.blockNumber(), 0);
    EXPECT_FALSE(response.isSuccess());
    EXPECT_EQ(response.errorMessage(), errorMessage);
}

// Test blockNumber() getter
TEST(GetBlockNumberRpcResponseTest, BlockNumberGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const BlockNumber expectedBlock = 999999;

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::Success, expectedBlock);

    EXPECT_EQ(response.blockNumber(), expectedBlock);
}

// Test method() returns RpcMethod::GetBlockNumber
TEST(GetBlockNumberRpcResponseTest, MethodReturnsGetBlockNumber)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::Success, 100);

    EXPECT_EQ(response.method(), RpcMethod::GetBlockNumber);
}

// Test fromJson() with valid response
TEST(GetBlockNumberRpcResponseTest, FromJsonWithValidResponse)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"block_number", 42}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetBlockNumberRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->blockNumber(), 42);
}

// Test fromJson() with non-success status
TEST(GetBlockNumberRpcResponseTest, FromJsonWithNonSuccessStatus)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = json::object();
    const std::string errorMessage = "Timeout occurred";

    auto response = GetBlockNumberRpcResponse::fromJson(
        uuid, RpcResponseStatus::Timeout, responseJson, errorMessage);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::Timeout);
    EXPECT_EQ(response->blockNumber(), 0);
    EXPECT_EQ(response->errorMessage(), errorMessage);
}

// Test fromJson() with invalid JSON structure
TEST(GetBlockNumberRpcResponseTest, FromJsonWithInvalidStructure)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    // Missing "result" field
    json responseJson = {{"error", nullptr}, {"id", 1}};

    auto response = GetBlockNumberRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::ParseError);
}

// Test Shared typedef
TEST(GetBlockNumberRpcResponseTest, SharedTypedefWorks)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcResponse::Shared response = std::make_shared<GetBlockNumberRpcResponse>(
        uuid, RpcResponseStatus::Success, 500);

    EXPECT_NE(response, nullptr);
    EXPECT_EQ(response->blockNumber(), 500);
}

