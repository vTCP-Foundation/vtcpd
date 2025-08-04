#ifndef VTCPD_REMOVEOUTDATEDCRYPTODATATRANSACTION_H
#define VTCPD_REMOVEOUTDATEDCRYPTODATATRANSACTION_H

#include "../base/BaseTransaction.h"

#include "../../../interface/commands_interface/commands/general/RemoveOutdatedCryptoDataCommand.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../io/storage/interfaces/StorageHandler.h"
#include "../../../crypto/keychain.h"

#include "../../../interface/commands_interface/commands/general/RemoveOutdatedCryptoDataCommand.h"

class RemoveOutdatedCryptoDataTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<RemoveOutdatedCryptoDataTransaction> Shared;

public:
    RemoveOutdatedCryptoDataTransaction(
        RemoveOutdatedCryptoDataCommand::Shared command,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        Keystore *keystore,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

    TransactionResult::SharedConst resultOK();

private:
    static const AuditNumber kOutdatedDifference = 2;

private:
    RemoveOutdatedCryptoDataCommand::Shared mCommand;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    StorageHandler *mStorageHandler;
    Keystore *mKeysStore;
};


#endif //VTCPD_REMOVEOUTDATEDCRYPTODATATRANSACTION_H
