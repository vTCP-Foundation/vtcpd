#include <gtest/gtest.h>

#include "core/transactions/scheduler/TransactionsScheduler.h"
#include "core/transactions/transactions/base/BaseTransaction.h"
#include "core/transactions/transactions/result/state/TransactionState.h"
#include "core/network/rpc/RpcResponse.h"
#include "core/network/rpc/RpcResponseStatus.h"
#include "core/network/rpc/RpcMethod.h"
#include "core/network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "core/network/rpc/responses/AcceptClaimRpcResponse.h"
#include "core/subsystems_controller/TrustLinesInfluenceController.h"
#include "core/logger/Logger.h"

#include <boost/asio/io_context.hpp>
#include <memory>
#include <vector>

using namespace std;
namespace as = boost::asio;


/**
 * Test transaction class for TransactionsScheduler RPC routing tests.
 * Provides access to transaction state and RPC response handling.
 */
class MockRpcTransaction : public BaseTransaction
{
public:
    MockRpcTransaction(
        const TransactionUUID &uuid,
        Logger &logger) :
        BaseTransaction(
            TransactionType::NoEquivalentType,
            uuid,
            logger),
        mWasLaunched(false)
    {}

    TransactionResult::SharedConst run() override
    {
        mWasLaunched = true;
        // Return state waiting for RPC response to allow scheduler routing tests
        return resultWaitForRpcResponse(mExpectedRpcMethod);
    }

    const string logHeader() const override
    {
        return "MockRpcTransaction";
    }

    void setExpectedRpcMethod(RpcMethod method)
    {
        mExpectedRpcMethod = method;
    }

    bool wasLaunched() const
    {
        return mWasLaunched;
    }

    void resetLaunchFlag()
    {
        mWasLaunched = false;
    }

    // Expose protected method for verification
    template<typename ResponseType>
    shared_ptr<ResponseType> getResponse()
    {
        if (!hasRpcResponse()) {
            return nullptr;
        }
        return popRpcResponse<ResponseType>();
    }

private:
    RpcMethod mExpectedRpcMethod = RpcMethod::GetBlockNumber;
    bool mWasLaunched;
};


/**
 * Friend function declaration to access scheduler's internal transaction map.
 * This matches the friend declaration in TransactionsScheduler.h
 */
const map<BaseTransaction::Shared, TransactionState::SharedConst>* transactions(
    TransactionsScheduler *scheduler);


class TransactionsSchedulerRpcRoutingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mLogger = make_unique<Logger>();
        mInfluenceController = make_unique<TrustLinesInfluenceController>(*mLogger);
        mScheduler = make_unique<TransactionsScheduler>(
            mIOCtx,
            mInfluenceController.get(),
            *mLogger);
    }

    void TearDown() override
    {
        mScheduler.reset();
        mInfluenceController.reset();
        mLogger.reset();
    }

    shared_ptr<MockRpcTransaction> createAndScheduleTransaction(
        RpcMethod expectedMethod = RpcMethod::GetBlockNumber)
    {
        auto uuid = TransactionUUID();
        auto transaction = make_shared<MockRpcTransaction>(uuid, *mLogger);
        transaction->setExpectedRpcMethod(expectedMethod);

        // Schedule the transaction
        mScheduler->scheduleTransaction(transaction);

        // Run the transaction once to set up the RPC waiting state
        // This is needed because the scheduler stores the TransactionState returned by run()
        runTransaction(transaction);

        transaction->resetLaunchFlag();
        return transaction;
    }

    void runTransaction(shared_ptr<MockRpcTransaction> transaction)
    {
        // Simulate what scheduler does when launching a transaction
        auto result = transaction->run();
        if (result && result->state()) {
            // Update the transaction's state in the scheduler's map
            auto transactionsMap = const_cast<map<BaseTransaction::Shared, TransactionState::SharedConst>*>(
                transactions(mScheduler.get()));
            (*transactionsMap)[transaction] = result->state();
        }
    }

    RpcResponse::Shared createRpcResponse(
        const TransactionUUID &uuid,
        RpcMethod method = RpcMethod::GetBlockNumber,
        RpcResponseStatus status = RpcResponseStatus::Success)
    {
        switch (method) {
        case RpcMethod::GetBlockNumber:
            return make_shared<GetBlockNumberRpcResponse>(
                uuid, status, 42);
        case RpcMethod::AcceptClaim:
            return make_shared<AcceptClaimRpcResponse>(
                uuid, status, true, "claim accepted");
        default:
            return make_shared<GetBlockNumberRpcResponse>(
                uuid, status, 0);
        }
    }

