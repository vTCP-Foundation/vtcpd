#ifndef VTCPD_CLEAREXCHANGERATESTRANSACTION_H
#define VTCPD_CLEAREXCHANGERATESTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/rates/ClearExchangeRatesCommand.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class ClearExchangeRatesTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<ClearExchangeRatesTransaction> Shared;

public:
    ClearExchangeRatesTransaction(
        ClearExchangeRatesCommand::Shared command,
        ExchangeRatesManager *exchangeRatesManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOK() const;

private:
    ClearExchangeRatesCommand::Shared mCommand;
    ExchangeRatesManager *mExchangeRatesManager;
};

#endif //VTCPD_CLEAREXCHANGERATESTRANSACTION_H