#include "AuditSourceTransaction.h"

#include "../../../network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "../../../network/rpc/responses/GetBlockNumberRpcResponse.h"

AuditSourceTransaction::AuditSourceTransaction(
    ContractorID contractorID,
    const SerializedEquivalent equivalent,
    ContractorsManager *contractorsManager,
    TrustLinesManager *manager,
    StorageHandler *storageHandler,
    Keystore *keystore,
    FeaturesManager *featuresManager,
    TrustLinesInfluenceController *trustLinesInfluenceController,
    Logger &logger) :
    BaseTrustLineTransaction(
        BaseTransaction::AuditSourceTransactionType,
        equivalent,
        contractorID,
        contractorsManager,
        manager,
        storageHandler,
        keystore,
        featuresManager,
        trustLinesInfluenceController,
        logger),
    mCountSendingAttempts(0),
    mCountPendingAttempts(0),
    mCountContractorPendingAttempts(0),
    mCurrentBlockNumber(0),
    mBlockNumberRequestSent(false),
    mBlockNumberRequestPurpose(BlockNumberRequestNone),
    mAuditRetryCount(0)
{
    mAuditNumber = mTrustLines->auditNumber(mContractorID) + 1;
    mStep = Initialization;
}

AuditSourceTransaction::AuditSourceTransaction(
    const SerializedEquivalent equivalent,
    ContractorsManager *contractorsManager,
    ContractorID contractorID,
    TrustLinesManager *manager,
    StorageHandler *storageHandler,
    Keystore *keystore,
    FeaturesManager *featuresManager,
    TrustLinesInfluenceController *trustLinesInfluenceController,
    Logger &logger) :
    BaseTrustLineTransaction(
        BaseTransaction::AuditSourceTransactionType,
        equivalent,
        contractorID,
        contractorsManager,
        manager,
        storageHandler,
        keystore,
        featuresManager,
        trustLinesInfluenceController,
        logger),
    mCountSendingAttempts(0),
    mCountPendingAttempts(0),
    mCountContractorPendingAttempts(0),
    mCurrentBlockNumber(0),
    mBlockNumberRequestSent(false),
    mBlockNumberRequestPurpose(BlockNumberRequestNone),
    mAuditRetryCount(0)
{
    mAuditNumber = mTrustLines->auditNumber(mContractorID);
    mStep = NextAttempt;
}

TransactionResult::SharedConst AuditSourceTransaction::run()
{
    switch (mStep) {
    case Stages::Initialization: {
        return runInitializationStage();
    }
    case Stages::Pending: {
        return runAuditPendingStage();
    }
    case Stages::NextAttempt: {
        return runNextAttemptStage();
    }
    case Stages::NextAttemptPending: {
        return runNextAttemptAuditPendingStage();
    }
    case Stages::BlockNumberRequest: {
        return runBlockNumberRequestStage();
    }
    case Stages::ResponseProcessing: {
        return runResponseProcessingStage();
    }
    case Stages::ContractorPending: {
        return runContractorPendingStage();
    }
    default:
        throw ValueError(logHeader() + "::run: "
                                       "wrong value of mStep");
    }
}

TransactionResult::SharedConst AuditSourceTransaction::runInitializationStage()
{
    info() << "runInitializationStage " << mContractorID;

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "There is no contractor with requested id";
        return resultDone();
    }

    try {
        if (mTrustLines->trustLineState(mContractorID) != TrustLine::Active) {
            warning() << "Invalid TL state " << mTrustLines->trustLineState(mContractorID);
            return resultDone();
        }

    } catch (NotFoundError &e) {
        warning() << "Attempt to audit not existing TL";
        return resultDone();
    }

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::KeysSharing) {
        info() << "Keys sharing transaction in pending. Audit cancelled.";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineOwnKeysPresent(mContractorID)) {
        warning() << "There are no own keys";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineContractorKeysPresent(mContractorID)) {
        warning() << "There are no contractor keys";
        return resultDone();
    }

    return startBlockNumberRequest(
        BlockNumberRequestInitialization);
}

