#include <gtest/gtest.h>

#include "core/network/rpc/requests/GetClaimStatusRpcRequest.h"


// Test constructor stores all fields
TEST(GetClaimStatusRpcRequestTest, ConstructorStoresAllFields)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));
    const BlockNumber maxClaimBlockNumber = 5000;

    GetClaimStatusRpcRequest request(transactionUUID, claimTransactionUUID, maxClaimBlockNumber);

    EXPECT_EQ(request.transactionUUID(), transactionUUID);
    EXPECT_EQ(request.claimTransactionUUID(), claimTransactionUUID);
    EXPECT_EQ(request.maxClaimBlockNumber(), maxClaimBlockNumber);
}

// Test claimTransactionUUID() getter
TEST(GetClaimStatusRpcRequestTest, ClaimTransactionUUIDGetter)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));

    GetClaimStatusRpcRequest request(transactionUUID, claimTransactionUUID, 100);

    EXPECT_EQ(request.claimTransactionUUID(), claimTransactionUUID);
}

// Test maxClaimBlockNumber() getter
TEST(GetClaimStatusRpcRequestTest, MaxClaimBlockNumberGetter)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));
    const BlockNumber expectedBlockNumber = 12345;

    GetClaimStatusRpcRequest request(transactionUUID, claimTransactionUUID, expectedBlockNumber);

    EXPECT_EQ(request.maxClaimBlockNumber(), expectedBlockNumber);
}

// Test method() returns RpcMethod::GetClaimStatus
TEST(GetClaimStatusRpcRequestTest, MethodReturnsGetClaimStatus)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));

    GetClaimStatusRpcRequest request(transactionUUID, claimTransactionUUID, 100);

    EXPECT_EQ(request.method(), RpcMethod::GetClaimStatus);
}

// Test toJson() produces valid JSON-RPC payload
TEST(GetClaimStatusRpcRequestTest, ToJsonProducesValidPayload)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));
    const BlockNumber maxClaimBlockNumber = 2000;

    GetClaimStatusRpcRequest request(transactionUUID, claimTransactionUUID, maxClaimBlockNumber);
    json jsonPayload = request.toJson();

    EXPECT_EQ(jsonPayload["method"], "RPCService.GetClaimStatus");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];
    EXPECT_TRUE(params.contains("transaction_uuid"));
    EXPECT_TRUE(params.contains("max_claim_block_number"));
    EXPECT_EQ(params["max_claim_block_number"], maxClaimBlockNumber);
}

// Test Shared typedef
TEST(GetClaimStatusRpcRequestTest, SharedTypedefWorks)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));

    GetClaimStatusRpcRequest::Shared request = std::make_shared<GetClaimStatusRpcRequest>(
        transactionUUID, claimTransactionUUID, 100);

    EXPECT_NE(request, nullptr);
    EXPECT_EQ(request->method(), RpcMethod::GetClaimStatus);
}

