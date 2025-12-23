#include "CompletedPaymentsMonitoringDelayedTask.h"

// Task 15-02: delayed task implementation for completed payments monitoring.

// Schedules the first monitoring run after the initial delay.
CompletedPaymentsMonitoringDelayedTask::CompletedPaymentsMonitoringDelayedTask(
    as::io_context &ioCtx,
    Logger &logger):
    mIOCtx(ioCtx),
    mLog(logger)
{
    // Prepare timer and schedule the first run.
    mMonitoringTimer = make_unique<as::steady_timer>(
                           mIOCtx);
    info() << "schedule completed payments monitoring after "
           << kInitialDelaySeconds
           << " seconds";
    mMonitoringTimer->expires_after(
        chrono::seconds(
            kInitialDelaySeconds));
    mMonitoringTimer->async_wait(
        boost::bind(
            &CompletedPaymentsMonitoringDelayedTask::runMonitoring,
            this,
            as::placeholders::error));
}

// Handles timer expiration, reschedules the next run, and emits the signal.
void CompletedPaymentsMonitoringDelayedTask::runMonitoring(
    const boost::system::error_code &errorCode)
{
    // Log the timer event and any timer errors.
    info() << "run completed payments monitoring";
    if (errorCode) {
        warning() << errorCode.message().c_str();
    }

    // Reschedule before emitting the signal to keep cycles consistent.
    mMonitoringTimer->cancel();
    mMonitoringTimer->expires_after(
        chrono::seconds(
            kMonitoringIntervalSeconds));
    info() << "schedule completed payments monitoring after "
           << kMonitoringIntervalSeconds
           << " seconds";
    mMonitoringTimer->async_wait(
        boost::bind(
            &CompletedPaymentsMonitoringDelayedTask::runMonitoring,
            this,
            as::placeholders::error));
    monitoringSignal();
}

// Returns logger stream for info entries.
LoggerStream CompletedPaymentsMonitoringDelayedTask::info() const
{
    return mLog.info(logHeader());
}

// Returns logger stream for warning entries.
LoggerStream CompletedPaymentsMonitoringDelayedTask::warning() const
{
    return mLog.warning(logHeader());
}

// Provides a log header for this delayed task.
const string CompletedPaymentsMonitoringDelayedTask::logHeader() const
{
    stringstream s;
    s << "CompletedPaymentsMonitoringDelayedTask";
    return s.str();
}
