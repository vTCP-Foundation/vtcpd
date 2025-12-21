#include <gtest/gtest.h>

#include "core/transactions/transactions/base/BaseTransaction.h"
#include "core/network/rpc/RpcResponse.h"
#include "core/network/rpc/RpcResponseStatus.h"
#include "core/network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "core/network/rpc/responses/AcceptClaimRpcResponse.h"
#include "core/logger/Logger.h"

#include <memory>
#include <vector>

using namespace std;


/**
 * Concrete test transaction class for testing BaseTransaction RPC queue functionality.
 * This minimal implementation allows testing the mRpcContext queue without requiring
 * full transaction infrastructure.
 */
class TestTransaction : public BaseTransaction
{
public:
    TestTransaction(
        const TransactionUUID &uuid,
        Logger &logger) :
        BaseTransaction(
            TransactionType::NoEquivalentType,
            uuid,
            logger)
    {}

    TransactionResult::SharedConst run() override
    {
        return resultDone();
    }

    const string logHeader() const override
    {
        return "TestTransaction";
    }

    // Expose protected method for testing
    template<typename ResponseType>
    shared_ptr<ResponseType> testPopRpcResponse()
    {
        return popRpcResponse<ResponseType>();
    }
};


class BaseTransactionRpcQueueTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mLogger = make_unique<Logger>();
        mTransactionUUID = TransactionUUID();
        mTransaction = make_shared<TestTransaction>(mTransactionUUID, *mLogger);
    }

    void TearDown() override
    {
        mTransaction.reset();
        mLogger.reset();
    }

    RpcResponse::Shared createTestResponse(RpcResponseStatus status = RpcResponseStatus::Success)
    {
        return make_shared<GetBlockNumberRpcResponse>(
            mTransactionUUID,
            status,
            42);
    }

    RpcResponse::Shared createTestResponseWithBlockNumber(BlockNumber blockNumber)
    {
        return make_shared<GetBlockNumberRpcResponse>(
            mTransactionUUID,
            RpcResponseStatus::Success,
            blockNumber);
    }

protected:
    unique_ptr<Logger> mLogger;
    TransactionUUID mTransactionUUID;
    shared_ptr<TestTransaction> mTransaction;
};


// ============================================================================
// Test: kMaxRpcResponsesPerTransaction constant equals 50
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, QueueLimitConstantIs50)
{
    EXPECT_EQ(BaseTransaction::kMaxRpcResponsesPerTransaction, 50);
}


// ============================================================================
// Test: hasRpcResponse returns false for empty queue
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, HasRpcResponseReturnsFalseForEmptyQueue)
{
    EXPECT_FALSE(mTransaction->hasRpcResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 0);
}


// ============================================================================
// Test: pushRpcResponse returns true and hasRpcResponse returns true after push
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, PushRpcResponseSucceedsAndUpdatesHasRpcResponse)
{
    auto response = createTestResponse();

    bool result = mTransaction->pushRpcResponse(response);

    EXPECT_TRUE(result);
    EXPECT_TRUE(mTransaction->hasRpcResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 1);
}


// ============================================================================
// Test: Multiple pushes work correctly up to limit
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, MultiplePushesUpToLimit)
{
    for (size_t i = 0; i < BaseTransaction::kMaxRpcResponsesPerTransaction; ++i) {
        auto response = createTestResponseWithBlockNumber(static_cast<BlockNumber>(i));
        bool result = mTransaction->pushRpcResponse(response);
        EXPECT_TRUE(result) << "Push failed at index " << i;
    }

    EXPECT_TRUE(mTransaction->hasRpcResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), BaseTransaction::kMaxRpcResponsesPerTransaction);
}


// ============================================================================
// Test: 51st push returns false (queue overflow)
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, PushReturnsFalseOnQueueOverflow)
{
    // Fill the queue to capacity
    for (size_t i = 0; i < BaseTransaction::kMaxRpcResponsesPerTransaction; ++i) {
        auto response = createTestResponseWithBlockNumber(static_cast<BlockNumber>(i));
        ASSERT_TRUE(mTransaction->pushRpcResponse(response));
    }

    // 51st push should return false
    auto overflowResponse = createTestResponseWithBlockNumber(999);
    bool result = mTransaction->pushRpcResponse(overflowResponse);

    EXPECT_FALSE(result);
    // Queue size should remain at limit
    EXPECT_EQ(mTransaction->rpcResponsesCount(), BaseTransaction::kMaxRpcResponsesPerTransaction);
}