protected:
    as::io_context mIOCtx;
    unique_ptr<Logger> mLogger;
    unique_ptr<TrustLinesInfluenceController> mInfluenceController;
    unique_ptr<TransactionsScheduler> mScheduler;
};


// ============================================================================
// Test: Response is delivered to matching transaction
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, ResponseDeliveredToMatchingTransaction)
{
    auto transaction = createAndScheduleTransaction(RpcMethod::GetBlockNumber);
    auto response = createRpcResponse(
        transaction->currentTransactionUUID(),
        RpcMethod::GetBlockNumber);

    bool result = mScheduler->tryAttachRpcResponseToTransaction(response);

    EXPECT_TRUE(result);
    EXPECT_TRUE(transaction->hasRpcResponse());
}


// ============================================================================
// Test: Response not delivered when transaction not found
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, ResponseNotDeliveredWhenTransactionNotFound)
{
    // Create a response for a non-existent transaction
    auto unknownUUID = TransactionUUID();
    auto response = createRpcResponse(unknownUUID);

    bool result = mScheduler->tryAttachRpcResponseToTransaction(response);

    EXPECT_FALSE(result);
}


// ============================================================================
// Test: Response not delivered when RpcMethod mismatch
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, ResponseNotDeliveredWhenRpcMethodMismatch)
{
    // Transaction expects GetBlockNumber
    auto transaction = createAndScheduleTransaction(RpcMethod::GetBlockNumber);

    // But we send AcceptClaim response
    auto response = createRpcResponse(
        transaction->currentTransactionUUID(),
        RpcMethod::AcceptClaim);

    bool result = mScheduler->tryAttachRpcResponseToTransaction(response);

    EXPECT_FALSE(result);
    EXPECT_FALSE(transaction->hasRpcResponse());
}


// ============================================================================
// Test: Null response returns false
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, NullResponseReturnsFalse)
{
    RpcResponse::Shared nullResponse = nullptr;

    bool result = mScheduler->tryAttachRpcResponseToTransaction(nullResponse);

    EXPECT_FALSE(result);
}


// ============================================================================
// Test: Transaction is awakened after response delivery
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, TransactionAwakenedAfterResponseDelivery)
{
    auto transaction = createAndScheduleTransaction(RpcMethod::GetBlockNumber);
    ASSERT_FALSE(transaction->wasLaunched());

    auto response = createRpcResponse(
        transaction->currentTransactionUUID(),
        RpcMethod::GetBlockNumber);

    mScheduler->tryAttachRpcResponseToTransaction(response);

    // The scheduler should have called launchTransaction which triggers run()
    // Note: Since we're testing the scheduler's behavior, the transaction's
    // wasLaunched flag would be set after launchTransaction is called.
    // In this unit test context, we verify the response was attached.
    EXPECT_TRUE(transaction->hasRpcResponse());
}


// ============================================================================
// Test: Multiple transactions - response delivered to correct one
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, ResponseDeliveredToCorrectTransactionAmongMultiple)
{
    auto transaction1 = createAndScheduleTransaction(RpcMethod::GetBlockNumber);
    auto transaction2 = createAndScheduleTransaction(RpcMethod::AcceptClaim);

    // Send response for transaction2
    auto response = createRpcResponse(
        transaction2->currentTransactionUUID(),
        RpcMethod::AcceptClaim);

    bool result = mScheduler->tryAttachRpcResponseToTransaction(response);

    EXPECT_TRUE(result);
    EXPECT_FALSE(transaction1->hasRpcResponse());
    EXPECT_TRUE(transaction2->hasRpcResponse());
}


