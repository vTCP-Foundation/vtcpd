#ifndef VTCPD_HISTORYADDITIONALPAYMENTSTRANSACTION_H
#define VTCPD_HISTORYADDITIONALPAYMENTSTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/history/HistoryAdditionalPaymentsCommand.h"
#include "../../../io/storage/interfaces/StorageHandler.h"

#include <vector>

class HistoryAdditionalPaymentsTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<HistoryAdditionalPaymentsTransaction> Shared;

public:
    HistoryAdditionalPaymentsTransaction(
        HistoryAdditionalPaymentsCommand::Shared command,
        StorageHandler *storageHandler,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOk(
        const vector<PaymentAdditionalRecord::Shared> &records);

private:
    HistoryAdditionalPaymentsCommand::Shared mCommand;
    StorageHandler *mStorageHandler;
};

#endif //VTCPD_HISTORYADDITIONALPAYMENTSTRANSACTION_H
