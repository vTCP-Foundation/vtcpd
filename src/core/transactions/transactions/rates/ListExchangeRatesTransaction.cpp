#include "ListExchangeRatesTransaction.h"

ListExchangeRatesTransaction::ListExchangeRatesTransaction(
    ListExchangeRatesCommand::Shared command,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::SetOutgoingTrustLineTransaction, // We'll add our own enum later
        0, // No equivalent for exchange rates
        logger),
    mCommand(command),
    mExchangeRatesManager(exchangeRatesManager)
{}

TransactionResult::SharedConst ListExchangeRatesTransaction::run()
{
    debug() << "ListExchangeRatesTransaction run";

    try {
        auto exchangeRates = mExchangeRatesManager->list();
        
        debug() << "ListExchangeRatesTransaction: Successfully retrieved " << exchangeRates.size() << " exchange rates";
        return resultOK(exchangeRates);

    } catch (const exception &e) {
        warning() << "ListExchangeRatesTransaction: Error occurred: " << e.what();
        return transactionResultFromCommand(
            mCommand->responseUnexpectedError());
    }
}

TransactionResult::SharedConst ListExchangeRatesTransaction::resultOK(
    const vector<ExchangeRate::Shared> &exchangeRates) const
{
    stringstream s;
    s << exchangeRates.size();
    
    for (auto const &exchangeRate : exchangeRates) {
        s << kTokensSeparator << serializeExchangeRate(exchangeRate);
    }
    
    string result = s.str();
    return transactionResultFromCommand(
        mCommand->resultOk(result));
}

string ListExchangeRatesTransaction::serializeExchangeRate(ExchangeRate::Shared exchangeRate) const
{
    const auto kUnixEpoch = DateTime(boost::gregorian::date(1970,1,1));
    const auto kUnixTimestampMicrosec = (exchangeRate->expiresAt() - kUnixEpoch).total_microseconds();
    
    stringstream s;
    s << exchangeRate->equivalentFrom() << kTokensSeparator
      << exchangeRate->equivalentTo() << kTokensSeparator
      << exchangeRate->exchangeRate() << kTokensSeparator
      << exchangeRate->exchangeRateShift() << kTokensSeparator
      << exchangeRate->minExchangeAmount() << kTokensSeparator
      << exchangeRate->maxExchangeAmount() << kTokensSeparator
      << kUnixTimestampMicrosec;
    
    return s.str();
}

const string ListExchangeRatesTransaction::logHeader() const
{
    stringstream s;
    s << "[ListExchangeRatesTA: " << currentTransactionUUID() << "]";
    return s.str();
}