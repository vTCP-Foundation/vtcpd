#include <gtest/gtest.h>

#include "core/network/rpc/RpcRequest.h"
#include "core/network/rpc/requests/GetBlockNumberRpcRequest.h"


// Test that base class stores transactionUUID correctly
TEST(RpcRequestTest, ConstructorStoresTransactionUUID)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    // Use concrete implementation to test base class behavior
    GetBlockNumberRpcRequest request(uuid);

    EXPECT_EQ(request.transactionUUID(), uuid);
}

// Test transactionUUID() getter returns correct value
TEST(RpcRequestTest, TransactionUUIDGetterReturnsCorrectValue)
{
    const TransactionUUID uuid1(std::string("11111111-2222-4333-8444-555555555555"));
    const TransactionUUID uuid2(std::string("66666666-7777-4888-8999-aaaaaaaaaaaa"));

    GetBlockNumberRpcRequest request1(uuid1);
    GetBlockNumberRpcRequest request2(uuid2);

    EXPECT_EQ(request1.transactionUUID(), uuid1);
    EXPECT_EQ(request2.transactionUUID(), uuid2);
    EXPECT_NE(request1.transactionUUID(), request2.transactionUUID());
}

// Test that Shared typedef is correctly defined
TEST(RpcRequestTest, SharedTypedefIsDefined)
{
    const TransactionUUID uuid(std::string("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));

    RpcRequest::Shared sharedRequest = std::make_shared<GetBlockNumberRpcRequest>(uuid);

    EXPECT_NE(sharedRequest, nullptr);
    EXPECT_EQ(sharedRequest->transactionUUID(), uuid);
}

