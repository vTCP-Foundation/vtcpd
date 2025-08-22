#include "ExchangeRatesManager.h"

ExchangeRatesManager::ExchangeRatesManager(
    as::io_context &ioCtx,
    Logger &logger):

    mIOCtx(ioCtx),
    mLogger(logger)
{
    mExpiryTimer = make_unique<as::steady_timer>(mIOCtx);
}

void ExchangeRatesManager::addOrUpdate(
    const SerializedEquivalent equivFrom,
    const SerializedEquivalent equivTo,
    const TrustLineAmount &exchangeRate,
    const int16_t exchangeRateShift,
    const TrustLineAmount &minExchangeAmount,
    const TrustLineAmount &maxExchangeAmount)
{
    auto key = make_pair(equivFrom, equivTo);
    auto expiresAt = utc_now() + Duration(0, 0, 0, kTTLMilliseconds * 1000);

    auto it = mExchangeRates.find(key);
    if (it != mExchangeRates.end()) {
        debug() << "Updating existing rate from " << equivFrom << " to " << equivTo;
        it->second->update(
            exchangeRate,
            exchangeRateShift,
            minExchangeAmount,
            maxExchangeAmount,
            expiresAt);
    } else {
        debug() << "Adding new rate from " << equivFrom << " to " << equivTo;
        auto rate = make_shared<ExchangeRate>(
            equivFrom,
            equivTo,
            exchangeRate,
            exchangeRateShift,
            expiresAt,
            minExchangeAmount,
            maxExchangeAmount);
        mExchangeRates[key] = rate;
    }

    scheduleExpiryTimer();
}

ExchangeRate::Shared ExchangeRatesManager::get(
    const SerializedEquivalent equivFrom,
    const SerializedEquivalent equivTo) const
{
    auto key = make_pair(equivFrom, equivTo);
    auto it = mExchangeRates.find(key);
    
    if (it != mExchangeRates.end()) {
        return it->second;
    }
    
    return nullptr;
}

vector<ExchangeRate::Shared> ExchangeRatesManager::list() const
{
    vector<ExchangeRate::Shared> result;
    result.reserve(mExchangeRates.size());
    
    for (const auto &pair : mExchangeRates) {
        result.push_back(pair.second);
    }
    
    return result;
}

void ExchangeRatesManager::remove(
    const SerializedEquivalent equivFrom,
    const SerializedEquivalent equivTo)
{
    auto key = make_pair(equivFrom, equivTo);
    auto it = mExchangeRates.find(key);
    
    if (it != mExchangeRates.end()) {
        debug() << "Removing rate from " << equivFrom << " to " << equivTo;
        mExchangeRates.erase(it);
        scheduleExpiryTimer();
    }
}

void ExchangeRatesManager::clear()
{
    debug() << "Clearing all exchange rates";
    mExchangeRates.clear();
    if (mExpiryTimer) {
        mExpiryTimer->cancel();
    }
}

TrustLineAmount ExchangeRatesManager::calculateConvertedAmount(
    const SerializedEquivalent equivFrom,
    const SerializedEquivalent equivTo,
    const TrustLineAmount &amountInEquivFrom) const
{
    auto rate = get(equivFrom, equivTo);
    if (!rate) {
        throw NotFoundError("Exchange rate not found for pair");
    }

    // Multiply amount by exchange rate
    TrustLineAmount result;
    try {
        result = amountInEquivFrom * rate->exchangeRate();
    } catch (const std::overflow_error &) {
        throw OverflowError("Overflow in amount multiplication");
    }

    // Apply base-10 shift
    int16_t shift = rate->exchangeRateShift();
    if (shift > 0) {
        // Positive shift - multiply by 10^shift
        for (int16_t i = 0; i < shift; ++i) {
            try {
                TrustLineAmount newResult = result * 10;
                if (newResult < result) {
                    throw OverflowError("Overflow in positive shift application");
                }
                result = newResult;
            } catch (const std::overflow_error &) {
                throw OverflowError("Overflow in positive shift application");
            }
        }
    } else if (shift < 0) {
        // Negative shift - divide by 10^(-shift) with truncation toward zero
        for (int16_t i = 0; i < -shift; ++i) {
            result = result / 10; // Integer division truncates toward zero
        }
    }
    // shift == 0: no change needed

    return result;
}

void ExchangeRatesManager::scheduleExpiryTimer()
{
    if (mExchangeRates.empty()) {
        return;
    }

    DateTime earliestExpiry = earliestExpiryTime();
    auto now = utc_now();
    
    if (earliestExpiry <= now) {
        // Some rates have already expired, remove them immediately
        removeExpiredRates();
        if (mExchangeRates.empty()) {
            return;
        }
        earliestExpiry = earliestExpiryTime();
    }

    Duration delay = earliestExpiry - now;
    auto delayMs = delay.total_milliseconds();
    if (delayMs < 0) {
        delayMs = 0;
    }

    debug() << "Scheduling expiry timer for " << delayMs << "ms";
    mExpiryTimer->expires_after(chrono::milliseconds(delayMs));
    mExpiryTimer->async_wait(boost::bind(
        &ExchangeRatesManager::onExpiryTimer,
        this,
        as::placeholders::error));
}

void ExchangeRatesManager::onExpiryTimer(
    const boost::system::error_code &error)
{
    if (error == as::error::operation_aborted) {
        return; // Timer was cancelled
    }
    if (error != boost::system::errc::success) {
        return;
    }

    debug() << "Expiry timer fired, removing expired rates";
    removeExpiredRates();
    
    // Reschedule timer if there are still rates remaining
    if (!mExchangeRates.empty()) {
        scheduleExpiryTimer();
    }
}

DateTime ExchangeRatesManager::earliestExpiryTime() const
{
    if (mExchangeRates.empty()) {
        return utc_now();
    }

    DateTime earliest = mExchangeRates.begin()->second->expiresAt();
    for (const auto &pair : mExchangeRates) {
        if (pair.second->expiresAt() < earliest) {
            earliest = pair.second->expiresAt();
        }
    }
    
    return earliest;
}

void ExchangeRatesManager::removeExpiredRates()
{
    auto now = utc_now();
    auto it = mExchangeRates.begin();
    
    while (it != mExchangeRates.end()) {
        if (it->second->expiresAt() <= now) {
            debug() << "Removing expired rate from " 
                    << it->first.first << " to " << it->first.second;
            it = mExchangeRates.erase(it);
        } else {
            ++it;
        }
    }
}

LoggerStream ExchangeRatesManager::debug() const
{
    return mLogger.debug(logHeader());
}

const string ExchangeRatesManager::logHeader() const
{
    return "[ExchangeRatesManager]";
}