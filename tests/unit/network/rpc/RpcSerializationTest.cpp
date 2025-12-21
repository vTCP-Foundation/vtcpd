#include <gtest/gtest.h>
#include <sodium.h>
#include <set>
#include <map>

#include "core/network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "core/network/rpc/requests/AcceptClaimRpcRequest.h"
#include "core/network/rpc/requests/GetClaimStatusRpcRequest.h"
#include "core/network/rpc/requests/SubmitClaimVotesRpcRequest.h"
#include "core/network/rpc/requests/GetClaimStatusesRpcRequest.h"
#include "core/network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "core/network/rpc/responses/AcceptClaimRpcResponse.h"
#include "core/network/rpc/responses/GetClaimStatusRpcResponse.h"
#include "core/network/rpc/responses/SubmitClaimVotesRpcResponse.h"
#include "core/network/rpc/responses/GetClaimStatusesRpcResponse.h"
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

/**
 * Serialization tests verify JSON structure against observer/README.md contract.
 * Tests cover both request serialization and response deserialization.
 */


// ============================================================================
// GetBlockNumber Serialization Tests (per observer/README.md)
// ============================================================================

TEST(RpcSerializationTest, GetBlockNumberRequestMatchesContract)
{
    // Contract: {"method":"RPCService.GetCurrentBlock","params":[{}],"id":1}
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

TEST(RpcSerializationTest, GetBlockNumberResponseParses)
{
    // Contract response: {"result":{"block_number":42},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"block_number", 42}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetBlockNumberRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->blockNumber(), 42);
}


// ============================================================================
// AcceptClaim Serialization Tests (per observer/README.md)
// ============================================================================

TEST(RpcSerializationTest, AcceptClaimRequestMatchesContract)
{
    // Contract: method="RPCService.AcceptClaim"
    // params contain: transaction_uuid, max_claim_block_number, participants, public_key, signature
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("12345678-1234-4123-8123-123456789abc"));
    const BlockNumber maxClaimBlockNumber = 10;

    auto keyPair1 = sphincs::util::generateKeyPair();
    auto keyPair2 = sphincs::util::generateKeyPair();
    std::map<PaymentNodeID, sphincs::PublicKey::Shared> participants;
    participants[1] = keyPair1.second;
    participants[2] = keyPair2.second;

    auto submitterKeyPair = sphincs::util::generateKeyPair();
    auto submitterSignature = sphincs::util::signData(*submitterKeyPair.first, "claim_data");
    ASSERT_NE(submitterSignature, nullptr);

    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participants,
        submitterKeyPair.second,
        submitterSignature);

    json jsonPayload = request.toJson();

    // Verify JSON-RPC structure
    EXPECT_EQ(jsonPayload["method"], "RPCService.AcceptClaim");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];

    // Verify transaction_uuid value matches
    EXPECT_EQ(params["transaction_uuid"].get<std::string>(), "12345678-1234-4123-8123-123456789abc");

    // Verify max_claim_block_number value matches
    EXPECT_EQ(params["max_claim_block_number"].get<BlockNumber>(), maxClaimBlockNumber);

    // Verify participants array structure and values
    EXPECT_TRUE(params["participants"].is_array());
    EXPECT_EQ(params["participants"].size(), 2);

    // Collect participant indices and verify they match what we passed
    std::set<PaymentNodeID> foundIndices;
    for (const auto& participant : params["participants"]) {
        EXPECT_TRUE(participant.contains("index"));
        EXPECT_TRUE(participant.contains("public_key"));
        PaymentNodeID idx = participant["index"].get<PaymentNodeID>();
        foundIndices.insert(idx);
        // Verify public key is non-empty string
        EXPECT_FALSE(participant["public_key"].get<std::string>().empty());
    }
    EXPECT_EQ(foundIndices.count(1), 1);
    EXPECT_EQ(foundIndices.count(2), 1);

    // Verify public_key matches submitter's key
    EXPECT_EQ(params["public_key"].get<std::string>(), submitterKeyPair.second->toString());

    // Verify signature matches
    EXPECT_EQ(params["signature"].get<std::string>(), submitterSignature->toString());
}

