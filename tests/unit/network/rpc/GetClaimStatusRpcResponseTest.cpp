#include <gtest/gtest.h>
#include <sodium.h>

#include "core/network/rpc/responses/GetClaimStatusRpcResponse.h"
#include "core/crypto/sphincskeys.h"
#include "core/crypto/sphincsscheme.h"

using namespace crypto;
using ClaimState = GetClaimStatusRpcResponse::ClaimState;

namespace {
// Helper to ensure sodium is initialized
struct SodiumInit {
    SodiumInit() {
        if (sodium_init() < 0) {
            throw std::runtime_error("Failed to initialize sodium library");
        }
    }
};
static SodiumInit sodiumInit;
}


// Test constructor with NotFound state
TEST(GetClaimStatusRpcResponseTest, ConstructorWithNotFoundState)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse response(uuid, RpcResponseStatus::Success, ClaimState::NotFound);

    EXPECT_EQ(response.transactionUUID(), uuid);
    EXPECT_EQ(response.status(), RpcResponseStatus::Success);
    EXPECT_EQ(response.state(), ClaimState::NotFound);
    EXPECT_TRUE(response.votes().empty());
    EXPECT_TRUE(response.rejectionSignature().empty());
}

// Test constructor with Observing state
TEST(GetClaimStatusRpcResponseTest, ConstructorWithObservingState)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse response(uuid, RpcResponseStatus::Success, ClaimState::Observing);

    EXPECT_EQ(response.state(), ClaimState::Observing);
    EXPECT_TRUE(response.votes().empty());
    EXPECT_TRUE(response.rejectionSignature().empty());
}

// Test constructor with Approved state and votes
TEST(GetClaimStatusRpcResponseTest, ConstructorWithApprovedStateAndVotes)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::map<PaymentNodeID, sphincs::Signature::Shared> votes;
    votes[1] = std::make_shared<sphincs::Signature>();
    votes[2] = std::make_shared<sphincs::Signature>();

    GetClaimStatusRpcResponse response(uuid, RpcResponseStatus::Success, ClaimState::Approved, votes);

    EXPECT_EQ(response.state(), ClaimState::Approved);
    EXPECT_EQ(response.votes().size(), 2);
    EXPECT_NE(response.votes().find(1), response.votes().end());
    EXPECT_NE(response.votes().find(2), response.votes().end());
    EXPECT_TRUE(response.rejectionSignature().empty());
}

// Test constructor with Rejected state and rejection signature
TEST(GetClaimStatusRpcResponseTest, ConstructorWithRejectedStateAndSignature)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const std::string rejectionSignature = "REJECTED_BY_TIMEOUT";

    GetClaimStatusRpcResponse response(
        uuid, RpcResponseStatus::Success, ClaimState::Rejected,
        std::map<PaymentNodeID, sphincs::Signature::Shared>(), rejectionSignature);

    EXPECT_EQ(response.state(), ClaimState::Rejected);
    EXPECT_TRUE(response.votes().empty());
    EXPECT_EQ(response.rejectionSignature(), rejectionSignature);
}

// Test state() getter
TEST(GetClaimStatusRpcResponseTest, StateGetter)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse response(uuid, RpcResponseStatus::Success, ClaimState::Approved);

    EXPECT_EQ(response.state(), ClaimState::Approved);
}

// Test votes() getter - empty for non-Approved states
TEST(GetClaimStatusRpcResponseTest, VotesGetterEmptyForNonApproved)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse notFoundResponse(uuid, RpcResponseStatus::Success, ClaimState::NotFound);
    GetClaimStatusRpcResponse observingResponse(uuid, RpcResponseStatus::Success, ClaimState::Observing);
    GetClaimStatusRpcResponse rejectedResponse(uuid, RpcResponseStatus::Success, ClaimState::Rejected);

    EXPECT_TRUE(notFoundResponse.votes().empty());
    EXPECT_TRUE(observingResponse.votes().empty());
    EXPECT_TRUE(rejectedResponse.votes().empty());
}

// Test rejectionSignature() getter - empty for non-Rejected states
TEST(GetClaimStatusRpcResponseTest, RejectionSignatureGetterEmptyForNonRejected)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse notFoundResponse(uuid, RpcResponseStatus::Success, ClaimState::NotFound);
    GetClaimStatusRpcResponse observingResponse(uuid, RpcResponseStatus::Success, ClaimState::Observing);
    GetClaimStatusRpcResponse approvedResponse(uuid, RpcResponseStatus::Success, ClaimState::Approved);

    EXPECT_TRUE(notFoundResponse.rejectionSignature().empty());
    EXPECT_TRUE(observingResponse.rejectionSignature().empty());
    EXPECT_TRUE(approvedResponse.rejectionSignature().empty());
}

// Test method() returns RpcMethod::GetClaimStatus
TEST(GetClaimStatusRpcResponseTest, MethodReturnsGetClaimStatus)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse response(uuid, RpcResponseStatus::Success, ClaimState::Observing);

    EXPECT_EQ(response.method(), RpcMethod::GetClaimStatus);
}

