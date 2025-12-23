#ifndef VTCPD_COMPLETEDPAYMENTSMONITORINGDELAYEDTASK_H
#define VTCPD_COMPLETEDPAYMENTSMONITORINGDELAYEDTASK_H

// Task 15-02: delayed task for completed payments observer monitoring.

#include "../logger/Logger.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>
#include <boost/asio.hpp>

using namespace std;
namespace as = boost::asio;
namespace signals = boost::signals2;

class CompletedPaymentsMonitoringDelayedTask
{
public:
    typedef signals::signal<void()> MonitoringSignal;

public:
    // Initializes the timer and schedules the first monitoring run.
    CompletedPaymentsMonitoringDelayedTask(
        as::io_context &ioCtx,
        Logger &logger);

public:
    // Emitted on each monitoring timer expiration.
    mutable MonitoringSignal monitoringSignal;

private:
    // Handles timer expiration, reschedules the next run, and emits the signal.
    void runMonitoring(
        const boost::system::error_code &errorCode);

    // Helper for info-level logs.
    LoggerStream info() const;

    // Helper for warning-level logs.
    LoggerStream warning() const;

    // Log prefix for this delayed task.
    const string logHeader() const;

private:
    static const uint32_t kInitialDelaySeconds = 60;
    static const uint32_t kMonitoringIntervalSeconds = 300;

private:
    as::io_context &mIOCtx;
    unique_ptr<as::steady_timer> mMonitoringTimer;
    Logger &mLog;
};

#endif //VTCPD_COMPLETEDPAYMENTSMONITORINGDELAYEDTASK_H
