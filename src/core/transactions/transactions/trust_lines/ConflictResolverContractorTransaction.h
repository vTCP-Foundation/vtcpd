#ifndef VTCPD_CONFLICTRESOLVERCONTRACTORTRANSACTION_H
#define VTCPD_CONFLICTRESOLVERCONTRACTORTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../crypto/keychain.h"

#include "../../../subsystems_controller/TrustLinesInfluenceController.h"

#include "../../../network/messages/trust_lines/ConflictResolverMessage.h"
#include "../../../network/messages/trust_lines/ConflictResolverResponseMessage.h"
#include "ConflictResolverInitiatorTransaction.h"

class ConflictResolverContractorTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<ConflictResolverContractorTransaction> Shared;

public:
    ConflictResolverContractorTransaction(
        ConflictResolverMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLinesManager,
        StorageHandler *storageHandler,
        Keystore *keystore,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    pair<BytesShared, size_t> getSerializedReceipt(
        ContractorID source,
        ContractorID target,
        const ReceiptRecord::Shared receiptRecord);

    void acceptContractorAuditData(
        IOTransaction::Shared ioTransaction,
        TrustLineKeychain *keyChain);

private:
    ConflictResolverMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    StorageHandler *mStorageHandler;
    Keystore *mKeysStore;

    ContractorID mContractorID;

    TrustLinesInfluenceController *mTrustLinesInfluenceController;
};


#endif //VTCPD_CONFLICTRESOLVERCONTRACTORTRANSACTION_H