TransactionResult::SharedConst AuditSourceTransaction::runAuditPendingStage()
{
    info() << "runAuditPendingStage with " << mContractorID << " attempt " << mCountPendingAttempts;

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "There is no contractor with requested id";
        return resultDone();
    }

    try {
        if (mTrustLines->trustLineState(mContractorID) != TrustLine::AuditPending) {
            warning() << "Invalid TL state " << mTrustLines->trustLineState(mContractorID);
            return resultDone();
        }

    } catch (NotFoundError &e) {
        warning() << "Attempt to audit not existing TL";
        return resultDone();
    }

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::KeysSharing) {
        info() << "Keys sharing transaction in pending. Audit cancelled.";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineOwnKeysPresent(mContractorID)) {
        warning() << "There are no own keys";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineContractorKeysPresent(mContractorID)) {
        warning() << "There are no contractor keys";
        return resultDone();
    }

    return startBlockNumberRequest(
        BlockNumberRequestInitialization);
}


TransactionResult::SharedConst AuditSourceTransaction::runNextAttemptStage()
{
    info() << "runNextAttemptStage " << mContractorID;

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "There is no contractor with requested id";
        return resultDone();
    }

    try {
        if (mTrustLines->trustLineState(mContractorID) != TrustLine::AuditPending) {
            warning() << "Invalid TL state " << mTrustLines->trustLineState(mContractorID);
            return resultDone();
        }

    } catch (NotFoundError &e) {
        warning() << "Attempt to audit not existing TL";
        return resultDone();
    }

    processPongMessage(mContractorID);

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineOwnKeysPresent(mContractorID)) {
        warning() << "There are no own keys";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineContractorKeysPresent(mContractorID)) {
        warning() << "There are no contractor keys";
        return resultDone();
    }

    return startBlockNumberRequest(
        BlockNumberRequestNextAttempt);
}

TransactionResult::SharedConst AuditSourceTransaction::runNextAttemptAuditPendingStage()
{
    info() << "runNextAttemptAuditPendingStage " << mContractorID << " attempt " << mCountPendingAttempts;

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "There is no contractor with requested id";
        return resultDone();
    }

    try {
        if (mTrustLines->trustLineState(mContractorID) != TrustLine::AuditPending) {
            warning() << "Invalid TL state " << mTrustLines->trustLineState(mContractorID);
            return resultDone();
        }

    } catch (NotFoundError &e) {
        warning() << "Attempt to audit not existing TL";
        return resultDone();
    }

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::KeysSharing) {
        info() << "Keys sharing transaction in pending. Audit cancelled.";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineOwnKeysPresent(mContractorID)) {
        warning() << "There are no own keys";
        return resultDone();
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineContractorKeysPresent(mContractorID)) {
        warning() << "There are no contractor keys";
        return resultDone();
    }

    return startBlockNumberRequest(
        BlockNumberRequestNextAttempt);
}

TransactionResult::SharedConst AuditSourceTransaction::startBlockNumberRequest(
    BlockNumberRequestPurpose purpose)
{
    info() << "startBlockNumberRequest";
    mBlockNumberRequestPurpose = purpose;
    mBlockNumberRequestSent = false;
    mStep = BlockNumberRequest;
    info() << "Switching to block number request stage";
    return runBlockNumberRequestStage();
}