// Test tryParseState() with valid states
TEST(GetClaimStatusRpcResponseTest, TryParseStateValidStates)
{
    ClaimState state;

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("not found", state));
    EXPECT_EQ(state, ClaimState::NotFound);

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("observing", state));
    EXPECT_EQ(state, ClaimState::Observing);

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("approved", state));
    EXPECT_EQ(state, ClaimState::Approved);

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("rejected", state));
    EXPECT_EQ(state, ClaimState::Rejected);
}

// Test tryParseState() is case-insensitive
TEST(GetClaimStatusRpcResponseTest, TryParseStateCaseInsensitive)
{
    ClaimState state;

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("NOT FOUND", state));
    EXPECT_EQ(state, ClaimState::NotFound);

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("OBSERVING", state));
    EXPECT_EQ(state, ClaimState::Observing);

    EXPECT_TRUE(GetClaimStatusRpcResponse::tryParseState("Approved", state));
    EXPECT_EQ(state, ClaimState::Approved);
}

// Test tryParseState() with invalid state
TEST(GetClaimStatusRpcResponseTest, TryParseStateInvalidState)
{
    ClaimState state;

    EXPECT_FALSE(GetClaimStatusRpcResponse::tryParseState("invalid", state));
    EXPECT_FALSE(GetClaimStatusRpcResponse::tryParseState("", state));
    EXPECT_FALSE(GetClaimStatusRpcResponse::tryParseState("pending", state));
}

// Test fromJson() with observing state
TEST(GetClaimStatusRpcResponseTest, FromJsonWithObservingState)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"state", "observing"}, {"votes", json::array()}, {"signature", ""}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Observing);
    EXPECT_TRUE(response->votes().empty());
}

// Test fromJson() with approved state and valid votes
TEST(GetClaimStatusRpcResponseTest, FromJsonWithApprovedStateAndVotes)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    // Generate real sphincs signatures for valid parsing
    auto keyPair1 = sphincs::util::generateKeyPair();
    auto keyPair2 = sphincs::util::generateKeyPair();
    auto sig1 = sphincs::util::signData(*keyPair1.first, "test_data_1");
    auto sig2 = sphincs::util::signData(*keyPair2.first, "test_data_2");

    ASSERT_NE(sig1, nullptr);
    ASSERT_NE(sig2, nullptr);

    json responseJson = {
        {"result", {
            {"state", "approved"},
            {"votes", {
                {{"index", 1}, {"signature", sig1->toString()}},
                {{"index", 2}, {"signature", sig2->toString()}}
            }},
            {"signature", ""}
        }},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Approved);
    // Verify votes were actually parsed
    EXPECT_EQ(response->votes().size(), 2);
    EXPECT_NE(response->votes().find(1), response->votes().end());
    EXPECT_NE(response->votes().find(2), response->votes().end());
    // Verify vote signatures are valid
    EXPECT_TRUE(response->votes().at(1)->isValid());
    EXPECT_TRUE(response->votes().at(2)->isValid());
}

// Test fromJson() with approved state but invalid signatures (edge case)
TEST(GetClaimStatusRpcResponseTest, FromJsonWithApprovedStateInvalidSignatures)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    // Invalid short base64 strings should be rejected by parseVotes
    json responseJson = {
        {"result", {
            {"state", "approved"},
            {"votes", {
                {{"index", 1}, {"signature", "c2lnbmF0dXJlMQ=="}},
                {{"index", 2}, {"signature", "c2lnbmF0dXJlMg=="}}
            }},
            {"signature", ""}
        }},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Approved);
    // Invalid signatures should result in empty votes map
    EXPECT_TRUE(response->votes().empty());
}

// Test fromJson() with rejected state and signature
TEST(GetClaimStatusRpcResponseTest, FromJsonWithRejectedStateAndSignature)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"state", "rejected"}, {"votes", json::array()}, {"signature", "REJECTED_BY_TIMEOUT"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Rejected);
    EXPECT_EQ(response->rejectionSignature(), "REJECTED_BY_TIMEOUT");
}

// Test fromJson() with not found state
TEST(GetClaimStatusRpcResponseTest, FromJsonWithNotFoundState)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"state", "not found"}, {"votes", json::array()}, {"signature", ""}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_NE(response, nullptr);
    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::NotFound);
}

// Test fromJson() with transport error
TEST(GetClaimStatusRpcResponseTest, FromJsonWithTransportError)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = json::object();
    const std::string errorMessage = "Connection timeout";

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Timeout, responseJson, errorMessage);

    EXPECT_NE(response, nullptr);
    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::Timeout);
    EXPECT_EQ(response->errorMessage(), errorMessage);
}

// Test Shared typedef
TEST(GetClaimStatusRpcResponseTest, SharedTypedefWorks)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    GetClaimStatusRpcResponse::Shared response = std::make_shared<GetClaimStatusRpcResponse>(
        uuid, RpcResponseStatus::Success, ClaimState::Approved);

    EXPECT_NE(response, nullptr);
    EXPECT_EQ(response->state(), ClaimState::Approved);
}