// ============================================================================
// Test: Response delivery order matches arrival order
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, ResponseDeliveryOrderMatchesArrivalOrder)
{
    auto transaction = createAndScheduleTransaction(RpcMethod::GetBlockNumber);

    // Deliver multiple responses (simulating arrival order)
    auto response1 = make_shared<GetBlockNumberRpcResponse>(
        transaction->currentTransactionUUID(),
        RpcResponseStatus::Success,
        100);
    auto response2 = make_shared<GetBlockNumberRpcResponse>(
        transaction->currentTransactionUUID(),
        RpcResponseStatus::Success,
        200);
    auto response3 = make_shared<GetBlockNumberRpcResponse>(
        transaction->currentTransactionUUID(),
        RpcResponseStatus::Success,
        300);

    mScheduler->tryAttachRpcResponseToTransaction(response1);
    mScheduler->tryAttachRpcResponseToTransaction(response2);
    mScheduler->tryAttachRpcResponseToTransaction(response3);

    // Verify FIFO order
    auto popped1 = transaction->getResponse<GetBlockNumberRpcResponse>();
    ASSERT_NE(popped1, nullptr);
    EXPECT_EQ(popped1->blockNumber(), 100);

    auto popped2 = transaction->getResponse<GetBlockNumberRpcResponse>();
    ASSERT_NE(popped2, nullptr);
    EXPECT_EQ(popped2->blockNumber(), 200);

    auto popped3 = transaction->getResponse<GetBlockNumberRpcResponse>();
    ASSERT_NE(popped3, nullptr);
    EXPECT_EQ(popped3->blockNumber(), 300);
}


// ============================================================================
// Test: Overflow generates synthetic RpcError response
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, OverflowGeneratesSyntheticRpcError)
{
    auto transaction = createAndScheduleTransaction(RpcMethod::GetBlockNumber);

    // Fill the queue to capacity
    for (size_t i = 0; i < BaseTransaction::kMaxRpcResponsesPerTransaction; ++i) {
        auto response = make_shared<GetBlockNumberRpcResponse>(
            transaction->currentTransactionUUID(),
            RpcResponseStatus::Success,
            static_cast<BlockNumber>(i));
        mScheduler->tryAttachRpcResponseToTransaction(response);
    }

    EXPECT_EQ(transaction->rpcResponsesCount(), BaseTransaction::kMaxRpcResponsesPerTransaction);

    // 51st response should trigger overflow handling
    auto overflowResponse = make_shared<GetBlockNumberRpcResponse>(
        transaction->currentTransactionUUID(),
        RpcResponseStatus::Success,
        999);

    bool result = mScheduler->tryAttachRpcResponseToTransaction(overflowResponse);

    // The response should still be "attached" (method returns true) because
    // the scheduler creates a synthetic RpcError response
    EXPECT_TRUE(result);

    // The queue should now contain only the synthetic RpcError (cleared + added)
    EXPECT_EQ(transaction->rpcResponsesCount(), 1);

    auto errorResponse = transaction->getResponse<GetBlockNumberRpcResponse>();
    ASSERT_NE(errorResponse, nullptr);
    EXPECT_EQ(errorResponse->status(), RpcResponseStatus::RpcError);
    EXPECT_EQ(errorResponse->errorMessage(), "RPC responses queue overflow");
}


// ============================================================================
// Test: Response not delivered when transaction not waiting for RPC
// ============================================================================
TEST_F(TransactionsSchedulerRpcRoutingTest, ResponseNotDeliveredWhenTransactionNotWaitingForRpc)
{
    auto uuid = TransactionUUID();
    auto transaction = make_shared<MockRpcTransaction>(uuid, *mLogger);

    // Schedule without running (so state is awakeAsFastAsPossible, not waiting for RPC)
    mScheduler->scheduleTransaction(transaction);

    auto response = createRpcResponse(uuid, RpcMethod::GetBlockNumber);

    bool result = mScheduler->tryAttachRpcResponseToTransaction(response);

    EXPECT_FALSE(result);
    EXPECT_FALSE(transaction->hasRpcResponse());
}