TransactionResult::SharedConst AuditSourceTransaction::runBlockNumberRequestStage()
{
    info() << "runBlockNumberRequestStage";

    // Send GetBlockNumber request once and wait for response.
    if (!mBlockNumberRequestSent) {
        info() << "Requesting current block number";
        sendRpcRequest(
            make_shared<GetBlockNumberRpcRequest>(
                currentTransactionUUID()));
        mBlockNumberRequestSent = true;
        return resultWaitForRpcResponse(RpcMethod::GetBlockNumber);
    }

    if (!hasRpcResponse()) {
        warning() << "GetBlockNumber RPC timed out";
        // TODO: revisit block number retrieval failure handling.
        return resultDone();
    }

    if (mRpcContext.front()->method() != RpcMethod::GetBlockNumber) {
        warning() << "Unexpected RPC response in block number stage";
        // TODO: revisit block number retrieval failure handling.
        return resultDone();
    }

    auto response = popRpcResponse<GetBlockNumberRpcResponse>();
    if (response->status() != RpcResponseStatus::Success) {
        warning() << "Block number retrieval failed: " << response->errorMessage();
        // TODO: revisit block number retrieval failure handling.
        return resultDone();
    }

    mCurrentBlockNumber = response->blockNumber();
    info() << "Current block number received: " << mCurrentBlockNumber;
    mBlockNumberRequestSent = false;

    const auto kPurpose = mBlockNumberRequestPurpose;
    mBlockNumberRequestPurpose = BlockNumberRequestNone;

    // Resume audit flow after successful block number retrieval.
    switch (kPurpose) {
    case BlockNumberRequestInitialization:
        return initializeAudit();
    case BlockNumberRequestNextAttempt:
        return nextAttemptAudit();
    default:
        warning() << "Unexpected block number request purpose";
        return resultDone();
    }
}


