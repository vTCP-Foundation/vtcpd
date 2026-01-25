#include "ConfirmChannelTransaction.h"

ConfirmChannelTransaction::ConfirmChannelTransaction(
    InitChannelMessage::Shared message,
    ContractorsManager *contractorsManager,
    StorageHandler *storageHandler,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::ConfirmChannelTransaction,
        message->transactionUUID(),
        0,
        logger),
    mMessage(message),
    mContractorsManager(contractorsManager),
    mStorageHandler(storageHandler)
{}

TransactionResult::SharedConst ConfirmChannelTransaction::run()
{
    info() << "sender incoming IP " << mMessage->senderIncomingIP();
    for (auto &senderAddress : mMessage->senderAddresses) {
        info() << "contractor address " << senderAddress->fullAddress();
    }

    if (mMessage->senderAddresses.empty()) {
        warning() << "Contractor addresses are empty";
        // todo send reply
        return resultDone();
    }

    if (mContractorsManager->selfContractor()->containsAtLeastOneAddress(
                mMessage->senderAddresses)) {
        warning() << "Contractor's addresses contain at least one address is equal to own address";
        // todo : send reply
        return resultDone();
    }

    auto contractorID = mContractorsManager->contractorIDByAddresses(
                            mMessage->senderAddresses);
    if (contractorID == ContractorsManager::kNotFoundContractorID) {
        warning() << "There is no contractor for requested addresses";
        // todo send reply
        return resultDone();
    }
    info() << "Channel ID " << contractorID;

    // Extract initiator payment public key from init message.
    const auto initiatorPaymentPublicKey = mMessage->paymentPublicKey();
    auto ioTransaction = mStorageHandler->beginTransaction();
    // Load own payment public key for channel confirmation message.
    auto paymentPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
    if (paymentPublicKey == nullptr) {
        // Silent termination if own payment key is missing.
        error() << "Cannot retrieve own payment public key for confirmation.";
        return resultDone();
    }

    auto contractor = mContractorsManager->contractor(contractorID);

    try {
        // Persist initiator payment public key for receipt verification.
        contractor->setPaymentPublicKey(initiatorPaymentPublicKey);
        ioTransaction->contractorsHandler()->updatePaymentPublicKey(contractor);
        mContractorsManager->setConfirmationInfo(
            ioTransaction,
            contractorID,
            mMessage->contractorID(),
            mMessage->publicKey());
    } catch (IOError &e) {
        error() << "Error during saving Contractor data. Details: " << e.what();
        ioTransaction->rollback();
        throw e;
    }
    info() << "Channel confirmed";
    sendMessage<ConfirmChannelMessage>(
        contractorID,
        mTransactionUUID,
        contractor,
        paymentPublicKey);
    return resultDone();
}

const string ConfirmChannelTransaction::logHeader() const
{
    stringstream s;
    s << "[ConfirmChannelTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}
