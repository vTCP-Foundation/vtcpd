#include <gtest/gtest.h>

#include "core/network/rpc/requests/GetBlockNumberRpcRequest.h"


// Test constructor stores transactionUUID
TEST(GetBlockNumberRpcRequestTest, ConstructorStoresTransactionUUID)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcRequest request(uuid);

    EXPECT_EQ(request.transactionUUID(), uuid);
}

// Test method() returns RpcMethod::GetBlockNumber
TEST(GetBlockNumberRpcRequestTest, MethodReturnsGetBlockNumber)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcRequest request(uuid);

    EXPECT_EQ(request.method(), RpcMethod::GetBlockNumber);
}

// Test Shared typedef
TEST(GetBlockNumberRpcRequestTest, SharedTypedefWorks)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcRequest::Shared request = std::make_shared<GetBlockNumberRpcRequest>(uuid);

    EXPECT_NE(request, nullptr);
    EXPECT_EQ(request->method(), RpcMethod::GetBlockNumber);
}

// Test toJson() produces valid JSON-RPC 1.0 payload
TEST(GetBlockNumberRpcRequestTest, ToJsonProducesValidPayload)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetBlockNumberRpcRequest request(uuid);
    json jsonPayload = request.toJson();

    EXPECT_EQ(jsonPayload["method"], "RPCService.GetCurrentBlock");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);
    EXPECT_TRUE(jsonPayload["params"][0].is_object());
    EXPECT_TRUE(jsonPayload["params"][0].empty());
}