TEST(RpcSerializationTest, AcceptClaimSuccessResponseParses)
{
    // Contract: {"result":{"success":true,"message":"claim accepted successfully"},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", true}, {"message", "claim accepted successfully"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = AcceptClaimRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_TRUE(response->success());
    EXPECT_EQ(response->message(), "claim accepted successfully");
}

TEST(RpcSerializationTest, AcceptClaimDuplicateResponseParses)
{
    // Contract: {"result":{"success":false,"message":"claim already exists..."},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", false}, {"message", "claim already exists for transaction_uuid=tx-123, max_claim_block_number=10"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = AcceptClaimRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_FALSE(response->success());
}


// ============================================================================
// GetClaimStatus Serialization Tests (per observer/README.md)
// ============================================================================

TEST(RpcSerializationTest, GetClaimStatusRequestMatchesContract)
{
    // Contract: method="RPCService.GetClaimStatus"
    // params contain: transaction_uuid, max_claim_block_number
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("12345678-1234-4123-8123-123456789abc"));
    const BlockNumber maxClaimBlockNumber = 10;

    GetClaimStatusRpcRequest request(transactionUUID, claimTransactionUUID, maxClaimBlockNumber);
    json jsonPayload = request.toJson();

    // Verify JSON-RPC structure
    EXPECT_EQ(jsonPayload["method"], "RPCService.GetClaimStatus");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];

    // Verify transaction_uuid value matches
    EXPECT_EQ(params["transaction_uuid"].get<std::string>(), "12345678-1234-4123-8123-123456789abc");

    // Verify max_claim_block_number value matches
    EXPECT_EQ(params["max_claim_block_number"].get<BlockNumber>(), maxClaimBlockNumber);
}

TEST(RpcSerializationTest, GetClaimStatusObservingResponseParses)
{
    // Contract: {"result":{"state":"observing","votes":[],"signature":""},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"state", "observing"}, {"votes", json::array()}, {"signature", ""}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Observing);
    EXPECT_TRUE(response->votes().empty());
    EXPECT_TRUE(response->rejectionSignature().empty());
}

TEST(RpcSerializationTest, GetClaimStatusApprovedResponseParses)
{
    // Contract: {"result":{"state":"approved","votes":[{"index":1,"signature":"..."}],"signature":""},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    // Generate valid sphincs signatures for proper parsing
    auto keyPair1 = sphincs::util::generateKeyPair();
    auto keyPair2 = sphincs::util::generateKeyPair();
    auto sig1 = sphincs::util::signData(*keyPair1.first, "vote_data_1");
    auto sig2 = sphincs::util::signData(*keyPair2.first, "vote_data_2");
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

    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Approved);
    // Verify votes were actually parsed (not empty due to invalid signatures)
    EXPECT_EQ(response->votes().size(), 2);
    EXPECT_NE(response->votes().find(1), response->votes().end());
    EXPECT_NE(response->votes().find(2), response->votes().end());
}

TEST(RpcSerializationTest, GetClaimStatusRejectedResponseParses)
{
    // Contract: {"result":{"state":"rejected","votes":[],"signature":"REJECTED_BY_TIMEOUT"},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"state", "rejected"}, {"votes", json::array()}, {"signature", "REJECTED_BY_TIMEOUT"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::Rejected);
    EXPECT_EQ(response->rejectionSignature(), "REJECTED_BY_TIMEOUT");
}

TEST(RpcSerializationTest, GetClaimStatusNotFoundResponseParses)
{
    // Contract: {"result":{"state":"not found","votes":[],"signature":""},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"state", "not found"}, {"votes", json::array()}, {"signature", ""}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->state(), ClaimState::NotFound);
}


// ============================================================================
// SubmitClaimVotes Serialization Tests (per observer/README.md)
// ============================================================================

