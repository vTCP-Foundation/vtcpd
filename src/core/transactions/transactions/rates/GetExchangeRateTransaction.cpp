#include "GetExchangeRateTransaction.h"

GetExchangeRateTransaction::GetExchangeRateTransaction(
    GetExchangeRateCommand::Shared command,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::SetOutgoingTrustLineTransaction, // We'll add our own enum later
        0, // No equivalent for exchange rates
        logger),
    mCommand(command),
    mExchangeRatesManager(exchangeRatesManager)
{}

TransactionResult::SharedConst GetExchangeRateTransaction::run()
{
    debug() << "GetExchangeRateTransaction run: " << mCommand->equivalentFrom() 
            << " -> " << mCommand->equivalentTo();

    try {
        auto exchangeRate = mExchangeRatesManager->get(
            mCommand->equivalentFrom(),
            mCommand->equivalentTo());

        if (exchangeRate == nullptr) {
            debug() << "GetExchangeRateTransaction: Exchange rate not found";
            return transactionResultFromCommand(
                mCommand->responseExchangeRateIsAbsent());
        }

        debug() << "GetExchangeRateTransaction: Successfully retrieved exchange rate";
        return resultOK(exchangeRate);

    } catch (const exception &e) {
        warning() << "GetExchangeRateTransaction: Error occurred: " << e.what();
        return transactionResultFromCommand(
            mCommand->responseUnexpectedError());
    }
}

TransactionResult::SharedConst GetExchangeRateTransaction::resultOK(
    ExchangeRate::Shared exchangeRate) const
{
    string serializedRate = serializeExchangeRate(exchangeRate);
    return transactionResultFromCommand(
        mCommand->resultOk(serializedRate));
}

TransactionResult::SharedConst GetExchangeRateTransaction::resultNotFound() const
{
    return transactionResultFromCommand(
        mCommand->responseExchangeRateIsAbsent());
}

string GetExchangeRateTransaction::serializeExchangeRate(ExchangeRate::Shared exchangeRate) const
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

const string GetExchangeRateTransaction::logHeader() const
{
    stringstream s;
    s << "[GetExchangeRateTA: " << currentTransactionUUID().stringUUID() << "]";
    return s.str();
}