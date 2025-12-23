#include <gtest/gtest.h>

#include "core/network/rpc/requests/GetClaimStatusesRpcRequest.h"
#include "core/crypto/sphincskeys.h"
#include "core/crypto/sphincsscheme.h"

using ClaimInfo = GetClaimStatusesRpcRequest::ClaimInfo;
using namespace crypto::sphincs;


// Test constructor with claims vector
TEST(GetClaimStatusesRpcRequestTest, ConstructorWithClaims)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimUUID1(std::string("11111111-1111-4111-8111-111111111111"));
    const TransactionUUID claimUUID2(std::string("22222222-2222-4222-8222-222222222222"));
    const TransactionUUID claimUUID3(std::string("33333333-3333-4333-8333-333333333333"));

    std::vector<ClaimInfo> claims = {
        {claimUUID1, 100},
        {claimUUID2, 200},
        {claimUUID3, 300}
    };

    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    EXPECT_EQ(request.transactionUUID(), transactionUUID);
    EXPECT_EQ(request.claims().size(), 3);
}

// Test constructor with empty claims
TEST(GetClaimStatusesRpcRequestTest, ConstructorWithEmptyClaims)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;

    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    EXPECT_TRUE(request.claims().empty());
}

// Test claims() getter
TEST(GetClaimStatusesRpcRequestTest, ClaimsGetter)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimUUID1(std::string("11111111-1111-4111-8111-111111111111"));
    const TransactionUUID claimUUID2(std::string("22222222-2222-4222-8222-222222222222"));

    std::vector<ClaimInfo> claims = {
        {claimUUID1, 100},
        {claimUUID2, 200}
    };

    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    const auto& retrievedClaims = request.claims();
    EXPECT_EQ(retrievedClaims.size(), 2);
    EXPECT_EQ(retrievedClaims[0].transactionUUID, claimUUID1);
    EXPECT_EQ(retrievedClaims[0].maxClaimBlockNumber, 100);
    EXPECT_EQ(retrievedClaims[1].transactionUUID, claimUUID2);
    EXPECT_EQ(retrievedClaims[1].maxClaimBlockNumber, 200);
}

// Test method() returns RpcMethod::GetClaimStatuses
TEST(GetClaimStatusesRpcRequestTest, MethodReturnsGetClaimStatuses)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;

    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    EXPECT_EQ(request.method(), RpcMethod::GetClaimStatuses);
}

// Test toJson() produces valid JSON-RPC payload
TEST(GetClaimStatusesRpcRequestTest, ToJsonProducesValidPayload)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    const TransactionUUID claimUUID1(std::string("11111111-1111-4111-8111-111111111111"));
    const TransactionUUID claimUUID2(std::string("22222222-2222-4222-8222-222222222222"));

    std::vector<ClaimInfo> claims = {
        {claimUUID1, 100},
        {claimUUID2, 200}
    };

    GetClaimStatusesRpcRequest request(transactionUUID, claims);
    json jsonPayload = request.toJson();

    EXPECT_EQ(jsonPayload["method"], "RPCService.GetClaimStatuses");
    EXPECT_EQ(jsonPayload["id"], 1);
    EXPECT_TRUE(jsonPayload["params"].is_array());
    EXPECT_EQ(jsonPayload["params"].size(), 1);

    const auto& params = jsonPayload["params"][0];
    EXPECT_TRUE(params.contains("claims"));
    EXPECT_TRUE(params["claims"].is_array());
    EXPECT_EQ(params["claims"].size(), 2);

    for (const auto& claim : params["claims"]) {
        EXPECT_TRUE(claim.contains("transaction_uuid"));
        EXPECT_TRUE(claim.contains("max_claim_block_number"));
    }
}

// Test toJson() with empty claims
TEST(GetClaimStatusesRpcRequestTest, ToJsonWithEmptyClaims)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;

    GetClaimStatusesRpcRequest request(transactionUUID, claims);
    json jsonPayload = request.toJson();

    const auto& params = jsonPayload["params"][0];
    EXPECT_TRUE(params["claims"].is_array());
    EXPECT_TRUE(params["claims"].empty());
}

// Test Shared typedef
TEST(GetClaimStatusesRpcRequestTest, SharedTypedefWorks)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;

    GetClaimStatusesRpcRequest::Shared request = std::make_shared<GetClaimStatusesRpcRequest>(
        transactionUUID, claims);

    EXPECT_NE(request, nullptr);
    EXPECT_EQ(request->method(), RpcMethod::GetClaimStatuses);
}

// Task 15-06: Test public key getter/setter
TEST(GetClaimStatusesRpcRequestTest, PublicKeyGetterSetter)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;
    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    // Create test public key with known data
    std::vector<byte_t> keyData(PublicKey::keySize(), 0xAB);
    auto testPublicKey = std::make_shared<PublicKey>(keyData.data());

    request.setPublicKey(testPublicKey);
    EXPECT_EQ(request.publicKey(), testPublicKey);
}

// Task 15-06: Test signature getter/setter
TEST(GetClaimStatusesRpcRequestTest, SignatureGetterSetter)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;
    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    // Create test signature with known data
    std::vector<byte_t> sigData(Signature::signatureSize(), 0xCD);
    auto testSignature = std::make_shared<Signature>(sigData.data());

    request.setSignature(testSignature);
    EXPECT_EQ(request.signature(), testSignature);
}

// Task 15-06: Test JSON serialization includes public_key field
TEST(GetClaimStatusesRpcRequestTest, JsonSerializationIncludesPublicKey)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;
    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    std::vector<byte_t> keyData(PublicKey::keySize(), 0xAB);
    auto testPublicKey = std::make_shared<PublicKey>(keyData.data());
    request.setPublicKey(testPublicKey);

    auto jsonPayload = request.toJson();
    const auto& params = jsonPayload["params"][0];

    EXPECT_TRUE(params.contains("public_key"));
    EXPECT_FALSE(params["public_key"].get<std::string>().empty());
}

// Task 15-06: Test JSON serialization includes signature field
TEST(GetClaimStatusesRpcRequestTest, JsonSerializationIncludesSignature)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;
    GetClaimStatusesRpcRequest request(transactionUUID, claims);

    std::vector<byte_t> sigData(Signature::signatureSize(), 0xCD);
    auto testSignature = std::make_shared<Signature>(sigData.data());
    request.setSignature(testSignature);

    auto jsonPayload = request.toJson();
    const auto& params = jsonPayload["params"][0];

    EXPECT_TRUE(params.contains("signature"));
    EXPECT_FALSE(params["signature"].get<std::string>().empty());
}

// Task 15-06: Test null public key serializes as empty string
TEST(GetClaimStatusesRpcRequestTest, NullPublicKeySerializesEmpty)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;
    GetClaimStatusesRpcRequest request(transactionUUID, claims);
    // Don't set public key - it should be nullptr

    auto jsonPayload = request.toJson();
    const auto& params = jsonPayload["params"][0];

    EXPECT_TRUE(params["public_key"].get<std::string>().empty());
}

// Task 15-06: Test null signature serializes as empty string
TEST(GetClaimStatusesRpcRequestTest, NullSignatureSerializesEmpty)
{
    const TransactionUUID transactionUUID(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    std::vector<ClaimInfo> claims;
    GetClaimStatusesRpcRequest request(transactionUUID, claims);
    // Don't set signature - it should be nullptr

    auto jsonPayload = request.toJson();
    const auto& params = jsonPayload["params"][0];

    EXPECT_TRUE(params["signature"].get<std::string>().empty());
}

