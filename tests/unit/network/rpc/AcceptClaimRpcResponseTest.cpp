#include <gtest/gtest.h>

#include "core/network/rpc/responses/AcceptClaimRpcResponse.h"


// Test constructor with success=true
TEST(AcceptClaimRpcResponseTest, ConstructorWithSuccessTrue)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string message = "claim accepted successfully";

    AcceptClaimRpcResponse response(uuid, RpcResponseStatus::Success, true, message);

    EXPECT_EQ(response.transactionUUID(), uuid);
    EXPECT_EQ(response.status(), RpcResponseStatus::Success);
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.message(), message);
    EXPECT_TRUE(response.isSuccess());
}

// Test constructor with success=false
TEST(AcceptClaimRpcResponseTest, ConstructorWithSuccessFalse)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string message = "claim already exists";

    AcceptClaimRpcResponse response(uuid, RpcResponseStatus::Success, false, message);

    EXPECT_FALSE(response.success());
    EXPECT_EQ(response.message(), message);
    // RPC call succeeded but claim was rejected
    EXPECT_TRUE(response.isSuccess());
}

// Test success() getter
TEST(AcceptClaimRpcResponseTest, SuccessGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    AcceptClaimRpcResponse successResponse(uuid, RpcResponseStatus::Success, true, "ok");
    AcceptClaimRpcResponse failResponse(uuid, RpcResponseStatus::Success, false, "failed");

    EXPECT_TRUE(successResponse.success());
    EXPECT_FALSE(failResponse.success());
}

// Test message() getter
TEST(AcceptClaimRpcResponseTest, MessageGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string expectedMessage = "custom message";

    AcceptClaimRpcResponse response(uuid, RpcResponseStatus::Success, true, expectedMessage);

    EXPECT_EQ(response.message(), expectedMessage);
}

// Test method() returns RpcMethod::AcceptClaim
TEST(AcceptClaimRpcResponseTest, MethodReturnsAcceptClaim)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    AcceptClaimRpcResponse response(uuid, RpcResponseStatus::Success, true, "ok");

    EXPECT_EQ(response.method(), RpcMethod::AcceptClaim);
}

// Test fromJson() with success response
TEST(AcceptClaimRpcResponseTest, FromJsonWithSuccessResponse)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", true}, {"message", "claim accepted successfully"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = AcceptClaimRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_TRUE(response->success());
    EXPECT_EQ(response->message(), "claim accepted successfully");
}

// Test fromJson() with failure response
TEST(AcceptClaimRpcResponseTest, FromJsonWithFailureResponse)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", false}, {"message", "claim already exists"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = AcceptClaimRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_FALSE(response->success());
    EXPECT_EQ(response->message(), "claim already exists");
}

// Test fromJson() with transport error
TEST(AcceptClaimRpcResponseTest, FromJsonWithTransportError)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = json::object();
    const std::string errorMessage = "Network error";

    auto response = AcceptClaimRpcResponse::fromJson(
        uuid, RpcResponseStatus::NetworkError, responseJson, errorMessage);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::NetworkError);
    EXPECT_EQ(response->errorMessage(), errorMessage);
}

// Test Shared typedef
TEST(AcceptClaimRpcResponseTest, SharedTypedefWorks)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    AcceptClaimRpcResponse::Shared response = std::make_shared<AcceptClaimRpcResponse>(
        uuid, RpcResponseStatus::Success, true, "ok");

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->success());
}