// ============================================================================
// Test: FIFO order is preserved (popRpcResponse returns in insertion order)
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, FIFOOrderPreserved)
{
    const size_t testCount = 5;

    // Push responses with distinct block numbers
    for (size_t i = 0; i < testCount; ++i) {
        auto response = createTestResponseWithBlockNumber(static_cast<BlockNumber>(i * 10));
        mTransaction->pushRpcResponse(response);
    }

    // Pop and verify order
    for (size_t i = 0; i < testCount; ++i) {
        ASSERT_TRUE(mTransaction->hasRpcResponse()) << "Queue unexpectedly empty at index " << i;
        auto popped = mTransaction->testPopRpcResponse<GetBlockNumberRpcResponse>();
        ASSERT_NE(popped, nullptr);
        EXPECT_EQ(popped->blockNumber(), static_cast<BlockNumber>(i * 10))
            << "FIFO order violated at index " << i;
    }

    EXPECT_FALSE(mTransaction->hasRpcResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 0);
}


// ============================================================================
// Test: hasRpcResponse becomes false after popping all elements
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, HasRpcResponseFalseAfterPoppingAll)
{
    // Push 3 responses
    for (int i = 0; i < 3; ++i) {
        mTransaction->pushRpcResponse(createTestResponse());
    }

    EXPECT_TRUE(mTransaction->hasRpcResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 3);

    // Pop all
    for (int i = 0; i < 3; ++i) {
        mTransaction->testPopRpcResponse<GetBlockNumberRpcResponse>();
    }

    EXPECT_FALSE(mTransaction->hasRpcResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 0);
}


// ============================================================================
// Test: Overflow with RpcError response clears queue and stores error
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, OverflowWithRpcErrorClearsQueueAndStoresError)
{
    // Fill the queue to capacity with Success responses
    for (size_t i = 0; i < BaseTransaction::kMaxRpcResponsesPerTransaction; ++i) {
        auto response = createTestResponseWithBlockNumber(static_cast<BlockNumber>(i));
        ASSERT_TRUE(mTransaction->pushRpcResponse(response));
    }

    EXPECT_EQ(mTransaction->rpcResponsesCount(), BaseTransaction::kMaxRpcResponsesPerTransaction);

    // Push an RpcError response when queue is full - this should clear and store
    auto errorResponse = make_shared<GetBlockNumberRpcResponse>(
        mTransactionUUID,
        RpcResponseStatus::RpcError,
        0,
        "RPC responses queue overflow");

    bool result = mTransaction->pushRpcResponse(errorResponse);

    // Push still returns false (overflow condition)
    EXPECT_FALSE(result);
    // But the queue should be cleared and contain only the error response
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 1);
    EXPECT_TRUE(mTransaction->hasRpcResponse());

    // Verify the stored response is the error response
    auto storedResponse = mTransaction->testPopRpcResponse<GetBlockNumberRpcResponse>();
    ASSERT_NE(storedResponse, nullptr);
    EXPECT_EQ(storedResponse->status(), RpcResponseStatus::RpcError);
    EXPECT_EQ(storedResponse->errorMessage(), "RPC responses queue overflow");
}


// ============================================================================
// Test: pushRpcResponse throws on null response
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, PushRpcResponseThrowsOnNullResponse)
{
    RpcResponse::Shared nullResponse = nullptr;

    EXPECT_THROW(mTransaction->pushRpcResponse(nullResponse), RuntimeError);
}


// ============================================================================
// Test: rpcResponsesCount reflects actual queue size
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, RpcResponsesCountReflectsQueueSize)
{
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 0);

    mTransaction->pushRpcResponse(createTestResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 1);

    mTransaction->pushRpcResponse(createTestResponse());
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 2);

    mTransaction->testPopRpcResponse<GetBlockNumberRpcResponse>();
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 1);

    mTransaction->testPopRpcResponse<GetBlockNumberRpcResponse>();
    EXPECT_EQ(mTransaction->rpcResponsesCount(), 0);
}


// ============================================================================
// Test: Different response types can be stored in the same queue
// ============================================================================
TEST_F(BaseTransactionRpcQueueTest, DifferentResponseTypesCanBeStored)
{
    auto blockNumberResponse = make_shared<GetBlockNumberRpcResponse>(
        mTransactionUUID,
        RpcResponseStatus::Success,
        100);

    auto acceptClaimResponse = make_shared<AcceptClaimRpcResponse>(
        mTransactionUUID,
        RpcResponseStatus::Success,
        true,
        "claim accepted");

    mTransaction->pushRpcResponse(blockNumberResponse);
    mTransaction->pushRpcResponse(acceptClaimResponse);

    EXPECT_EQ(mTransaction->rpcResponsesCount(), 2);

    // Pop and verify types (FIFO order)
    auto first = mTransaction->testPopRpcResponse<GetBlockNumberRpcResponse>();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->method(), RpcMethod::GetBlockNumber);
    EXPECT_EQ(first->blockNumber(), 100);

    auto second = mTransaction->testPopRpcResponse<AcceptClaimRpcResponse>();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->method(), RpcMethod::AcceptClaim);
    EXPECT_TRUE(second->success());
}
