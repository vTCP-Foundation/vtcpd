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

    void addOrUpdateExternal(
        const ContractorID contractorID,
        const ExchangeRate &rate);

    vector<pair<ContractorID, ExchangeRate::Shared>> getExternalRates(
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo) const;

    vector<pair<ContractorID, ExchangeRate::Shared>> listExternalRates() const;

    void removeExternal(
        const ContractorID contractorID,
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo);

    TrustLineAmount calculateConvertedAmount(
        const SerializedEquivalent equivFrom,
        const SerializedEquivalent equivTo,
        const TrustLineAmount &amountInEquivFrom) const;

    // Prints the contents of mExternalExchangeRates via debug() logger
    void printExternalRates() const;

private:
    void scheduleExpiryTimer();

    void onExpiryTimer(
        const boost::system::error_code &error);

    DateTime earliestExpiryTime() const;

    void removeExpiredRates();

    void scheduleExternalExpiryTimer();

    void onExternalExpiryTimer(
        const boost::system::error_code &error);

    DateTime earliestExternalExpiryTime() const;

    void removeExpiredExternalRates();

    LoggerStream debug() const;

    const string logHeader() const;

public:
    static const uint32_t kTTLMilliseconds = 300000; // 5 minutes

private:
    as::io_context &mIOCtx;
    Logger &mLogger;
    map<pair<SerializedEquivalent, SerializedEquivalent>, ExchangeRate::Shared> mExchangeRates;
    unique_ptr<as::steady_timer> mExpiryTimer;
    map<pair<SerializedEquivalent, SerializedEquivalent>, vector<pair<ContractorID, ExchangeRate::Shared>>> mExternalExchangeRates;
    unique_ptr<as::steady_timer> mExternalExpiryTimer;
};

#endif //VTCPD_EXCHANGERATESMANAGER_H