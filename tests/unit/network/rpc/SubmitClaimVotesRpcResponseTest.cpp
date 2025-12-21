#include <gtest/gtest.h>

#include "core/network/rpc/responses/SubmitClaimVotesRpcResponse.h"


// Test constructor with success=true
TEST(SubmitClaimVotesRpcResponseTest, ConstructorWithSuccessTrue)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string message = "votes submitted successfully";

    SubmitClaimVotesRpcResponse response(uuid, RpcResponseStatus::Success, true, message);

    EXPECT_EQ(response.transactionUUID(), uuid);
    EXPECT_EQ(response.status(), RpcResponseStatus::Success);
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.message(), message);
    EXPECT_TRUE(response.isSuccess());
}

// Test constructor with success=false
TEST(SubmitClaimVotesRpcResponseTest, ConstructorWithSuccessFalse)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string message = "votes count mismatch";

    SubmitClaimVotesRpcResponse response(uuid, RpcResponseStatus::Success, false, message);

    EXPECT_FALSE(response.success());
    EXPECT_EQ(response.message(), message);
    EXPECT_TRUE(response.isSuccess());
}

// Test success() getter
TEST(SubmitClaimVotesRpcResponseTest, SuccessGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    SubmitClaimVotesRpcResponse successResponse(uuid, RpcResponseStatus::Success, true, "ok");
    SubmitClaimVotesRpcResponse failResponse(uuid, RpcResponseStatus::Success, false, "failed");

    EXPECT_TRUE(successResponse.success());
    EXPECT_FALSE(failResponse.success());
}

// Test message() getter
TEST(SubmitClaimVotesRpcResponseTest, MessageGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string expectedMessage = "custom message";

    SubmitClaimVotesRpcResponse response(uuid, RpcResponseStatus::Success, true, expectedMessage);

    EXPECT_EQ(response.message(), expectedMessage);
}

// Test method() returns RpcMethod::SubmitClaimVotes
TEST(SubmitClaimVotesRpcResponseTest, MethodReturnsSubmitClaimVotes)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    SubmitClaimVotesRpcResponse response(uuid, RpcResponseStatus::Success, true, "ok");

    EXPECT_EQ(response.method(), RpcMethod::SubmitClaimVotes);
}

// Test fromJson() with success response
TEST(SubmitClaimVotesRpcResponseTest, FromJsonWithSuccessResponse)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", true}, {"message", "votes submitted successfully"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = SubmitClaimVotesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_TRUE(response->success());
    EXPECT_EQ(response->message(), "votes submitted successfully");
}

// Test fromJson() with failure response (votes count mismatch)
TEST(SubmitClaimVotesRpcResponseTest, FromJsonWithVotesCountMismatch)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", false}, {"message", "votes count (2) does not match participants count (3)"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = SubmitClaimVotesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_FALSE(response->success());
    EXPECT_EQ(response->message(), "votes count (2) does not match participants count (3)");
}

// Test fromJson() with claim not found
TEST(SubmitClaimVotesRpcResponseTest, FromJsonWithClaimNotFound)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", false}, {"message", "claim does not exist"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = SubmitClaimVotesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_FALSE(response->success());
}

// Test fromJson() with transport error
TEST(SubmitClaimVotesRpcResponseTest, FromJsonWithTransportError)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = json::object();
    const std::string errorMessage = "Connection refused";

    auto response = SubmitClaimVotesRpcResponse::fromJson(
        uuid, RpcResponseStatus::NetworkError, responseJson, errorMessage);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::NetworkError);
    EXPECT_EQ(response->errorMessage(), errorMessage);
}

// Test Shared typedef
TEST(SubmitClaimVotesRpcResponseTest, SharedTypedefWorks)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    SubmitClaimVotesRpcResponse::Shared response = std::make_shared<SubmitClaimVotesRpcResponse>(
        uuid, RpcResponseStatus::Success, true, "ok");

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->success());
}

