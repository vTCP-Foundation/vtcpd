#include "PongReactionTransaction.h"

PongReactionTransaction::PongReactionTransaction(
    const NodeUUID &nodeUUID,
    PongMessage::Shared message,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    Logger &logger):
    BaseTransaction(
        BaseTransaction::PongReactionType,
        nodeUUID,
        0,
        logger),
    mContractorUUID(message->senderUUID),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mStorageHandler(storageHandler)
{}

TransactionResult::SharedConst PongReactionTransaction::run()
{
    for (const auto equivalent : mEquivalentsSubsystemsRouter->equivalents()) {
        auto trustLineManager = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent);

        if (!trustLineManager->trustLineIsPresent(mContractorUUID)) {
            continue;
        }

        if (trustLineManager->trustLineState(mContractorUUID) == TrustLine::Init) {
            mResumeTransactionSignal(mContractorUUID, equivalent, BaseTransaction::OpenTrustLineTransaction);
        } else if (trustLineManager->trustLineState(mContractorUUID) == TrustLine::AuditPending) {
            mResumeTransactionSignal(mContractorUUID, equivalent, BaseTransaction::AuditSourceTransactionType);
        }
    }
    processPongMessage(mContractorUUID);
    return resultDone();
}

const string PongReactionTransaction::logHeader() const
{
    stringstream s;
    s << "[PongReactionTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}