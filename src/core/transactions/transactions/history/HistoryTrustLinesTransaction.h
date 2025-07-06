#ifndef VTCPD_HISTORYTRUSTLINESTRANSACTION_H
#define VTCPD_HISTORYTRUSTLINESTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/history/HistoryTrustLinesCommand.h"
#include "../../../io/storage/interfaces/StorageHandler.h"
#include "../../../io/storage/record/trust_line/TrustLineRecord.h"

#include <vector>

class HistoryTrustLinesTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<HistoryTrustLinesTransaction> Shared;

public:
    HistoryTrustLinesTransaction(
        HistoryTrustLinesCommand::Shared command,
        StorageHandler *storageHandler,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst resultOk(
        const vector<TrustLineRecord::Shared> &records);

private:
    HistoryTrustLinesCommand::Shared mCommand;
    StorageHandler *mStorageHandler;
};


#endif //VTCPD_HISTORYTRUSTLINESTRANSACTION_H
