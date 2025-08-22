#ifndef VTCPD_GETEXCHANGERATETRANSACTION_H
#define VTCPD_GETEXCHANGERATETRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/rates/GetExchangeRateCommand.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class GetExchangeRateTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<GetExchangeRateTransaction> Shared;

public:
    GetExchangeRateTransaction(
        GetExchangeRateCommand::Shared command,
        ExchangeRatesManager *exchangeRatesManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOK(
        ExchangeRate::Shared exchangeRate) const;
    
    TransactionResult::SharedConst resultNotFound() const;

    string serializeExchangeRate(ExchangeRate::Shared exchangeRate) const;

private:
    GetExchangeRateCommand::Shared mCommand;
    ExchangeRatesManager *mExchangeRatesManager;
};

#endif //VTCPD_GETEXCHANGERATETRANSACTION_H