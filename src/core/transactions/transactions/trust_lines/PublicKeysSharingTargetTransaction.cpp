#include "PublicKeysSharingTargetTransaction.h"

PublicKeysSharingTargetTransaction::PublicKeysSharingTargetTransaction(
    PublicKeysSharingInitMessage::Shared message,
    ContractorsManager *contractorsManager,
    TrustLinesManager *manager,
    StorageHandler *storageHandler,
    Keystore *keystore,
    TrustLinesInfluenceController *trustLinesInfluenceController,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::PublicKeysSharingTargetTransactionType,
        message->transactionUUID(),
        message->equivalent(),
        logger),
    mContractorID(message->idOnReceiverSide),
    mSenderIncomingIP(message->senderIncomingIP()),
    mContractorAddresses(message->senderAddresses),
    mContractorsManager(contractorsManager),
    mTrustLines(manager),
    mStorageHandler(storageHandler),
    mKeysStore(keystore),
    mTrustLinesInfluenceController(trustLinesInfluenceController)
{
    mContractorKeysCount = 1; // Single key architecture
    mCurrentPublicKey = message->publicKey();
}

TransactionResult::SharedConst PublicKeysSharingTargetTransaction::run()
{
    switch (mStep) {
    case Stages::Initialization: {
        return runPublicKeyReceiverInitStage();
    }
    case Stages::NextKeyProcessing: {
        return runReceiveNextKeyStage();
    }
    default:
        throw ValueError(logHeader() + "::run: "
                                       "wrong value of mStep " + to_string(mStep));
    }
}

TransactionResult::SharedConst PublicKeysSharingTargetTransaction::runPublicKeyReceiverInitStage()
{
    info() << "runPublicKeyReceiverInitStage " << mContractorID << " sender incoming IP " << mSenderIncomingIP;

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "There is no contractor with requested id";
        return sendKeyErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    if (!mTrustLines->trustLineIsPresent(mContractorID)) {
        warning() << "Trust line is absent.";
        return sendKeyErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    // todo check contractor keys count
    info() << "Contractor keys count " << mContractorKeysCount;

    // Single key architecture - no key number validation needed

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::Archived or
            mTrustLines->trustLineState(mContractorID) == TrustLine::Init) {
        warning() << "invalid TL state " << mTrustLines->trustLineState(mContractorID)
                  << ". Waiting for state updating";
        return sendKeyErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    auto ioTransaction = mStorageHandler->beginTransaction();
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));
    try {
        mCurrentKeysSetSequenceNumber = ioTransaction->contractorKeysHandler()->maxKeySetSequenceNumber(
                                            mTrustLines->trustLineID(mContractorID));
        mCurrentKeysSetSequenceNumber++;
    } catch (NotFoundError& ) {
        mCurrentKeysSetSequenceNumber = 0;
    } catch (IOError &e) {
        ioTransaction->rollback();
        error() << "Can't remove unused contractor keys. Details: " << e.what();
        throw e;
    }

    mTrustLines->setTrustLineState(mContractorID, TrustLine::KeysSharing);

    mStep = NextKeyProcessing;
    return runProcessKey(ioTransaction);
}

TransactionResult::SharedConst PublicKeysSharingTargetTransaction::runReceiveNextKeyStage()
{
    info() << "runReceiveNextKeyStage";
    if (mContext.empty()) {
        warning() << "No next public key message received. Transaction will be closed.";
        return resultDone();
    }

    if (mTrustLines->trustLineState(mContractorID) == TrustLine::Archived) {
        warning() << "invalid TL state " << mTrustLines->trustLineState(mContractorID);
        return sendKeyErrorConfirmation(
                   ConfirmationMessage::ErrorShouldBeRemovedFromQueue);
    }

    auto message = popNextMessage<PublicKeyMessage>();
    info() << "contractor " << message->idOnReceiverSide << " send next key.";
    if (message->idOnReceiverSide != mContractorID) {
        warning() << "Sender is not contractor of this transaction";
        return resultContinuePreviousState();
    }

    // Single key architecture - accept the key directly
    info() << "Received public key";

    mCurrentPublicKey = message->publicKey();

    auto ioTransaction = mStorageHandler->beginTransaction();
    return runProcessKey(ioTransaction);
}

TransactionResult::SharedConst PublicKeysSharingTargetTransaction::runProcessKey(
    IOTransaction::Shared ioTransaction)
{
    info() << "runProcessKeyMessage";
    auto keyChain = mKeysStore->keychain(
                        mTrustLines->trustLineID(mContractorID));
    try {
        keyChain.setContractorPublicKey(
            ioTransaction,
            mCurrentKeysSetSequenceNumber,
            mCurrentPublicKey);

#ifdef TESTS
        mTrustLinesInfluenceController->testThrowExceptionOnSourceInitializationStage(
            BaseTransaction::PublicKeysSharingTargetTransactionType);
        mTrustLinesInfluenceController->testTerminateProcessOnSourceInitializationStage(
            BaseTransaction::PublicKeysSharingTargetTransactionType);
        mTrustLinesInfluenceController->testThrowExceptionOnTargetStage(
            BaseTransaction::PublicKeysSharingTargetTransactionType);
        mTrustLinesInfluenceController->testTerminateProcessOnTargetStage(
            BaseTransaction::PublicKeysSharingTargetTransactionType);
#endif

    } catch (IOError &e) {
        ioTransaction->rollback();
        error() << "Can't store contractor public key. Details " << e.what();
        throw e;
    }
    info() << "Key saved, send hash confirmation";
    // Single key architecture - always send key confirmation
    sendMessageWithTemporaryCaching<PublicKeyHashConfirmation>(
        mContractorID,
        Message::TrustLines_PublicKey,
        kWaitMillisecondsForResponse / 1000 * kMaxCountSendingAttempts,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        mTransactionUUID,
        mCurrentPublicKey->hash());

    try {
        if (keyChain.contractorKeysPresent(ioTransaction)) {
            info() << "All keys received";
            // todo maybe don't save TL state in storage only in memory (don't use ioTransaction and try catch)
            mTrustLines->setTrustLineState(
                mContractorID,
                TrustLine::Active,
                ioTransaction);
            mTrustLines->setIsContractorKeysPresent(
                mContractorID,
                true);
            if (mTrustLines->isTrustLineEmpty(mContractorID) and
                    !keyChain.ownKeysPresent(ioTransaction)) {
                info() << "publicKeysSharing Signal";
                publicKeysSharingSignal(
                    mContractorID,
                    mEquivalent);
            }
            info() << "TL is ready for using";
            return resultDone();
        }
    } catch (IOError &e) {
        ioTransaction->rollback();
        error() << "Can't update TL state. Details " << e.what();
        throw e;
    }
    return resultWaitForMessageTypes(
    {Message::TrustLines_PublicKey},
    kWaitMillisecondsForResponse * kMaxCountSendingAttempts);
}

TransactionResult::SharedConst PublicKeysSharingTargetTransaction::sendKeyErrorConfirmation(
    ConfirmationMessage::OperationState errorState)
{
    sendMessage<PublicKeyHashConfirmation>(
        mContractorID,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        mTransactionUUID,
        errorState);
    return resultDone();
}

const string PublicKeysSharingTargetTransaction::logHeader() const
{
    stringstream s;
    s << "[PublicKeySharingTargetTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}
