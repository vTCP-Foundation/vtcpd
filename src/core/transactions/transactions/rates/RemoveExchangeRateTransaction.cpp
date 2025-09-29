#include "RemoveExchangeRateTransaction.h"

RemoveExchangeRateTransaction::RemoveExchangeRateTransaction(
    RemoveExchangeRateCommand::Shared command,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::SetOutgoingTrustLineTransaction, // We'll add our own enum later
        0, // No equivalent for exchange rates
        logger),
    mCommand(command),
    mExchangeRatesManager(exchangeRatesManager)
{}

TransactionResult::SharedConst RemoveExchangeRateTransaction::run()
{
    debug() << "RemoveExchangeRateTransaction run: " << mCommand->equivalentFrom() 
            << " -> " << mCommand->equivalentTo();

    try {
        mExchangeRatesManager->remove(
            mCommand->equivalentFrom(),
            mCommand->equivalentTo());

        debug() << "RemoveExchangeRateTransaction: Successfully removed exchange rate";
        return resultOK();

    } catch (const exception &e) {
        warning() << "RemoveExchangeRateTransaction: Error occurred: " << e.what();
        return transactionResultFromCommand(
            mCommand->responseUnexpectedError());
    }
}

TransactionResult::SharedConst RemoveExchangeRateTransaction::resultOK() const
{
    stringstream s;
    s << currentTransactionUUID().stringUUID();
    
    return transactionResultFromCommand(
        mCommand->responseOK());
}

const string RemoveExchangeRateTransaction::logHeader() const
{
    stringstream s;
    s << "[RemoveExchangeRateTA: " << currentTransactionUUID().stringUUID() << "]";
    return s.str();
}