#ifndef VTCPD_EXCHANGERATESMANAGER_H
#define VTCPD_EXCHANGERATESMANAGER_H

#include "../ExchangeRate.h"
#include "../../logger/Logger.h"
#include "../../common/Types.h"
#include "../../common/time/TimeUtils.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../common/exceptions/OverflowError.h"
#include "../../common/multiprecision/MultiprecisionUtils.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <map>
#include <vector>
#include <memory>
#include <chrono>

using namespace std;
namespace as = boost::asio;

class ExchangeRatesManager
{
public:
    ExchangeRatesManager(
        as::io_context &ioCtx,
        Logger &logger);

public:
    void addOrUpdate(
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo,
        const TrustLineAmount &exchangeRate,
        const int16_t exchangeRateShift,
        const TrustLineAmount &minExchangeAmount,
        const TrustLineAmount &maxExchangeAmount);

    ExchangeRate::Shared get(
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo) const;

    vector<ExchangeRate::Shared> list() const;

    void remove(
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo);

    void clear();

    TrustLineAmount calculateConvertedAmount(
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo,
        const TrustLineAmount &amountInEquivFrom) const;

private:
    void scheduleExpiryTimer();

    void onExpiryTimer(
        const boost::system::error_code &error);

    DateTime earliestExpiryTime() const;

    void removeExpiredRates();

    LoggerStream debug() const;

    const string logHeader() const;

public:
    static const uint32_t kTTLMilliseconds = 300000; // 5 minutes

private:
    as::io_context &mIOCtx;
    Logger &mLogger;
    map<pair<SerializedEquivalent, SerializedEquivalent>, ExchangeRate::Shared> mExchangeRates;
    unique_ptr<as::steady_timer> mExpiryTimer;
};

#endif //VTCPD_EXCHANGERATESMANAGER_H