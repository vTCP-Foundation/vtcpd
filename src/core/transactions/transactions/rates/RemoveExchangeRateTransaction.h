#ifndef VTCPD_REMOVEEXCHANGERATETRANSACTION_H
#define VTCPD_REMOVEEXCHANGERATETRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/rates/RemoveExchangeRateCommand.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class RemoveExchangeRateTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<RemoveExchangeRateTransaction> Shared;

public:
    RemoveExchangeRateTransaction(
        RemoveExchangeRateCommand::Shared command,
        ExchangeRatesManager *exchangeRatesManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOK() const;

private:
    RemoveExchangeRateCommand::Shared mCommand;
    ExchangeRatesManager *mExchangeRatesManager;
};

#endif //VTCPD_REMOVEEXCHANGERATETRANSACTION_H