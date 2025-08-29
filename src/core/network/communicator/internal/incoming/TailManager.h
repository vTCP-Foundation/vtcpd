#ifndef VTCPD_TAILMANAGER_H
#define VTCPD_TAILMANAGER_H

#include <list>
#include <deque>
#include "../../../messages/Message.hpp"
#include "../../../../logger/Logger.h"

#include <boost/signals2.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

namespace as = boost::asio;
namespace signals = boost::signals2;

class TailManager
{
public:
    struct Msg : Message::Shared
    {
        Msg(const Message::Shared &ptr) : Message::Shared(ptr) {}
        DateTime time;
    };
    typedef std::deque<Msg> MsgList;

    struct Tail : MsgList
    {
        explicit Tail(TailManager &manager);
        TailManager &mManager;
    };
    typedef std::list<Tail *> TailList;

private:
    const uint32_t kUpdatingTimerPeriodSeconds = 60;

    static const byte_t kCleanHours = 0;
    static const byte_t kCleanMinutes = 5;
    static const byte_t kCleanSeconds = 0;

    static Duration &kCleanDuration()
    {
        static auto duration = Duration(
                                   kCleanHours,
                                   kCleanMinutes,
                                   kCleanSeconds);
        return duration;
    }

public:
    TailManager(
        as::io_context &ioCtx,
        Logger &logger);
    ~TailManager();

private:
    void update(const boost::system::error_code &err);

public:
    Tail &getFlowTail()
    {
        return mFlowTail;
    }
    Tail &getCyclesFiveTail()
    {
        return mCyclesFiveTail;
    }
    Tail &getCyclesSixTail()
    {
        return mCyclesSixTail;
    }
    Tail &getRoutingTableTail()
    {
        return mRoutingTableTail;
    }
    Tail &getExchangeRatesTail()
    {
        return mExchangeRatesTail;
    }

private:
    LoggerStream info() const;
    LoggerStream debug() const;
    LoggerStream warning() const;
    const string logHeader() const;

private:
    Logger &mLog;
    TailList mTails;

    Tail mFlowTail;
    Tail mCyclesFiveTail;
    Tail mCyclesSixTail;
    Tail mRoutingTableTail;
    Tail mExchangeRatesTail;

    as::io_context &mIOCtx;
    unique_ptr<as::steady_timer> mUpdatingTimer;
};

#endif // VTCPD_TAILMANAGER_H