TransactionResult::SharedConst AuditSourceTransaction::runResponseProcessingStage()
{
    info() << "runResponseProcessingStage";
    if (mContext.empty()) {
        warning() << "Contractor don't send response.";

        // check if audit was cancelled
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto keyChain = mKeysStore->keychain(
                            mTrustLines->trustLineID(mContractorID));
        try {
            if (keyChain.isAuditWasCancelled(ioTransaction, mAuditNumber)) {
                info() << "Audit was cancelled by other audit transaction";
                return resultDone();
            }
        } catch (IOError &e) {
            error() << "Attempt to check if audit was cancelled failed. "
                    << "IO transaction can't be completed. Details are: " << e.what();
            throw e;
        }

        if (mCountSendingAttempts < kMaxCountSendingAttempts) {
            info() << "Resending audit message";
            return sendAuditMessage();
        }
        info() << "Transaction will be closed and send ping";
        sendMessage<PingMessage>(
            mContractorID,
            mContractorsManager->idOnContractorSide(mContractorID));
        return resultDone();
    }

    auto message = popNextMessage<AuditResponseMessage>();
    info() << "contractor " << message->idOnReceiverSide << " confirmed audit.";
    if (message->idOnReceiverSide != mContractorID) {
        warning() << "Sender is not contractor of this transaction";
        return resultContinuePreviousState();
    }

    if (!mTrustLines->trustLineIsPresent(mContractorID)) {
        warning() << "Something wrong, because TL must be created";
        // todo : need correct reaction
        return resultDone();
    }

    if (message->state() == ConfirmationMessage::KeysSharingTxPresent) {
        info() << "Keys sharing transaction in pending on contractor side. Audit cancelled.";
        return resultDone();
    }

    if (message->state() == ConfirmationMessage::ReservationsPresentOnTrustLine) {
        info() << "Contractor's TL is not ready for audit yet";
        // message on communicator queue, wait for audit response after reservations committing or cancelling
        // todo add timeout or count failed attempts for running conflict resolver TA
        return resultWaitForMessageTypes(
        {Message::TrustLines_AuditConfirmation},
        kWaitMillisecondsForResponse);
    }

    if (message->state() == ConfirmationMessage::Audit_UpdateTransactionsList) {
        const auto normalizedUpdateList = AuditMessage::normalizedTransactionUUIDs(
            message->transactionUUIDs());
        info() << "Audit update list received with " << normalizedUpdateList.size() << " UUIDs";
        // Validate that all UUIDs belong to the original list.
        for (const auto &transactionUUID : normalizedUpdateList) {
            if (!containsTransactionUUID(mOriginalTransactionList, transactionUUID)) {
                warning() << "Audit update list contains unknown transaction UUID";
                setTrustLineToConflict();
                return resultDone();
            }
        }

        if (mAuditRetryCount >= kMaxAuditRetries) {
            warning() << "Audit update list retry limit exceeded";
            setTrustLineToConflict();
            return resultDone();
        }

        // Check observing window for listed transactions before retry.
        vector<TransactionUUID> expiredTransactions;
        auto observingTransaction = mStorageHandler->beginTransaction();
        for (const auto &transactionUUID : normalizedUpdateList) {
            try {
                const auto effectiveBlockNumber =
                    observingTransaction->paymentTransactionsHandler()->effectiveClaimingBlockNumber(
                        transactionUUID);
                if (mCurrentBlockNumber > effectiveBlockNumber) {
                    expiredTransactions.push_back(transactionUUID);
                }
            } catch (NotFoundError &e) {
                warning() << "Missing payment transaction for "
                          << transactionUUID.stringUUID() << ". Details are: " << e.what();
                expiredTransactions.push_back(transactionUUID);
            } catch (IOError &e) {
                observingTransaction->rollback();
                warning() << "Failed to load payment transaction data. Details are: " << e.what();
                return resultDone();
            }
        }

        if (!expiredTransactions.empty()) {
            warning() << "Audit update list includes transactions outside observing window: "
                      << expiredTransactions.size();
            setTrustLineToConflict();
            return resultDone();
        }

        ++mAuditRetryCount;
        excludeTransactions(normalizedUpdateList);
        mCountSendingAttempts = 0;

        // Replace stored audit part to match the updated transaction list.
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto keyChain = mKeysStore->keychain(
                            mTrustLines->trustLineID(mContractorID));
        try {
            ioTransaction->auditHandler()->deleteAuditByNumber(
                mTrustLines->trustLineID(mContractorID),
                mAuditNumber);

            auto serializedAuditData = getOwnSerializedAuditDataWithTransactionHash();
            mOwnSignature = keyChain.sign(
                ioTransaction,
                serializedAuditData.first,
                serializedAuditData.second);

            keyChain.saveOwnAuditPart(
                ioTransaction,
                mAuditNumber,
                mOwnSignature,
                mTrustLines->incomingTrustAmount(
                    mContractorID),
                mTrustLines->outgoingTrustAmount(
                    mContractorID),
                mAuditBalance);

            mTrustLines->setTrustLineAuditNumber(
                mContractorID,
                mAuditNumber);

        } catch (NotFoundError &e) {
            ioTransaction->rollback();
            warning() << "Attempt to update audit data failed. Details are: " << e.what();
            return resultDone();
        } catch(IOError &e) {
            ioTransaction->rollback();
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::Active);
            warning() << "Attempt to update audit data failed. "
                      << "IO transaction can't be completed. "
                      << "Details are: " << e.what();
            throw e;
        }

        return sendAuditMessage();
    }

    if (message->state() == ConfirmationMessage::Audit_Invalid) {
        warning() << "Contractor reported invalid audit data";
        setTrustLineToConflict();
        return resultDone();
    }

    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));
    try {

        // todo process ConfirmationMessage::OwnKeysAbsent and ConfirmationMessage::ContractorKeysAbsent

        if (message->state() != ConfirmationMessage::OK) {
            warning() << "Contractor didn't accept changing TL. Response code: " << message->state();
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::ConflictResolving,
                ioTransaction);
            // todo run conflict resolving TA
            return resultDone();
        }

        if (message->signature() == nullptr) {
            warning() << "Contractor audit signature is missing";
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::ConflictResolving,
                ioTransaction);
            return resultDone();
        }

        auto contractorSerializedAuditData = getContractorSerializedAuditDataWithTransactionHash();

        if (!keyChain.checkSign(
                    ioTransaction,
                    contractorSerializedAuditData.first,
                    contractorSerializedAuditData.second,
                    message->signature())) {
            warning() << "Contractor didn't sign message correctly";
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::ConflictResolving,
                ioTransaction);
            // todo run conflict resolver TA
            return resultDone();
        }
        info() << "Contractor sign audit correct";

        keyChain.saveContractorAuditPart(
            ioTransaction,
            mAuditNumber,
            message->signature());

        // Update receipt audit numbers and trust line totals atomically.
        ioTransaction->outgoingPaymentReceiptHandler()->updateAuditNumberByTransactionUUIDs(
            mTrustLines->trustLineID(mContractorID),
            mAuditNumber,
            mCurrentTransactionList);
        ioTransaction->incomingPaymentReceiptHandler()->updateAuditNumberByTransactionUUIDs(
            mTrustLines->trustLineID(mContractorID),
            mAuditNumber,
            mCurrentTransactionList);

        // Only excluded receipts are preserved after audit completion.
        mTrustLines->updateTrustLineTotalReceiptsAmounts(
            mContractorID,
            mExcludedIncomingReceiptsAmount,
            mExcludedOutgoingReceiptsAmount);

        if (mTrustLines->isTrustLineEmpty(mContractorID)) {
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::Archived,
                ioTransaction);
            info() << "Trust Line become empty";
        } else {
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::Active,
                ioTransaction);
            info() << "All data saved. Now TL is ready for using";
        }

