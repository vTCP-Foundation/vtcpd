#include <gtest/gtest.h>

#include "core/network/rpc/responses/GetClaimStatusesRpcResponse.h"

using ClaimStatus = GetClaimStatusesRpcResponse::ClaimStatus;
using ClaimState = GetClaimStatusesRpcResponse::ClaimState;


// Test constructor with statuses vector
TEST(GetClaimStatusesRpcResponseTest, ConstructorWithStatuses)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimUUID1(std::string("11111111-1111-4111-8111-111111111111"));
    const TransactionUUID claimUUID2(std::string("22222222-2222-4222-8222-222222222222"));

    std::vector<ClaimStatus> statuses = {
        {claimUUID1, 100, ClaimState::Observing},
        {claimUUID2, 200, ClaimState::Rejected}
    };

    GetClaimStatusesRpcResponse response(transactionUUID, RpcResponseStatus::Success, statuses);

    EXPECT_EQ(response.transactionUUID(), transactionUUID);
    EXPECT_EQ(response.status(), RpcResponseStatus::Success);
    EXPECT_EQ(response.statuses().size(), 2);
    EXPECT_TRUE(response.isSuccess());
}

// Test constructor with empty statuses
TEST(GetClaimStatusesRpcResponseTest, ConstructorWithEmptyStatuses)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimStatus> statuses;

    GetClaimStatusesRpcResponse response(transactionUUID, RpcResponseStatus::Success, statuses);

    EXPECT_TRUE(response.statuses().empty());
    EXPECT_TRUE(response.isSuccess());
}

// Test statuses() getter
TEST(GetClaimStatusesRpcResponseTest, StatusesGetter)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimUUID1(std::string("11111111-1111-4111-8111-111111111111"));
    const TransactionUUID claimUUID2(std::string("22222222-2222-4222-8222-222222222222"));
    const TransactionUUID claimUUID3(std::string("33333333-3333-4333-8333-333333333333"));

    std::vector<ClaimStatus> statuses = {
        {claimUUID1, 100, ClaimState::Observing},
        {claimUUID2, 200, ClaimState::Approved},
        {claimUUID3, 300, ClaimState::Rejected}
    };

    GetClaimStatusesRpcResponse response(transactionUUID, RpcResponseStatus::Success, statuses);

    const auto& retrievedStatuses = response.statuses();
    EXPECT_EQ(retrievedStatuses.size(), 3);

    EXPECT_EQ(retrievedStatuses[0].transactionUUID, claimUUID1);
    EXPECT_EQ(retrievedStatuses[0].maxClaimBlockNumber, 100);
    EXPECT_EQ(retrievedStatuses[0].state, ClaimState::Observing);

    EXPECT_EQ(retrievedStatuses[1].transactionUUID, claimUUID2);
    EXPECT_EQ(retrievedStatuses[1].state, ClaimState::Approved);

    EXPECT_EQ(retrievedStatuses[2].transactionUUID, claimUUID3);
    EXPECT_EQ(retrievedStatuses[2].state, ClaimState::Rejected);
}

// Test method() returns RpcMethod::GetClaimStatuses
TEST(GetClaimStatusesRpcResponseTest, MethodReturnsGetClaimStatuses)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusesRpcResponse response(transactionUUID, RpcResponseStatus::Success);

    EXPECT_EQ(response.method(), RpcMethod::GetClaimStatuses);
}

// Test fromJson() with some claims found
TEST(GetClaimStatusesRpcResponseTest, FromJsonWithSomeClaimsFound)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {
            {"statuses", {
                {{"transaction_uuid", "11111111-1111-4111-8111-111111111111"}, {"max_claim_block_number", 100}, {"state", "observing"}},
                {{"transaction_uuid", "22222222-2222-4222-8222-222222222222"}, {"max_claim_block_number", 200}, {"state", "rejected"}}
            }}
        }},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusesRpcResponse::fromJson(
        transactionUUID, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->statuses().size(), 2);

    EXPECT_EQ(response->statuses()[0].state, ClaimState::Observing);
    EXPECT_EQ(response->statuses()[1].state, ClaimState::Rejected);
}

// Test fromJson() with no claims found
TEST(GetClaimStatusesRpcResponseTest, FromJsonWithNoClaimsFound)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"statuses", json::array()}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusesRpcResponse::fromJson(
        transactionUUID, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_TRUE(response->statuses().empty());
}

// Test fromJson() with transport error
TEST(GetClaimStatusesRpcResponseTest, FromJsonWithTransportError)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = json::object();
    const std::string errorMessage = "Connection timeout";

    auto response = GetClaimStatusesRpcResponse::fromJson(
        transactionUUID, RpcResponseStatus::Timeout, responseJson, errorMessage);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::Timeout);
    EXPECT_TRUE(response->statuses().empty());
    EXPECT_EQ(response->errorMessage(), errorMessage);
}

// Test fromJson() with invalid JSON structure
TEST(GetClaimStatusesRpcResponseTest, FromJsonWithInvalidStructure)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    // Missing "statuses" field
    json responseJson = {
        {"result", json::object()},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusesRpcResponse::fromJson(
        transactionUUID, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::ParseError);
}

// Test Shared typedef
TEST(GetClaimStatusesRpcResponseTest, SharedTypedefWorks)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusesRpcResponse::Shared response = std::make_shared<GetClaimStatusesRpcResponse>(
        transactionUUID, RpcResponseStatus::Success);

    EXPECT_NE(response, nullptr);
    EXPECT_EQ(response->method(), RpcMethod::GetClaimStatuses);
}

