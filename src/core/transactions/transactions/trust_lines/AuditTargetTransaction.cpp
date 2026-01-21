#include "AuditTargetTransaction.h"

#include "../../../network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "../../../network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "../../../common/exceptions/NotFoundError.h"

AuditTargetTransaction::AuditTargetTransaction(
    AuditMessage::Shared message,
    ContractorsManager *contractorsManager,
    TrustLinesManager *manager,
    StorageHandler *storageHandler,
    Keystore *keystore,
    TopologyTrustLinesManager *topologyTrustLinesManager,
    TopologyCacheManager *topologyCacheManager,
    MaxFlowCacheManager *maxFlowCacheManager,
    FeaturesManager *featuresManager,
    TrustLinesInfluenceController *trustLinesInfluenceController,
    Logger &logger):
    BaseTrustLineTransaction(
        BaseTransaction::AuditTargetTransactionType,
        message->transactionUUID(),
        message->equivalent(),
        message->idOnReceiverSide,
        contractorsManager,
        manager,
        storageHandler,
        keystore,
        featuresManager,
        trustLinesInfluenceController,
        logger),
    mMessage(message),
    mSenderIncomingIP(message->senderIncomingIP()),
    mContractorAddresses(message->senderAddresses),
    mTopologyTrustLinesManager(topologyTrustLinesManager),
    mTopologyCacheManager(topologyCacheManager),
    mMaxFlowCacheManager(maxFlowCacheManager),
    mCurrentBlockNumber(0),
    mBlockNumberRequestSent(false),
    mInitiatorTransactionList()
{
    mAuditNumber = mTrustLines->auditNumber(message->idOnReceiverSide) + 1;
    mStep = Initialization;
}

TransactionResult::SharedConst AuditTargetTransaction::run()
{
    switch (mStep) {
    case Stages::Initialization: {
        return runInitializationStage();
    }
    case Stages::BlockNumberRequest: {
        return runBlockNumberRequestStage();
    }
    default:
        throw ValueError(logHeader() + "::run: "
                                       "wrong value of mStep " + to_string(mStep));
    }
}

TransactionResult::SharedConst AuditTargetTransaction::runInitializationStage()
{
    info() << "runInitializationStage " << mContractorID
           << " sender incoming IP " << mSenderIncomingIP;

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "There is no contractor with requested id";
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    if (!mTrustLines->trustLineIsPresent(mContractorID)) {
        warning() << "Trust line is absent.";
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::AuditPending) {
        warning() << "Simultaneous audit";
        if (mContractorsManager->selfContractor()->mainAddress()->fullAddress() >
                mContractorsManager->contractorMainAddress(mContractorID)->fullAddress()) {
            info() << "Current address greater than contractor's. Current transaction would be cancelled";
            return resultDone();
        } else {
            info() << "Current address more less then contractor's. Previous audit would be cancelled";
            auto ioTransaction = mStorageHandler->beginTransaction();
            try {
                auto keyChain = mKeysStore->keychain(
                                    mTrustLines->trustLineID(mContractorID));
                keyChain.removeCancelledOwnAuditPart(ioTransaction);
                mTrustLines->updateTrustLineFromStorage(
                    mContractorID,
                    ioTransaction);
                mTrustLines->setTrustLineState(
                    mContractorID,
                    TrustLine::Active);
                mAuditNumber = mTrustLines->auditNumber(mContractorID) + 1;
                info() << "Previous audit was successfully cancelled";
            } catch (ValueError &e) {
                info() << "Attempt to remove previous audit from the node " << mContractorID << " failed. "
                       << e.what();
                mTrustLines->setTrustLineState(
                    mContractorID,
                    TrustLine::Active);
                info() << "Previous audit was suspended and now successfully cancelled";
            } catch (IOError &e) {
                warning() << "Attempt to remove previous audit from the node " << mContractorID << " failed. "
                          << "IO transaction can't be completed. "
                          << "Details are: " << e.what();
                ioTransaction->rollback();
                // Rethrowing the exception,
                // because the TA can't finish properly and no result may be returned.
                throw e;
            }
        }
    }

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::KeysSharing) {
        info() << "Keys sharing transaction in pending. Audit cancelled.";
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::KeysSharingTxPresent);
    }

    if (mTrustLines->trustLineState(mContractorID) != TrustLine::Active and
            mTrustLines->trustLineState(mContractorID) != TrustLine::Reset) {
        warning() << "Invalid TL state " << mTrustLines->trustLineState(mContractorID);
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineOwnKeysPresent(mContractorID)) {
        warning() << "There are no own keys";
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::OwnKeysAbsent);
    }

    // todo maybe check in storage (keyChain)
    if (!mTrustLines->trustLineContractorKeysPresent(mContractorID)) {
        warning() << "There are no contractor keys";
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ContractorKeysAbsent);
    }

    if (mAuditNumber < mMessage->auditNumber()) {
        warning() << "Contractor's audit number " << mMessage->auditNumber()
                  << " is greater than own " << mAuditNumber;
        // todo : need correct reaction
    }

    if (mAuditNumber - mMessage->auditNumber() > 1) {
        warning() << "Contractor's audit number is deprecated " << mMessage->auditNumber();
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::Audit_IncorrectNumber);
    }

    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));

    if (mAuditNumber - mMessage->auditNumber() == 1) {
        info() << "Contractor send current audit " << mMessage->auditNumber();
        if (!keyChain.isActualAuditFull(ioTransaction)) {
            info() << "Current audit was cancelled";
            keyChain.removeCancelledOwnAuditPart(ioTransaction);
            info() << "Cancelled audit removed. Continue.";
        } else {
            auto ownSignature = keyChain.getCurrentAuditSignature(ioTransaction);
            // Sending confirmation back.
            sendMessageWithTemporaryCaching<AuditResponseMessage>(
                mContractorID,
                Message::TrustLines_Audit,
                kWaitMillisecondsForResponse / 1000 * kMaxCountSendingAttempts,
                mEquivalent,
                mContractorsManager->contractor(mContractorID),
                currentTransactionUUID(),
                ownSignature);

            info() << "Send audit again message";
            return resultDone();
        }
    }

    mPreviousIncomingAmount = mTrustLines->incomingTrustAmount(mContractorID);
    mPreviousOutgoingAmount = mTrustLines->outgoingTrustAmount(mContractorID);
    mPreviousState = mTrustLines->trustLineState(mContractorID);

    return startBlockNumberRequest();
}

