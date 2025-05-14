#ifndef VTCPD_TOPOLOGYEVENTDELAYEDTASK_H
#define VTCPD_TOPOLOGYEVENTDELAYEDTASK_H

#include "../equivalents/EquivalentsSubsystemsRouter.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>

class TopologyEventDelayedTask
{

public:
    TopologyEventDelayedTask(
        as::io_context &ioCtx,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        Logger &logger);

private:
    void runTopologyEvent();

private:
    const uint16_t kDelayedTaskTimeSec = 5;

private:
    as::io_context &mIOCtx;
    unique_ptr<as::steady_timer> mTopologyEventTimer;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    Logger &mLog;
};


#endif //VTCPD_TOPOLOGYEVENTDELAYEDTASK_H
