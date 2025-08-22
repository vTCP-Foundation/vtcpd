#ifndef VTCPD_SETEXCHANGERATETRANSACTION_H
#define VTCPD_SETEXCHANGERATETRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/rates/SetExchangeRateCommand.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class SetExchangeRateTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<SetExchangeRateTransaction> Shared;

public:
    SetExchangeRateTransaction(
        SetExchangeRateCommand::Shared command,
        ExchangeRatesManager *exchangeRatesManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOK() const;

private:
    SetExchangeRateCommand::Shared mCommand;
    ExchangeRatesManager *mExchangeRatesManager;
};

#endif //VTCPD_SETEXCHANGERATETRANSACTION_H