#include <gtest/gtest.h>

#include "core/network/rpc/RpcResponse.h"
#include "core/network/rpc/responses/GetBlockNumberRpcResponse.h"


// Test constructor stores all fields correctly
TEST(RpcResponseTest, ConstructorStoresAllFields)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const RpcResponseStatus status = RpcResponseStatus::Success;
    const BlockNumber blockNumber = 42;

    GetBlockNumberRpcResponse response(uuid, status, blockNumber);

    EXPECT_EQ(response.transactionUUID(), uuid);
    EXPECT_EQ(response.status(), status);
    EXPECT_TRUE(response.errorMessage().empty());
}

// Test constructor with error message
TEST(RpcResponseTest, ConstructorWithErrorMessage)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string errorMessage = "Connection timeout";

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::Timeout, 0, errorMessage);

    EXPECT_EQ(response.status(), RpcResponseStatus::Timeout);
    EXPECT_EQ(response.errorMessage(), errorMessage);
}

// Test isSuccess() returns true only for Success status
TEST(RpcResponseTest, IsSuccessReturnsTrueOnlyForSuccessStatus)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcResponse successResponse(uuid, RpcResponseStatus::Success, 100);
    GetBlockNumberRpcResponse timeoutResponse(uuid, RpcResponseStatus::Timeout, 0);
    GetBlockNumberRpcResponse networkErrorResponse(uuid, RpcResponseStatus::NetworkError, 0);
    GetBlockNumberRpcResponse parseErrorResponse(uuid, RpcResponseStatus::ParseError, 0);
    GetBlockNumberRpcResponse rpcErrorResponse(uuid, RpcResponseStatus::RpcError, 0);

    EXPECT_TRUE(successResponse.isSuccess());
    EXPECT_FALSE(timeoutResponse.isSuccess());
    EXPECT_FALSE(networkErrorResponse.isSuccess());
    EXPECT_FALSE(parseErrorResponse.isSuccess());
    EXPECT_FALSE(rpcErrorResponse.isSuccess());
}

// Test transactionUUID() getter
TEST(RpcResponseTest, TransactionUUIDGetter)
{
    const TransactionUUID uuid(std::string("11111111-2222-4333-8444-555555555555"));

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::Success, 50);

    EXPECT_EQ(response.transactionUUID(), uuid);
}

// Test status() getter
TEST(RpcResponseTest, StatusGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::NetworkError, 0, "Network failure");

    EXPECT_EQ(response.status(), RpcResponseStatus::NetworkError);
}

// Test errorMessage() getter
TEST(RpcResponseTest, ErrorMessageGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string expectedError = "Parse error: invalid JSON";

    GetBlockNumberRpcResponse response(uuid, RpcResponseStatus::ParseError, 0, expectedError);

    EXPECT_EQ(response.errorMessage(), expectedError);
}

// Test Shared typedef
TEST(RpcResponseTest, SharedTypedefIsDefined)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    RpcResponse::Shared sharedResponse = std::make_shared<GetBlockNumberRpcResponse>(
        uuid, RpcResponseStatus::Success, 100);

    EXPECT_NE(sharedResponse, nullptr);
    EXPECT_TRUE(sharedResponse->isSuccess());
}

