#include <gtest/gtest.h>
#include <boost/asio.hpp>

#include "core/delayed_tasks/CompletedPaymentsMonitoringDelayedTask.h"
#include "core/logger/Logger.h"

namespace as = boost::asio;

// Task 15-06: Unit tests for CompletedPaymentsMonitoringDelayedTask

class CompletedPaymentsMonitoringDelayedTaskTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mLogger = std::make_unique<Logger>();
    }

    void TearDown() override
    {
        mLogger.reset();
    }

    std::unique_ptr<Logger> mLogger;
};

// Test: Constructor initializes timer without throwing
TEST_F(CompletedPaymentsMonitoringDelayedTaskTest, ConstructorInitializesWithoutThrowing)
{
    as::io_context ioCtx;

    EXPECT_NO_THROW({
        CompletedPaymentsMonitoringDelayedTask task(ioCtx, *mLogger);
    });
}

// Test: Signal is properly declared and connectable
TEST_F(CompletedPaymentsMonitoringDelayedTaskTest, SignalIsConnectable)
{
    as::io_context ioCtx;
    CompletedPaymentsMonitoringDelayedTask task(ioCtx, *mLogger);

    bool signalConnected = false;
    auto connection = task.monitoringSignal.connect([&signalConnected]() {
        signalConnected = true;
    });

    // Verify connection was established (slot is connected)
    EXPECT_TRUE(connection.connected());
    EXPECT_EQ(task.monitoringSignal.num_slots(), 1);
}

// Test: Multiple slots can be connected to the signal
TEST_F(CompletedPaymentsMonitoringDelayedTaskTest, MultipleSlotConnections)
{
    as::io_context ioCtx;
    CompletedPaymentsMonitoringDelayedTask task(ioCtx, *mLogger);

    int callCount = 0;
    auto conn1 = task.monitoringSignal.connect([&callCount]() { callCount++; });
    auto conn2 = task.monitoringSignal.connect([&callCount]() { callCount++; });

    EXPECT_EQ(task.monitoringSignal.num_slots(), 2);
    EXPECT_TRUE(conn1.connected());
    EXPECT_TRUE(conn2.connected());
}

// Test: Signal disconnection works correctly
TEST_F(CompletedPaymentsMonitoringDelayedTaskTest, SignalDisconnection)
{
    as::io_context ioCtx;
    CompletedPaymentsMonitoringDelayedTask task(ioCtx, *mLogger);

    auto connection = task.monitoringSignal.connect([]() {});
    EXPECT_EQ(task.monitoringSignal.num_slots(), 1);

    connection.disconnect();
    EXPECT_EQ(task.monitoringSignal.num_slots(), 0);
    EXPECT_FALSE(connection.connected());
}