TEST(RpcSerializationTest, SubmitClaimVotesRequestMatchesContract)
{
    // Contract: method="RPCService.SubmitClaimVotes"
    // params contain: transaction_uuid, max_claim_block_number, votes, public_key, signature
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimTransactionUUID(std::string("12345678-1234-4123-8123-123456789abc"));
    const BlockNumber maxClaimBlockNumber = 10;

    // Generate real signatures for votes
    auto voteKeyPair1 = sphincs::util::generateKeyPair();
    auto voteKeyPair2 = sphincs::util::generateKeyPair();
    auto voteKeyPair3 = sphincs::util::generateKeyPair();
    auto voteSig1 = sphincs::util::signData(*voteKeyPair1.first, "vote_1");
    auto voteSig2 = sphincs::util::signData(*voteKeyPair2.first, "vote_2");
    auto voteSig3 = sphincs::util::signData(*voteKeyPair3.first, "vote_3");
    ASSERT_NE(voteSig1, nullptr);
    ASSERT_NE(voteSig2, nullptr);
    ASSERT_NE(voteSig3, nullptr);

    std::map<PaymentNodeID, sphincs::Signature::Shared> votes;
    votes[1] = voteSig1;
    votes[2] = voteSig2;
    votes[3] = voteSig3;

    // Generate submitter key and signature
    auto submitterKeyPair = sphincs::util::generateKeyPair();
    auto submitterSig = sphincs::util::signData(*submitterKeyPair.first, "submission");
    ASSERT_NE(submitterSig, nullptr);

    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        submitterKeyPair.second,
        submitterSig);

    json jsonPayload = request.toJson();

    // Verify JSON-RPC structure
    EXPECT_EQ(jsonPayload["method"], "RPCService.SubmitClaimVotes");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];

    // Verify transaction_uuid value matches
    EXPECT_EQ(params["transaction_uuid"].get<std::string>(), "12345678-1234-4123-8123-123456789abc");

    // Verify max_claim_block_number value matches
    EXPECT_EQ(params["max_claim_block_number"].get<BlockNumber>(), maxClaimBlockNumber);

    // Verify votes array structure and values
    EXPECT_TRUE(params["votes"].is_array());
    EXPECT_EQ(params["votes"].size(), 3);

    // Collect vote indices and verify they match what we passed
    std::set<PaymentNodeID> foundIndices;
    std::map<PaymentNodeID, std::string> foundSignatures;
    for (const auto& vote : params["votes"]) {
        EXPECT_TRUE(vote.contains("index"));
        EXPECT_TRUE(vote.contains("signature"));
        PaymentNodeID idx = vote["index"].get<PaymentNodeID>();
        foundIndices.insert(idx);
        foundSignatures[idx] = vote["signature"].get<std::string>();
    }
    EXPECT_EQ(foundIndices.count(1), 1);
    EXPECT_EQ(foundIndices.count(2), 1);
    EXPECT_EQ(foundIndices.count(3), 1);

    // Verify signatures match what we passed
    EXPECT_EQ(foundSignatures[1], voteSig1->toString());
    EXPECT_EQ(foundSignatures[2], voteSig2->toString());
    EXPECT_EQ(foundSignatures[3], voteSig3->toString());

    // Verify public_key matches submitter's key
    EXPECT_EQ(params["public_key"].get<std::string>(), submitterKeyPair.second->toString());

    // Verify signature matches
    EXPECT_EQ(params["signature"].get<std::string>(), submitterSig->toString());
}

TEST(RpcSerializationTest, SubmitClaimVotesSuccessResponseParses)
{
    // Contract: {"result":{"success":true,"message":"votes submitted successfully"},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", true}, {"message", "votes submitted successfully"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = SubmitClaimVotesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_TRUE(response->success());
    EXPECT_EQ(response->message(), "votes submitted successfully");
}

