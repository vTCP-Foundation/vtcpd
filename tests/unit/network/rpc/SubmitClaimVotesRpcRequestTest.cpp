#include <gtest/gtest.h>
#include <sodium.h>

#include "core/network/rpc/requests/SubmitClaimVotesRpcRequest.h"
#include "core/crypto/sphincskeys.h"
#include "core/crypto/sphincsscheme.h"

using namespace crypto;


class SubmitClaimVotesRpcRequestTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (sodium_init() < 0) {
            FAIL() << "Failed to initialize sodium library";
        }

        transactionUUID = TransactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
        claimTransactionUUID = TransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));
        maxClaimBlockNumber = 2000;

        // Generate test signatures
        votes[1] = std::make_shared<sphincs::Signature>();
        votes[2] = std::make_shared<sphincs::Signature>();
        votes[3] = std::make_shared<sphincs::Signature>();

        auto keyPair = sphincs::util::generateKeyPair();
        publicKey = keyPair.second;
        signature = std::make_shared<sphincs::Signature>();
    }

    TransactionUUID transactionUUID;
    TransactionUUID claimTransactionUUID;
    BlockNumber maxClaimBlockNumber;
    std::map<PaymentNodeID, sphincs::Signature::Shared> votes;
    sphincs::PublicKey::Shared publicKey;
    sphincs::Signature::Shared signature;
};


// Test constructor stores all fields
TEST_F(SubmitClaimVotesRpcRequestTest, ConstructorStoresAllFields)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_EQ(request.transactionUUID(), transactionUUID);
    EXPECT_EQ(request.claimTransactionUUID(), claimTransactionUUID);
    EXPECT_EQ(request.maxClaimBlockNumber(), maxClaimBlockNumber);
    EXPECT_EQ(request.votes().size(), votes.size());
    EXPECT_EQ(request.publicKey(), publicKey);
    EXPECT_EQ(request.signature(), signature);
}

// Test claimTransactionUUID() getter
TEST_F(SubmitClaimVotesRpcRequestTest, ClaimTransactionUUIDGetter)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_EQ(request.claimTransactionUUID(), claimTransactionUUID);
}

// Test maxClaimBlockNumber() getter
TEST_F(SubmitClaimVotesRpcRequestTest, MaxClaimBlockNumberGetter)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_EQ(request.maxClaimBlockNumber(), maxClaimBlockNumber);
}

// Test votes() getter
TEST_F(SubmitClaimVotesRpcRequestTest, VotesGetter)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    const auto& retrievedVotes = request.votes();
    EXPECT_EQ(retrievedVotes.size(), 3);
    EXPECT_NE(retrievedVotes.find(1), retrievedVotes.end());
    EXPECT_NE(retrievedVotes.find(2), retrievedVotes.end());
    EXPECT_NE(retrievedVotes.find(3), retrievedVotes.end());
}

// Test publicKey() getter
TEST_F(SubmitClaimVotesRpcRequestTest, PublicKeyGetter)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_EQ(request.publicKey(), publicKey);
}

// Test signature() getter
TEST_F(SubmitClaimVotesRpcRequestTest, SignatureGetter)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_EQ(request.signature(), signature);
}

// Test method() returns RpcMethod::SubmitClaimVotes
TEST_F(SubmitClaimVotesRpcRequestTest, MethodReturnsSubmitClaimVotes)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_EQ(request.method(), RpcMethod::SubmitClaimVotes);
}

// Test toJson() produces valid JSON-RPC payload
TEST_F(SubmitClaimVotesRpcRequestTest, ToJsonProducesValidPayload)
{
    SubmitClaimVotesRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    json jsonPayload = request.toJson();

    EXPECT_EQ(jsonPayload["method"], "RPCService.SubmitClaimVotes");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];
    EXPECT_TRUE(params.contains("transaction_uuid"));
    EXPECT_TRUE(params.contains("max_claim_block_number"));
    EXPECT_TRUE(params.contains("votes"));
    EXPECT_TRUE(params.contains("public_key"));
    EXPECT_TRUE(params.contains("signature"));

    // Verify votes is array with correct structure
    EXPECT_TRUE(params["votes"].is_array());
    EXPECT_EQ(params["votes"].size(), 3);

    for (const auto& vote : params["votes"]) {
        EXPECT_TRUE(vote.contains("index"));
        EXPECT_TRUE(vote.contains("signature"));
    }
}

// Test Shared typedef
TEST_F(SubmitClaimVotesRpcRequestTest, SharedTypedefWorks)
{
    SubmitClaimVotesRpcRequest::Shared request = std::make_shared<SubmitClaimVotesRpcRequest>(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        votes,
        publicKey,
        signature);

    EXPECT_NE(request, nullptr);
    EXPECT_EQ(request->method(), RpcMethod::SubmitClaimVotes);
}

