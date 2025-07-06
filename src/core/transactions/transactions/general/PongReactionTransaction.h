#ifndef VTCPD_PONGREACTIONTRANSACTION_H
#define VTCPD_PONGREACTIONTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../network/messages/general/PongMessage.h"

#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../io/storage/interfaces/StorageHandler.h"

namespace signals = boost::signals2;

class PongReactionTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<PongReactionTransaction> Shared;

public:
    typedef signals::signal<void(
        ContractorID,
        const SerializedEquivalent,
        const TransactionType)> ResumeTransactionSignal;

public:
    PongReactionTransaction(
        PongMessage::Shared message,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

public:
    mutable ResumeTransactionSignal mResumeTransactionSignal;

private:
    ContractorID mContractorID;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    StorageHandler *mStorageHandler;
};


#endif //VTCPD_PONGREACTIONTRANSACTION_H