TEST(RpcSerializationTest, SubmitClaimVotesCountMismatchResponseParses)
{
    // Contract: {"result":{"success":false,"message":"votes count (2) does not match participants count (3)"}...}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"success", false}, {"message", "votes count (2) does not match participants count (3)"}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = SubmitClaimVotesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_FALSE(response->success());
    EXPECT_EQ(response->message(), "votes count (2) does not match participants count (3)");
}


// ============================================================================
// GetClaimStatuses Serialization Tests (per observer/README.md)
// ============================================================================

TEST(RpcSerializationTest, GetClaimStatusesRequestMatchesContract)
{
    // Contract: method="RPCService.GetClaimStatuses"
    // params contain: claims array with transaction_uuid and max_claim_block_number
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimUUID1(std::string("11111111-1111-4111-8111-111111111111"));
    const TransactionUUID claimUUID2(std::string("22222222-2222-4222-8222-222222222222"));
    const BlockNumber blockNumber1 = 10;
    const BlockNumber blockNumber2 = 15;

    std::vector<GetClaimStatusesRpcRequest::ClaimInfo> claims = {
        {claimUUID1, blockNumber1},
        {claimUUID2, blockNumber2}
    };

    GetClaimStatusesRpcRequest request(transactionUUID, claims);
    json jsonPayload = request.toJson();

    // Verify JSON-RPC structure
    EXPECT_EQ(jsonPayload["method"], "RPCService.GetClaimStatuses");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];
    EXPECT_TRUE(params.contains("claims"));
    EXPECT_TRUE(params["claims"].is_array());
    EXPECT_EQ(params["claims"].size(), 2);

    // Verify claims array values match what we passed
    const auto& claimsArray = params["claims"];

    // First claim
    EXPECT_EQ(claimsArray[0]["transaction_uuid"].get<std::string>(), "11111111-1111-4111-8111-111111111111");
    EXPECT_EQ(claimsArray[0]["max_claim_block_number"].get<BlockNumber>(), blockNumber1);

    // Second claim
    EXPECT_EQ(claimsArray[1]["transaction_uuid"].get<std::string>(), "22222222-2222-4222-8222-222222222222");
    EXPECT_EQ(claimsArray[1]["max_claim_block_number"].get<BlockNumber>(), blockNumber2);
}

TEST(RpcSerializationTest, GetClaimStatusesSomeFoundResponseParses)
{
    // Contract: {"result":{"statuses":[{"transaction_uuid":"...","max_claim_block_number":10,"state":"observing"}]}...}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {
            {"statuses", {
                {{"transaction_uuid", "11111111-1111-4111-8111-111111111111"}, {"max_claim_block_number", 10}, {"state", "observing"}},
                {{"transaction_uuid", "22222222-2222-4222-8222-222222222222"}, {"max_claim_block_number", 20}, {"state", "rejected"}}
            }}
        }},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_EQ(response->statuses().size(), 2);
    EXPECT_EQ(response->statuses()[0].state, ClaimState::Observing);
    EXPECT_EQ(response->statuses()[1].state, ClaimState::Rejected);
}

TEST(RpcSerializationTest, GetClaimStatusesEmptyResponseParses)
{
    // Contract: {"result":{"statuses":[]},"error":null,"id":1}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    json responseJson = {
        {"result", {{"statuses", json::array()}}},
        {"error", nullptr},
        {"id", 1}
    };

    auto response = GetClaimStatusesRpcResponse::fromJson(
        uuid, RpcResponseStatus::Success, responseJson);

    EXPECT_TRUE(response->isSuccess());
    EXPECT_TRUE(response->statuses().empty());
}


// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(RpcSerializationTest, RpcErrorResponseHandled)
{
    // Contract: {"id":1,"result":null,"error":"error message here"}
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    // Simulate RpcError status passed from deserializeResponse
    auto response = GetBlockNumberRpcResponse::fromJson(
        uuid, RpcResponseStatus::RpcError, json::object(), "Method not found");

    EXPECT_FALSE(response->isSuccess());
    EXPECT_EQ(response->status(), RpcResponseStatus::RpcError);
    EXPECT_EQ(response->errorMessage(), "Method not found");
}