#ifdef TESTS
        mTrustLinesInfluenceController->testThrowExceptionOnSourceProcessingResponseStage(
            BaseTransaction::AuditSourceTransactionType);
        mTrustLinesInfluenceController->testTerminateProcessOnSourceProcessingResponseStage(
            BaseTransaction::AuditSourceTransactionType);
#endif

    } catch (ValueError &e) {
        ioTransaction->rollback();
        // todo need correct reaction, maybe conflict resolver
        error() << "Attempt to save audit from contractor " << mContractorID << " failed. "
                << "Details are: " << e.what();
        return resultDone();
    } catch (IOError &e) {
        ioTransaction->rollback();
        // todo need correct reaction, maybe conflict resolver
        error() << "Attempt to process confirmation from contractor " << mContractorID << " failed. "
                << "IO transaction can't be completed. Details are: " << e.what();
        throw e;
    }

    mTrustLines->resetAuditRule(mContractorID);
    trustLineActionSignal(
        mContractorID,
        mEquivalent,
        false);
    // Task 20-04: trigger historical crypto data cleanup after audit completion.
    historyCryptoDataCleanupSignal(
        mContractorID,
        mEquivalent);

    return resultDone();
}

TransactionResult::SharedConst AuditSourceTransaction::runContractorPendingStage()
{
    info() << "runContractorPendingStage with " << mContractorID
           << " attempt " << mCountContractorPendingAttempts;

    // check if audit was cancelled
    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));
    try {
        if (keyChain.isAuditWasCancelled(ioTransaction, mAuditNumber)) {
            info() << "Audit was cancelled by other audit transaction";
            return resultDone();
        }
    } catch (IOError &e) {
        error() << "Attempt to check if audit was cancelled failed. "
                << "IO transaction can't be completed. Details are: " << e.what();
        throw e;
    }

    return sendAuditMessage();

}

