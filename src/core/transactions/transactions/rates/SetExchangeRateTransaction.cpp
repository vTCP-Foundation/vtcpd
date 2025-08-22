#include "SetExchangeRateTransaction.h"

SetExchangeRateTransaction::SetExchangeRateTransaction(
    SetExchangeRateCommand::Shared command,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::SetOutgoingTrustLineTransaction, // We'll add our own enum later
        0, // No equivalent for exchange rates
        logger),
    mCommand(command),
    mExchangeRatesManager(exchangeRatesManager)
{}

TransactionResult::SharedConst SetExchangeRateTransaction::run()
{
    debug() << "SetExchangeRateTransaction run: " << mCommand->equivalentFrom() 
            << " -> " << mCommand->equivalentTo() 
            << " rate: " << mCommand->exchangeRate()
            << " shift: " << mCommand->exchangeRateShift();

    try {
        mExchangeRatesManager->addOrUpdate(
            mCommand->equivalentFrom(),
            mCommand->equivalentTo(),
            mCommand->exchangeRate(),
            mCommand->exchangeRateShift(),
            mCommand->minExchangeAmount(),
            mCommand->maxExchangeAmount());

        debug() << "SetExchangeRateTransaction: Successfully added/updated exchange rate";
        return resultOK();

    } catch (const exception &e) {
        warning() << "SetExchangeRateTransaction: Error occurred: " << e.what();
        return transactionResultFromCommand(
            mCommand->responseUnexpectedError());
    }
}

TransactionResult::SharedConst SetExchangeRateTransaction::resultOK() const
{
    stringstream s;
    s << currentTransactionUUID();
    
    return transactionResultFromCommand(
        mCommand->responseOK());
}

const string SetExchangeRateTransaction::logHeader() const
{
    stringstream s;
    s << "[SetExchangeRateTA: " << currentTransactionUUID() << "]";
    return s.str();
}