#include <gtest/gtest.h>
#include <sodium.h>

#include "core/network/rpc/requests/AcceptClaimRpcRequest.h"
#include "core/crypto/sphincskeys.h"
#include "core/crypto/sphincsscheme.h"

using namespace crypto;


class AcceptClaimRpcRequestTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (sodium_init() < 0) {
            FAIL() << "Failed to initialize sodium library";
        }

        transactionUUID = TransactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
        claimTransactionUUID = TransactionUUID(std::string("11111111-2222-4333-8444-555555555555"));
        maxClaimBlockNumber = 1000;

        // Generate test sphincs keys
        auto keyPair1 = sphincs::util::generateKeyPair();
        auto keyPair2 = sphincs::util::generateKeyPair();
        auto keyPair3 = sphincs::util::generateKeyPair();

        participantsPublicKeys[1] = keyPair1.second;
        participantsPublicKeys[2] = keyPair2.second;
        participantsPublicKeys[3] = keyPair3.second;

        publicKey = keyPair1.second;
        signature = std::make_shared<sphincs::Signature>();
    }

    TransactionUUID transactionUUID;
    TransactionUUID claimTransactionUUID;
    BlockNumber maxClaimBlockNumber;
    std::map<PaymentNodeID, sphincs::PublicKey::Shared> participantsPublicKeys;
    sphincs::PublicKey::Shared publicKey;
    sphincs::Signature::Shared signature;
};


// Test constructor stores all fields
TEST_F(AcceptClaimRpcRequestTest, ConstructorStoresAllFields)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_EQ(request.transactionUUID(), transactionUUID);
    EXPECT_EQ(request.claimTransactionUUID(), claimTransactionUUID);
    EXPECT_EQ(request.maxClaimBlockNumber(), maxClaimBlockNumber);
    EXPECT_EQ(request.participantsPublicKeys().size(), participantsPublicKeys.size());
    EXPECT_EQ(request.publicKey(), publicKey);
    EXPECT_EQ(request.signature(), signature);
}

// Test claimTransactionUUID() getter
TEST_F(AcceptClaimRpcRequestTest, ClaimTransactionUUIDGetter)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_EQ(request.claimTransactionUUID(), claimTransactionUUID);
}

// Test maxClaimBlockNumber() getter
TEST_F(AcceptClaimRpcRequestTest, MaxClaimBlockNumberGetter)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_EQ(request.maxClaimBlockNumber(), maxClaimBlockNumber);
}

// Test participantsPublicKeys() getter
TEST_F(AcceptClaimRpcRequestTest, ParticipantsPublicKeysGetter)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    const auto& keys = request.participantsPublicKeys();
    EXPECT_EQ(keys.size(), 3);
    EXPECT_NE(keys.find(1), keys.end());
    EXPECT_NE(keys.find(2), keys.end());
    EXPECT_NE(keys.find(3), keys.end());
}

// Test publicKey() getter
TEST_F(AcceptClaimRpcRequestTest, PublicKeyGetter)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_EQ(request.publicKey(), publicKey);
}

// Test signature() getter
TEST_F(AcceptClaimRpcRequestTest, SignatureGetter)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_EQ(request.signature(), signature);
}

// Test method() returns RpcMethod::AcceptClaim
TEST_F(AcceptClaimRpcRequestTest, MethodReturnsAcceptClaim)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_EQ(request.method(), RpcMethod::AcceptClaim);
}

// Test toJson() produces valid JSON-RPC payload per observer contract
TEST_F(AcceptClaimRpcRequestTest, ToJsonProducesValidPayload)
{
    AcceptClaimRpcRequest request(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    json jsonPayload = request.toJson();

    EXPECT_EQ(jsonPayload["method"], "RPCService.AcceptClaim");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];
    EXPECT_TRUE(params.contains("transaction_uuid"));
    EXPECT_TRUE(params.contains("max_claim_block_number"));
    EXPECT_TRUE(params.contains("participants"));
    EXPECT_TRUE(params.contains("public_key"));
    EXPECT_TRUE(params.contains("signature"));

    // Verify participants is array with correct structure
    EXPECT_TRUE(params["participants"].is_array());
    EXPECT_EQ(params["participants"].size(), 3);

    for (const auto& participant : params["participants"]) {
        EXPECT_TRUE(participant.contains("index"));
        EXPECT_TRUE(participant.contains("public_key"));
    }
}

// Test Shared typedef
TEST_F(AcceptClaimRpcRequestTest, SharedTypedefWorks)
{
    AcceptClaimRpcRequest::Shared request = std::make_shared<AcceptClaimRpcRequest>(
        transactionUUID,
        claimTransactionUUID,
        maxClaimBlockNumber,
        participantsPublicKeys,
        publicKey,
        signature);

    EXPECT_NE(request, nullptr);
    EXPECT_EQ(request->method(), RpcMethod::AcceptClaim);
}