TransactionResult::SharedConst AuditSourceTransaction::initializeAudit()
{
    info() << "initializeAudit";
    mAuditRetryCount = 0;
    mCountSendingAttempts = 0;

    if (!loadReceiptsAndBuildTransactionList()) {
        warning() << "Failed to load finalized receipts for audit";
        return resultDone();
    }

    recalculateReceiptAmounts();
    info() << "Prepared audit transaction list with " << mCurrentTransactionList.size() << " UUIDs";

    mTrustLines->setTrustLineState(
        mContractorID,
        TrustLine::AuditPending);

    // note: io transaction would commit automatically on destructor call.
    // there is no need to call commit manually.
    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));
    try {
        auto serializedAuditData = getOwnSerializedAuditDataWithTransactionHash();
        mOwnSignature = keyChain.sign(
                                        ioTransaction,
                                        serializedAuditData.first,
                                        serializedAuditData.second);

        keyChain.saveOwnAuditPart(
            ioTransaction,
            mAuditNumber,
            mOwnSignature,
            mTrustLines->incomingTrustAmount(
                mContractorID),
            mTrustLines->outgoingTrustAmount(
                mContractorID),
            mAuditBalance);

        mTrustLines->setTrustLineAuditNumber(
            mContractorID,
            mAuditNumber);

#ifdef TESTS
        mTrustLinesInfluenceController->testThrowExceptionOnSourceInitializationStage(
            BaseTransaction::AuditSourceTransactionType);
        mTrustLinesInfluenceController->testTerminateProcessOnTargetStage(
            BaseTransaction::AuditSourceTransactionType);
#endif

    } catch (NotFoundError &e) {
        ioTransaction->rollback();
        mTrustLines->setTrustLineState(
            mContractorID,
            TrustLine::Active);
        mTrustLines->setIsOwnKeysPresent(mContractorID, false);
        warning() << "Attempt to  outgoing trust line to the node " << mContractorID << " failed. "
                  << "There are no own keys. "
                  << "Details are: " << e.what();

        throw e;
    } catch(IOError &e) {
        ioTransaction->rollback();
        mTrustLines->setTrustLineState(
            mContractorID,
            TrustLine::Active);
        warning() << "Attempt to audit trust line to the node " << mContractorID << " failed. "
                  << "Can't sign audit data. IO transaction can't be completed. "
                  << "Details are: " << e.what();

        // Rethrowing the exception,
        // because the TA can't finish properly and no result may be returned.
        throw e;
    }

    return sendAuditMessage();
}

TransactionResult::SharedConst AuditSourceTransaction::nextAttemptAudit()
{
    info() << "nextAttemptAudit";
    if (mOriginalTransactionList.empty() && !loadReceiptsAndBuildTransactionList()) {
        warning() << "Failed to load finalized receipts for pending audit";
        return resultDone();
    }

    recalculateReceiptAmounts();

    // note: io transaction would commit automatically on destructor call.
    // there is no need to call commit manually.
    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));
    try {
        mOwnSignature = keyChain.getSignatureForPendingAudit(
                                        ioTransaction,
                                        mAuditNumber);
        debug() << "signature getting";

#ifdef TESTS
        mTrustLinesInfluenceController->testThrowExceptionOnSourceResumingStage(
            BaseTransaction::AuditSourceTransactionType);
        mTrustLinesInfluenceController->testTerminateProcessOnSourceResumingStage(
            BaseTransaction::AuditSourceTransactionType);
#endif

    } catch(IOError &e) {
        ioTransaction->rollback();
        warning() << "Attempt to audit trust line to the node " << mContractorID << " failed. "
                  << "Can't get audit data. IO transaction can't be completed. "
                  << "Details are: " << e.what();

        // Rethrowing the exception,
        // because the TA can't finish properly and no result may be returned.
        throw e;
    }

    return sendAuditMessage();
}

TransactionResult::SharedConst AuditSourceTransaction::sendAuditMessage()
{
    // Ensure list is normalized before sending.
    mCurrentTransactionList = AuditMessage::normalizedTransactionUUIDs(
        mCurrentTransactionList);

    sendMessage<AuditMessage>(
        mContractorID,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        mTransactionUUID,
        mAuditNumber,
        mTrustLines->incomingTrustAmount(mContractorID),
        mTrustLines->outgoingTrustAmount(mContractorID),
        mOwnSignature,
        mCurrentTransactionList);
    mCountSendingAttempts++;
    info() << "Send audit message attempt " << mCountSendingAttempts;

    mStep = ResponseProcessing;
    info() << "Switching to response processing stage";
    return resultWaitForMessageTypes(
    {Message::TrustLines_AuditConfirmation},
    kWaitMillisecondsForResponse);
}




void AuditSourceTransaction::setTrustLineToConflict()
{
    auto ioTransaction = mStorageHandler->beginTransaction();
    mTrustLines->setTrustLineState(
        mContractorID,
        TrustLine::Conflict,
        ioTransaction);
    info() << "Trust line moved to Conflict state";
}


const string AuditSourceTransaction::logHeader() const
{
    stringstream s;
    s << "[AuditSourceTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}