TransactionResult::SharedConst AuditTargetTransaction::startBlockNumberRequest()
{
    info() << "startBlockNumberRequest";
    mBlockNumberRequestSent = false;
    mStep = BlockNumberRequest;
    info() << "Switching to block number request stage";
    return runBlockNumberRequestStage();
}

TransactionResult::SharedConst AuditTargetTransaction::runBlockNumberRequestStage()
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
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    if (mRpcContext.front()->method() != RpcMethod::GetBlockNumber) {
        warning() << "Unexpected RPC response in block number stage";
        // TODO: revisit block number retrieval failure handling.
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    auto response = popRpcResponse<GetBlockNumberRpcResponse>();
    if (response->status() != RpcResponseStatus::Success) {
        warning() << "Block number retrieval failed: " << response->errorMessage();
        // TODO: revisit block number retrieval failure handling.
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    mCurrentBlockNumber = response->blockNumber();
    info() << "Current block number received: " << mCurrentBlockNumber;
    mBlockNumberRequestSent = false;

    return runAuditProcessingStage();
}

TransactionResult::SharedConst AuditTargetTransaction::runAuditProcessingStage()
{
    info() << "runAuditProcessingStage";

    mInitiatorTransactionList = AuditMessage::normalizedTransactionUUIDs(
        mMessage->transactionUUIDs());
    info() << "Initiator transaction list size: " << mInitiatorTransactionList.size();

    if (!loadReceiptsAndBuildTransactionList()) {
        warning() << "Failed to load finalized receipts for audit";
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    recalculateReceiptAmounts();
    info() << "Local finalized transaction list size: " << mCurrentTransactionList.size();

    vector<TransactionUUID> unknownTransactions;
    vector<TransactionUUID> notFinalizedLocally;

    // Compare initiator list with finalized local receipts.
    try {
        auto receiptsTransaction = mStorageHandler->beginTransaction();
        for (const auto &transactionUUID : mInitiatorTransactionList) {
            if (!containsTransactionUUID(mCurrentTransactionList, transactionUUID)) {
                if (hasAnyReceiptForTransaction(receiptsTransaction, transactionUUID)) {
                    notFinalizedLocally.push_back(transactionUUID);
                } else {
                    unknownTransactions.push_back(transactionUUID);
                }
            }
        }
    } catch (IOError &e) {
        warning() << "Failed to check receipts presence. Details are: " << e.what();
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    if (!unknownTransactions.empty()) {
        warning() << "Initiator provided unknown finalized transactions: "
                  << unknownTransactions.size();
        setTrustLineToConflict();
        return sendAuditResponse(
                   ConfirmationMessage::Audit_Invalid);
    }

    if (!notFinalizedLocally.empty()) {
        auto normalizedNotFinalized = AuditMessage::normalizedTransactionUUIDs(
            notFinalizedLocally);
        vector<TransactionUUID> cannotObserve;

        auto observingTransaction = mStorageHandler->beginTransaction();
        for (const auto &transactionUUID : normalizedNotFinalized) {
            try {
                const auto effectiveBlockNumber =
                    effectiveClaimingBlockNumber(observingTransaction, transactionUUID);
                if (mCurrentBlockNumber > effectiveBlockNumber) {
                    cannotObserve.push_back(transactionUUID);
                }
            } catch (NotFoundError &e) {
                warning() << "Missing payment transaction for "
                          << transactionUUID.stringUUID() << ". Details are: " << e.what();
                cannotObserve.push_back(transactionUUID);
            } catch (IOError &e) {
                warning() << "Failed to load payment transaction data. Details are: " << e.what();
                return sendAuditErrorConfirmation(
                           ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
            }
        }

        if (!cannotObserve.empty()) {
            warning() << "Not finalized transactions can no longer be observed: "
                      << cannotObserve.size();
            setTrustLineToConflict();
            return sendAuditResponse(
                       ConfirmationMessage::Audit_Invalid);
        }

        info() << "Requesting audit update list for "
               << normalizedNotFinalized.size() << " transactions";
        return sendAuditResponse(
                   ConfirmationMessage::Audit_UpdateTransactionsList,
                   normalizedNotFinalized);
    }

    vector<TransactionUUID> extraOnContractor;
    for (const auto &transactionUUID : mCurrentTransactionList) {
        if (!containsTransactionUUID(mInitiatorTransactionList, transactionUUID)) {
            extraOnContractor.push_back(transactionUUID);
        }
    }

    if (!extraOnContractor.empty()) {
        auto normalizedExtras = AuditMessage::normalizedTransactionUUIDs(
            extraOnContractor);
        vector<TransactionUUID> cannotObserve;

        auto observingTransaction = mStorageHandler->beginTransaction();
        for (const auto &transactionUUID : normalizedExtras) {
            try {
                const auto effectiveBlockNumber =
                    effectiveClaimingBlockNumber(observingTransaction, transactionUUID);
                if (mCurrentBlockNumber > effectiveBlockNumber) {
                    cannotObserve.push_back(transactionUUID);
                }
            } catch (NotFoundError &e) {
                warning() << "Missing payment transaction for "
                          << transactionUUID.stringUUID() << ". Details are: " << e.what();
                cannotObserve.push_back(transactionUUID);
            } catch (IOError &e) {
                warning() << "Failed to load payment transaction data. Details are: " << e.what();
                return sendAuditErrorConfirmation(
                           ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
            }
        }

        if (!cannotObserve.empty()) {
            warning() << "Extra finalized transactions can no longer be observed: "
                      << cannotObserve.size();
            setTrustLineToConflict();
            return sendAuditResponse(
                       ConfirmationMessage::Audit_Invalid);
        }

        // Exclude locally finalized transactions that initiator couldn't observe.
        excludeTransactions(normalizedExtras);
    }

    if (mCurrentTransactionList.size() != mInitiatorTransactionList.size()) {
        warning() << "Transaction list mismatch after reconciliation";
        setTrustLineToConflict();
        return sendAuditResponse(
                   ConfirmationMessage::Audit_Invalid);
    }

    for (size_t i = 0; i < mCurrentTransactionList.size(); ++i) {
        if (mCurrentTransactionList[i] != mInitiatorTransactionList[i]) {
            warning() << "Transaction list order mismatch after reconciliation";
            setTrustLineToConflict();
            return sendAuditResponse(
                       ConfirmationMessage::Audit_Invalid);
        }
    }

    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));

    try {
        // note: io transaction would commit automatically on destructor call.
        // there is no need to call commit manually.

        if (mMessage->outgoingAmount() != mTrustLines->incomingTrustAmount(mContractorID)) {
            info() << "setIncomingTrustLineAmount " << mMessage->outgoingAmount();
            setIncomingTrustLineAmount(ioTransaction);
        }

        if (mMessage->incomingAmount() == TrustLine::kZeroAmount()
                and mTrustLines->outgoingTrustAmount(mContractorID) != TrustLine::kZeroAmount()) {
            info() << "closeOutgoingTrustLine";
            closeOutgoingTrustLine(ioTransaction);
        }

        auto contractorSerializedAuditData = getContractorSerializedAuditDataWithTransactionHash();

        if (!keyChain.checkSign(
                    ioTransaction,
                    contractorSerializedAuditData.first,
                    contractorSerializedAuditData.second,
                    mMessage->signature())) {
            warning() << "Contractor didn't sign message correctly";
            setTrustLineToConflict();
            return sendAuditResponse(
                       ConfirmationMessage::Audit_Invalid);
        }
        info() << "Signature is correct";

        auto serializedAuditData = getOwnSerializedAuditDataWithTransactionHash();
        mOwnSignature = keyChain.sign(
                                        ioTransaction,
                                        serializedAuditData.first,
                                        serializedAuditData.second);

        keyChain.saveFullAudit(
            ioTransaction,
            mAuditNumber,
            mOwnSignature,
            mMessage->signature(),
            mTrustLines->incomingTrustAmount(
                mContractorID),
            mTrustLines->outgoingTrustAmount(
                mContractorID),
            mAuditBalance);

        mTrustLines->setTrustLineAuditNumber(
            mContractorID,
            mAuditNumber);

        const auto kTrustLineID = mTrustLines->trustLineID(mContractorID);
        // Update receipt audit numbers and preserve excluded totals atomically.
        ioTransaction->outgoingPaymentReceiptHandler()->updateAuditNumberByTransactionUUIDs(
            kTrustLineID,
            mAuditNumber,
            mCurrentTransactionList);
        ioTransaction->incomingPaymentReceiptHandler()->updateAuditNumberByTransactionUUIDs(
            kTrustLineID,
            mAuditNumber,
            mCurrentTransactionList);

        mTrustLines->updateTrustLineTotalReceiptsAmounts(
            mContractorID,
            mExcludedIncomingReceiptsAmount,
            mExcludedOutgoingReceiptsAmount);

        mTrustLines->setTrustLineState(
            mContractorID,
            TrustLine::Active,
            ioTransaction);

        if (mTrustLines->isTrustLineEmpty(mContractorID)) {
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::Archived,
                ioTransaction);
            info() << "Trust Line become empty";
        } else {
            info() << "All data saved. Now TL is ready for using";
        }

#ifdef TESTS
        mTrustLinesInfluenceController->testThrowExceptionOnTargetStage(
            BaseTransaction::AuditTargetTransactionType);
        mTrustLinesInfluenceController->testTerminateProcessOnTargetStage(
            BaseTransaction::AuditTargetTransactionType);
#endif

    } catch (ValueError &e) {
        warning() << "Attempt to change trust line from the node " << mContractorID << " failed. "
                  << e.what();
        return sendAuditErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    } catch (IOError &e) {
        ioTransaction->rollback();
        mTrustLines->setIncoming(
            mContractorID,
            mPreviousIncomingAmount);
        mTrustLines->setOutgoing(
            mContractorID,
            mPreviousOutgoingAmount);
        mTrustLines->setTrustLineState(
            mContractorID,
            mPreviousState);
        warning() << "Attempt to change trust line to the node " << mContractorID << " failed. "
                  << "IO transaction can't be completed. "
                  << "Details are: " << e.what();

        // Rethrowing the exception,
        // because the TA can't finish properly and no result may be returned.
        throw e;
    }

    // Sending confirmation back.
    sendMessageWithTemporaryCaching<AuditResponseMessage>(
        mContractorID,
        Message::TrustLines_Audit,
        kWaitMillisecondsForResponse / 1000 * kMaxCountSendingAttempts,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        currentTransactionUUID(),
        mOwnSignature);
    info() << "Send audit message";

    mTrustLines->resetAuditRule(mContractorID);
    trustLineActionSignal(
        mContractorID,
        mEquivalent,
        false);

    return resultDone();
}

void AuditTargetTransaction::setTrustLineToConflict()
{
    auto ioTransaction = mStorageHandler->beginTransaction();
    mTrustLines->setTrustLineState(
        mContractorID,
        TrustLine::Conflict,
        ioTransaction);
    info() << "Trust line moved to Conflict state";
}

bool AuditTargetTransaction::hasAnyReceiptForTransaction(
    IOTransaction::Shared ioTransaction,
    const TransactionUUID &transactionUUID) const
{
    return ioTransaction->outgoingPaymentReceiptHandler()->isContainsTransaction(transactionUUID)
        || ioTransaction->incomingPaymentReceiptHandler()->isContainsTransaction(transactionUUID);
}

BlockNumber AuditTargetTransaction::effectiveClaimingBlockNumber(
    IOTransaction::Shared ioTransaction,
    const TransactionUUID &transactionUUID) const
{
    return ioTransaction->paymentTransactionsHandler()->effectiveClaimingBlockNumber(
        transactionUUID);
}

TransactionResult::SharedConst AuditTargetTransaction::sendAuditResponse(
    ConfirmationMessage::OperationState state,
    const vector<TransactionUUID> &transactionUUIDs)
{
    if (state == ConfirmationMessage::Audit_UpdateTransactionsList) {
        const auto normalizedUUIDs = AuditMessage::normalizedTransactionUUIDs(
            transactionUUIDs);
        sendMessage<AuditResponseMessage>(
            mContractorID,
            mEquivalent,
            mContractorsManager->contractor(mContractorID),
            currentTransactionUUID(),
            state,
            normalizedUUIDs);
        return resultDone();
    }

    sendMessage<AuditResponseMessage>(
        mContractorID,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        currentTransactionUUID(),
        state);
    return resultDone();
}

void AuditTargetTransaction::setIncomingTrustLineAmount(
    IOTransaction::Shared ioTransaction)
{
    auto kOperationResult = mTrustLines->setIncoming(
                                mContractorID,
                                mMessage->outgoingAmount());
    switch (kOperationResult) {
    case TrustLinesManager::TrustLineOperationResult::Updated: {
        populateHistory(ioTransaction, TrustLineRecord::Updating);
        mTopologyCacheManager->resetInitiatorCache();
        mMaxFlowCacheManager->clearCashes();
        info() << "Incoming trust line from the node " << mContractorID
               << " has been successfully set to " << mMessage->outgoingAmount();
        break;
    }

    case TrustLinesManager::TrustLineOperationResult::Closed: {
        populateHistory(ioTransaction, TrustLineRecord::Rejecting);
        // remove this TL from Topology TrustLines Manager
        mTopologyTrustLinesManager->addTrustLine(
            make_shared<TopologyTrustLine>(
                0,
                mContractorID,
                make_shared<const TrustLineAmount>(0)));
        mTopologyCacheManager->resetInitiatorCache();
        mMaxFlowCacheManager->clearCashes();
        info() << "Incoming trust line from the node " << mContractorID
               << " has been successfully closed.";
        break;
    }

    case TrustLinesManager::TrustLineOperationResult::NoChanges: {
        // It is possible, that set trust line request will arrive twice or more times,
        // but only first processed update must be written to the trust lines history.
        info() << "Incoming trust line from the node " << mContractorID
               << " has not been changed.";
        break;
    }
    default: {
        warning() << "Invalid operation result " << kOperationResult << ". History wouldn't be recorded";
    }
    }
}

void AuditTargetTransaction::closeOutgoingTrustLine(
    IOTransaction::Shared ioTransaction)
{
    mTrustLines->closeOutgoing(
        mContractorID);
    populateHistory(ioTransaction, TrustLineRecord::RejectingOutgoing);
    mTopologyCacheManager->resetInitiatorCache();
    mMaxFlowCacheManager->clearCashes();
    info() << "Outgoing trust line to the node " << mContractorID
           << " has been successfully closed by remote node.";
}

void AuditTargetTransaction::populateHistory(
    IOTransaction::Shared ioTransaction,
    TrustLineRecord::TrustLineOperationType operationType)
{
#ifndef TESTS
    TrustLineRecord::Shared record;
    if (operationType != TrustLineRecord::RejectingOutgoing) {
        record = make_shared<TrustLineRecord>(
                     mTransactionUUID,
                     operationType,
                     mContractorsManager->contractor(mContractorID),
                     mMessage->outgoingAmount());
    } else {
        record = make_shared<TrustLineRecord>(
                     mTransactionUUID,
                     operationType,
                     mContractorsManager->contractor(mContractorID));
    }

    ioTransaction->historyStorage()->saveTrustLineRecord(
        record,
        mEquivalent);
#endif
}

const string AuditTargetTransaction::logHeader() const
{
    stringstream s;
    s << "[AuditTargetTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}
