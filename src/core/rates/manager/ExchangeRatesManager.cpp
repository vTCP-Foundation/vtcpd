#include "ExchangeRatesManager.h"
#include <algorithm>

ExchangeRatesManager::ExchangeRatesManager(
    as::io_context &ioCtx,
    Logger &logger):

    mIOCtx(ioCtx),
    mLogger(logger)
{
    mExpiryTimer = make_unique<as::steady_timer>(mIOCtx);
    mExternalExpiryTimer = make_unique<as::steady_timer>(mIOCtx);
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
    mExternalExchangeRates.clear();
    if (mExpiryTimer) {
        mExpiryTimer->cancel();
    }
    if (mExternalExpiryTimer) {
        mExternalExpiryTimer->cancel();
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

void ExchangeRatesManager::addOrUpdateExternal(
    const ExchangeRate &rate)
{
    auto key = make_pair(rate.equivalentFrom(), rate.equivalentTo());
    auto rateShared = make_shared<ExchangeRate>(
        rate.equivalentFrom(),
        rate.equivalentTo(),
        rate.exchangeRate(),
        rate.exchangeRateShift(),
        rate.expiresAt(),
        rate.minExchangeAmount(),
        rate.maxExchangeAmount());

    debug() << "Adding external rate from " << rate.equivalentFrom() 
            << " to " << rate.equivalentTo();

    auto it = mExternalExchangeRates.find(key);
    if (it != mExternalExchangeRates.end()) {
        it->second.push_back(rateShared);
    } else {
        vector<ExchangeRate::Shared> rates = { rateShared };
        mExternalExchangeRates[key] = rates;
    }

    scheduleExternalExpiryTimer();
}

vector<ExchangeRate::Shared> ExchangeRatesManager::getExternalRates(
    const SerializedEquivalent equivFrom,
    const SerializedEquivalent equivTo) const
{
    auto key = make_pair(equivFrom, equivTo);
    auto it = mExternalExchangeRates.find(key);
    
    if (it != mExternalExchangeRates.end()) {
        return it->second;
    }
    
    return vector<ExchangeRate::Shared>();
}

vector<ExchangeRate::Shared> ExchangeRatesManager::listExternalRates() const
{
    vector<ExchangeRate::Shared> result;
    
    for (const auto &pair : mExternalExchangeRates) {
        for (const auto &rate : pair.second) {
            result.push_back(rate);
        }
    }
    
    return result;
}

void ExchangeRatesManager::scheduleExternalExpiryTimer()
{
    if (mExternalExchangeRates.empty()) {
        return;
    }

    DateTime earliestExpiry = earliestExternalExpiryTime();
    auto now = utc_now();
    
    if (earliestExpiry <= now) {
        removeExpiredExternalRates();
        if (mExternalExchangeRates.empty()) {
            return;
        }
        earliestExpiry = earliestExternalExpiryTime();
    }

    Duration delay = earliestExpiry - now;
    auto delayMs = delay.total_milliseconds();
    if (delayMs < 0) {
        delayMs = 0;
    }

    debug() << "Scheduling external expiry timer for " << delayMs << "ms";
    mExternalExpiryTimer->expires_after(chrono::milliseconds(delayMs));
    mExternalExpiryTimer->async_wait(boost::bind(
        &ExchangeRatesManager::onExternalExpiryTimer,
        this,
        as::placeholders::error));
}

void ExchangeRatesManager::onExternalExpiryTimer(
    const boost::system::error_code &error)
{
    if (error == as::error::operation_aborted) {
        return;
    }
    if (error != boost::system::errc::success) {
        return;
    }

    debug() << "External expiry timer fired, removing expired external rates";
    removeExpiredExternalRates();
    
    if (!mExternalExchangeRates.empty()) {
        scheduleExternalExpiryTimer();
    }
}

DateTime ExchangeRatesManager::earliestExternalExpiryTime() const
{
    if (mExternalExchangeRates.empty()) {
        return utc_now();
    }

    DateTime earliest;
    bool firstFound = false;
    
    for (const auto &pair : mExternalExchangeRates) {
        for (const auto &rate : pair.second) {
            if (!firstFound) {
                earliest = rate->expiresAt();
                firstFound = true;
            } else if (rate->expiresAt() < earliest) {
                earliest = rate->expiresAt();
            }
        }
    }
    
    return earliest;
}

void ExchangeRatesManager::removeExpiredExternalRates()
{
    auto now = utc_now();
    auto it = mExternalExchangeRates.begin();
    
    while (it != mExternalExchangeRates.end()) {
        auto &rates = it->second;
        
        rates.erase(
            remove_if(rates.begin(), rates.end(),
                [now](const ExchangeRate::Shared &rate) {
                    return rate->expiresAt() <= now;
                }),
            rates.end());
        
        if (rates.empty()) {
            debug() << "Removing empty external rates vector for pair " 
                    << it->first.first << " to " << it->first.second;
            it = mExternalExchangeRates.erase(it);
        } else {
            ++it;
        }
    }
}