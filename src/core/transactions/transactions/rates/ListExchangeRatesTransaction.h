#ifndef VTCPD_LISTEXCHANGERATESTRANSACTION_H
#define VTCPD_LISTEXCHANGERATESTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/rates/ListExchangeRatesCommand.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class ListExchangeRatesTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<ListExchangeRatesTransaction> Shared;

public:
    ListExchangeRatesTransaction(
        ListExchangeRatesCommand::Shared command,
        ExchangeRatesManager *exchangeRatesManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOK(
        const vector<ExchangeRate::Shared> &exchangeRates) const;
    
    string serializeExchangeRate(ExchangeRate::Shared exchangeRate) const;

private:
    ListExchangeRatesCommand::Shared mCommand;
    ExchangeRatesManager *mExchangeRatesManager;
};

#endif //VTCPD_LISTEXCHANGERATESTRANSACTION_H