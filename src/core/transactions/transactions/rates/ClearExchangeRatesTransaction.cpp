#include "ClearExchangeRatesTransaction.h"

ClearExchangeRatesTransaction::ClearExchangeRatesTransaction(
    ClearExchangeRatesCommand::Shared command,
    ExchangeRatesManager *exchangeRatesManager,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::SetOutgoingTrustLineTransaction, // We'll add our own enum later
        0, // No equivalent for exchange rates
        logger),
    mCommand(command),
    mExchangeRatesManager(exchangeRatesManager)
{}

TransactionResult::SharedConst ClearExchangeRatesTransaction::run()
{
    debug() << "ClearExchangeRatesTransaction run";

    try {
        mExchangeRatesManager->clear();

        debug() << "ClearExchangeRatesTransaction: Successfully cleared all exchange rates";
        return resultOK();

    } catch (const exception &e) {
        warning() << "ClearExchangeRatesTransaction: Error occurred: " << e.what();
        return transactionResultFromCommand(
            mCommand->responseUnexpectedError());
    }
}

TransactionResult::SharedConst ClearExchangeRatesTransaction::resultOK() const
{
    stringstream s;
    s << currentTransactionUUID().stringUUID();
    
    return transactionResultFromCommand(
        mCommand->responseOK());
}

const string ClearExchangeRatesTransaction::logHeader() const
{
    stringstream s;
    s << "[ClearExchangeRatesTA: " << currentTransactionUUID().stringUUID() << "]";
    return s.str();
}