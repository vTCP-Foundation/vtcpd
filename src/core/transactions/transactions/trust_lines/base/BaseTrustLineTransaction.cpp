#include "BaseTrustLineTransaction.h"

#include "../../../../network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "../../../../network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "../../../../network/rpc/requests/SubmitClaimVotesRpcRequest.h"

#include <algorithm>
#include <cstring>
#include <exception>

BaseTrustLineTransaction::BaseTrustLineTransaction(
    const TransactionType type,
    const SerializedEquivalent equivalent,
    ContractorID contractorID,
    ContractorsManager *contractorsManager,
    TrustLinesManager *trustLines,
    StorageHandler *storageHandler,
    Keystore *keystore,
    FeaturesManager *featuresManager,
    TrustLinesInfluenceController *trustLinesInfluenceController,
    Logger &log) :

    BaseTransaction(
        type,
        equivalent,
        log),
    mContractorsManager(contractorsManager),
    mTrustLines(trustLines),
    mStorageHandler(storageHandler),
    mKeysStore(keystore),
    mFeaturesManager(featuresManager),
    mContractorID(contractorID),
    mIncomingReceipts(),
    mOutgoingReceipts(),
    mOriginalTransactionList(),
    mCurrentTransactionList(),
    mExcludedIncomingReceiptsAmount(TrustLine::kZeroAmount()),
    mExcludedOutgoingReceiptsAmount(TrustLine::kZeroAmount()),
    mAuditBalance(TrustLine::kZeroBalance()),
    mTrustLinesInfluenceController(trustLinesInfluenceController)
{
}

BaseTrustLineTransaction::BaseTrustLineTransaction(
    const TransactionType type,
    const TransactionUUID &transactionUUID,
    const SerializedEquivalent equivalent,
    ContractorID contractorID,
    ContractorsManager *contractorsManager,
    TrustLinesManager *trustLines,
    StorageHandler *storageHandler,
    Keystore *keystore,
    FeaturesManager *featuresManager,
    TrustLinesInfluenceController *trustLinesInfluenceController,
    Logger &log) :

    BaseTransaction(
        type,
        transactionUUID,
        equivalent,
        log),
    mContractorsManager(contractorsManager),
    mTrustLines(trustLines),
    mStorageHandler(storageHandler),
    mKeysStore(keystore),
    mFeaturesManager(featuresManager),
    mContractorID(contractorID),
    mIncomingReceipts(),
    mOutgoingReceipts(),
    mOriginalTransactionList(),
    mCurrentTransactionList(),
    mExcludedIncomingReceiptsAmount(TrustLine::kZeroAmount()),
    mExcludedOutgoingReceiptsAmount(TrustLine::kZeroAmount()),
    mAuditBalance(TrustLine::kZeroBalance()),
    mTrustLinesInfluenceController(trustLinesInfluenceController)
{
}

TransactionResult::SharedConst BaseTrustLineTransaction::sendAuditErrorConfirmation(
    ConfirmationMessage::OperationState errorState)
{
    sendMessage<AuditResponseMessage>(
        mContractorID,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        currentTransactionUUID(),
        errorState);
    return resultDone();
}

pair<BytesShared, size_t> BaseTrustLineTransaction::getOwnSerializedAuditData()
{
    size_t bytesCount = sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + kTrustLineBalanceSerializeBytesCount + sizeof(EquivalentRegisterAddressLength) + mFeaturesManager->getEquivalentsRegistryAddress().length();
    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);
    info() << "own audit " << mAuditNumber;

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
            mTrustLines->incomingTrustAmount(
                mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "own incoming amount " << mTrustLines->incomingTrustAmount(mContractorID);

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
            mTrustLines->outgoingTrustAmount(
                mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "own outgoing amount " << mTrustLines->outgoingTrustAmount(mContractorID);

    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
                                            const_cast<TrustLineBalance&>(mTrustLines->balance(mContractorID)));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);
    dataBytesOffset += kTrustLineBalanceSerializeBytesCount;
    info() << "own balance " << mTrustLines->balance(mContractorID);

    auto equivalentRegistryAddress = mFeaturesManager->getEquivalentsRegistryAddress();
    auto equivalentsRegistryAddressLength = (EquivalentRegisterAddressLength)equivalentRegistryAddress.length();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &equivalentsRegistryAddressLength,
        sizeof(EquivalentRegisterAddressLength));
    dataBytesOffset += sizeof(EquivalentRegisterAddressLength);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        equivalentRegistryAddress.c_str(),
        equivalentRegistryAddress.length());
    info() << "own equivalents registry address: " << equivalentRegistryAddress;

    return make_pair(
               dataBytesShared,
               bytesCount);
}

pair<BytesShared, size_t> BaseTrustLineTransaction::getContractorSerializedAuditData()
{
    size_t bytesCount = sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + kTrustLineBalanceSerializeBytesCount + sizeof(EquivalentRegisterAddressLength) + mFeaturesManager->getEquivalentsRegistryAddress().length();
    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);
    info() << "contractor audit " << mAuditNumber;

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
            mTrustLines->outgoingTrustAmount(
                mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "contractor outgoing amount " << mTrustLines->outgoingTrustAmount(mContractorID);

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
            mTrustLines->incomingTrustAmount(
                mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "contractor incoming amount " << mTrustLines->incomingTrustAmount(mContractorID);

    auto contractorBalance = -1 * mTrustLines->balance(mContractorID);
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
                                            const_cast<TrustLineBalance&>(contractorBalance));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);
    dataBytesOffset += kTrustLineBalanceSerializeBytesCount;
    info() << "contractor balance " << contractorBalance;

    auto equivalentRegistryAddress = mFeaturesManager->getEquivalentsRegistryAddress();
    auto equivalentsRegistryAddressLength = (EquivalentRegisterAddressLength)equivalentRegistryAddress.length();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &equivalentsRegistryAddressLength,
        sizeof(EquivalentRegisterAddressLength));
    dataBytesOffset += sizeof(EquivalentRegisterAddressLength);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        equivalentRegistryAddress.c_str(),
        equivalentRegistryAddress.length());
    info() << "contractor equivalents registry address: " << equivalentRegistryAddress;

    return make_pair(
               dataBytesShared,
               bytesCount);
}

bool BaseTrustLineTransaction::requestCurrentBlockNumber(
    bool &requestSent,
    BlockNumber &currentBlockNumber,
    TransactionResult::SharedConst &result)
{
    // Send block number request once and wait for response.
    if (!requestSent) {
        info() << "Requesting current block number";
        sendRpcRequest(
            make_shared<GetBlockNumberRpcRequest>(
                currentTransactionUUID()));
        requestSent = true;
        result = resultWaitForRpcResponse(RpcMethod::GetBlockNumber);
        return false;
    }

    if (!hasRpcResponse()) {
        error() << "GetBlockNumber RPC timed out";
        // TODO: revisit block number retrieval failure handling.
        result = resultDone();
        return false;
    }

    if (mRpcContext.front()->method() != RpcMethod::GetBlockNumber) {
        error() << "Unexpected RPC response in block number stage";
        // TODO: revisit block number retrieval failure handling.
        result = resultDone();
        return false;
    }

    auto response = popRpcResponse<GetBlockNumberRpcResponse>();
    if (response->status() != RpcResponseStatus::Success) {
        error() << "Block number retrieval failed: " << response->errorMessage();
        // TODO: revisit block number retrieval failure handling.
        result = resultDone();
        return false;
    }

    currentBlockNumber = response->blockNumber();
    info() << "Current block number received: " << currentBlockNumber;
    requestSent = false;
    return true;
}

bool BaseTrustLineTransaction::loadReceiptsAndBuildTransactionList()
{
    // Reset cached receipts to avoid mixing audit attempts.
    mIncomingReceipts.clear();
    mOutgoingReceipts.clear();
    mOriginalTransactionList.clear();
    mCurrentTransactionList.clear();

    auto ioTransaction = mStorageHandler->beginTransaction();
    try {
        const auto kTrustLineID = mTrustLines->trustLineID(mContractorID);
        const auto outgoingReceipts =
            ioTransaction->outgoingPaymentReceiptHandler()->getFinalizedReceiptsWithZeroAuditNumber(
                kTrustLineID);
        const auto incomingReceipts =
            ioTransaction->incomingPaymentReceiptHandler()->getFinalizedReceiptsWithZeroAuditNumber(
                kTrustLineID);

        mOutgoingReceipts.reserve(outgoingReceipts.size());
        mIncomingReceipts.reserve(incomingReceipts.size());

        vector<TransactionUUID> transactionUUIDs;
        transactionUUIDs.reserve(outgoingReceipts.size() + incomingReceipts.size());

        for (const auto &receipt : outgoingReceipts) {
            mOutgoingReceipts.emplace_back(
                receipt->transactionUUID(),
                receipt->amount());
            transactionUUIDs.push_back(
                receipt->transactionUUID());
        }

        for (const auto &receipt : incomingReceipts) {
            mIncomingReceipts.emplace_back(
                receipt->transactionUUID(),
                receipt->amount());
            transactionUUIDs.push_back(
                receipt->transactionUUID());
        }

        mOriginalTransactionList = AuditMessage::normalizedTransactionUUIDs(
            transactionUUIDs);
        mCurrentTransactionList = mOriginalTransactionList;
        return true;
    } catch (NotFoundError &e) {
        warning() << "Failed to load receipts for audit. Details are: " << e.what();
        return false;
    } catch (IOError &e) {
        warning() << "Failed to load receipts for audit. Details are: " << e.what();
        return false;
    }
}

void BaseTrustLineTransaction::recalculateReceiptAmounts()
{
    // Normalize list to make membership checks deterministic.
    mCurrentTransactionList = AuditMessage::normalizedTransactionUUIDs(
        mCurrentTransactionList);

    mExcludedIncomingReceiptsAmount = TrustLine::kZeroAmount();
    mExcludedOutgoingReceiptsAmount = TrustLine::kZeroAmount();

    for (const auto &receipt : mIncomingReceipts) {
        if (!containsTransactionUUID(mCurrentTransactionList, receipt.first)) {
            mExcludedIncomingReceiptsAmount =
                mExcludedIncomingReceiptsAmount + receipt.second;
        }
    }

    for (const auto &receipt : mOutgoingReceipts) {
        if (!containsTransactionUUID(mCurrentTransactionList, receipt.first)) {
            mExcludedOutgoingReceiptsAmount =
                mExcludedOutgoingReceiptsAmount + receipt.second;
        }
    }

    // Exclude pending receipts from the audited balance.
    mAuditBalance = mTrustLines->balance(mContractorID)
        - mExcludedIncomingReceiptsAmount + mExcludedOutgoingReceiptsAmount;
}

void BaseTrustLineTransaction::excludeTransactions(
    const vector<TransactionUUID> &transactionUUIDsToExclude)
{
    if (transactionUUIDsToExclude.empty()) {
        return;
    }

    const auto normalizedToExclude = AuditMessage::normalizedTransactionUUIDs(
        transactionUUIDsToExclude);
    vector<TransactionUUID> updatedList;
    updatedList.reserve(mCurrentTransactionList.size());

    for (const auto &transactionUUID : mCurrentTransactionList) {
        if (!containsTransactionUUID(normalizedToExclude, transactionUUID)) {
            updatedList.push_back(transactionUUID);
        }
    }

    mCurrentTransactionList = AuditMessage::normalizedTransactionUUIDs(
        updatedList);
    recalculateReceiptAmounts();
    info() << "Excluded transactions. Current list size: " << mCurrentTransactionList.size();
}

pair<BytesShared, size_t> BaseTrustLineTransaction::getOwnSerializedAuditDataWithTransactionHash() const
{
    const auto transactionListHash =
        AuditMessage::computeTransactionListHash(mCurrentTransactionList);
    const auto equivalentRegistryAddress =
        mFeaturesManager->getEquivalentsRegistryAddress();

    const size_t bytesCount =
        sizeof(AuditNumber) +
        kTrustLineAmountBytesCount +
        kTrustLineAmountBytesCount +
        kTrustLineBalanceSerializeBytesCount +
        AuditMessage::kTransactionUUIDsHashSize +
        sizeof(EquivalentRegisterAddressLength) +
        equivalentRegistryAddress.length();
    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);
    info() << "own audit " << mAuditNumber;

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
        mTrustLines->incomingTrustAmount(
            mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "own incoming amount " << mTrustLines->incomingTrustAmount(mContractorID);

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
        mTrustLines->outgoingTrustAmount(
            mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "own outgoing amount " << mTrustLines->outgoingTrustAmount(mContractorID);

    TrustLineBalance auditBalance = mAuditBalance;
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
        auditBalance);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);
    dataBytesOffset += kTrustLineBalanceSerializeBytesCount;
    info() << "own balance " << mAuditBalance;

    // Add transaction list hash before equivalents registry address.
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        transactionListHash.get(),
        AuditMessage::kTransactionUUIDsHashSize);
    dataBytesOffset += AuditMessage::kTransactionUUIDsHashSize;

    auto equivalentsRegistryAddressLength =
        (EquivalentRegisterAddressLength)equivalentRegistryAddress.length();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &equivalentsRegistryAddressLength,
        sizeof(EquivalentRegisterAddressLength));
    dataBytesOffset += sizeof(EquivalentRegisterAddressLength);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        equivalentRegistryAddress.c_str(),
        equivalentRegistryAddress.length());
    info() << "own equivalents registry address: " << equivalentRegistryAddress;

    return make_pair(
               dataBytesShared,
               bytesCount);
}

pair<BytesShared, size_t> BaseTrustLineTransaction::getContractorSerializedAuditDataWithTransactionHash() const
{
    const auto transactionListHash =
        AuditMessage::computeTransactionListHash(mCurrentTransactionList);
    const auto equivalentRegistryAddress =
        mFeaturesManager->getEquivalentsRegistryAddress();

    const size_t bytesCount =
        sizeof(AuditNumber) +
        kTrustLineAmountBytesCount +
        kTrustLineAmountBytesCount +
        kTrustLineBalanceSerializeBytesCount +
        AuditMessage::kTransactionUUIDsHashSize +
        sizeof(EquivalentRegisterAddressLength) +
        equivalentRegistryAddress.length();
    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);
    info() << "contractor audit " << mAuditNumber;

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
        mTrustLines->outgoingTrustAmount(
            mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "contractor outgoing amount " << mTrustLines->outgoingTrustAmount(mContractorID);

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
        mTrustLines->incomingTrustAmount(
            mContractorID));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;
    info() << "contractor incoming amount " << mTrustLines->incomingTrustAmount(mContractorID);

    TrustLineBalance contractorBalance = -1 * mAuditBalance;
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
        contractorBalance);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);
    dataBytesOffset += kTrustLineBalanceSerializeBytesCount;
    info() << "contractor balance " << contractorBalance;

    // Add transaction list hash before equivalents registry address.
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        transactionListHash.get(),
        AuditMessage::kTransactionUUIDsHashSize);
    dataBytesOffset += AuditMessage::kTransactionUUIDsHashSize;

    auto equivalentsRegistryAddressLength =
        (EquivalentRegisterAddressLength)equivalentRegistryAddress.length();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &equivalentsRegistryAddressLength,
        sizeof(EquivalentRegisterAddressLength));
    dataBytesOffset += sizeof(EquivalentRegisterAddressLength);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        equivalentRegistryAddress.c_str(),
        equivalentRegistryAddress.length());
    info() << "contractor equivalents registry address: " << equivalentRegistryAddress;

    return make_pair(
               dataBytesShared,
               bytesCount);
}

bool BaseTrustLineTransaction::containsTransactionUUID(
    const vector<TransactionUUID> &transactionUUIDs,
    const TransactionUUID &transactionUUID) const
{
    return binary_search(
        transactionUUIDs.begin(),
        transactionUUIDs.end(),
        transactionUUID,
        [](const TransactionUUID &left, const TransactionUUID &right) {
            return memcmp(
                left.data,
                right.data,
                TransactionUUID::kBytesSize) < 0;
        });
}

ContractorID BaseTrustLineTransaction::contractorID() const
{
    return mContractorID;
}

bool BaseTrustLineTransaction::validateUpdateTransactionsList(
    const vector<TransactionUUID> &transactionUUIDs) const
{
    for (const auto &transactionUUID : transactionUUIDs) {
        if (!containsTransactionUUID(mOriginalTransactionList, transactionUUID)) {
            warning() << "Audit update list contains unknown transaction UUID";
            return false;
        }
    }
    return true;
}

BaseTrustLineTransaction::ObservingCheckResult
BaseTrustLineTransaction::checkUpdateListObservingWindow(
    const vector<TransactionUUID> &transactionUUIDs,
    BlockNumber currentBlockNumber)
{
    vector<TransactionUUID> expiredTransactions;
    auto observingTransaction = mStorageHandler->beginTransaction();

    for (const auto &transactionUUID : transactionUUIDs) {
        try {
            const auto effectiveBlockNumber =
                observingTransaction->paymentTransactionsHandler()->effectiveClaimingBlockNumber(
                    transactionUUID);
            if (currentBlockNumber > effectiveBlockNumber) {
                expiredTransactions.push_back(transactionUUID);
            }
        } catch (NotFoundError &e) {
            warning() << "Missing payment transaction for "
                      << transactionUUID.stringUUID() << ". Details are: " << e.what();
            expiredTransactions.push_back(transactionUUID);
        } catch (IOError &e) {
            observingTransaction->rollback();
            warning() << "Failed to load payment transaction data. Details are: " << e.what();
            return ObservingFailed;
        }
    }

    if (!expiredTransactions.empty()) {
        warning() << "Audit update list includes transactions outside observing window: "
                  << expiredTransactions.size();
        return ObservingExpired;
    }

    return ObservingOk;
}

bool BaseTrustLineTransaction::updateOwnAuditDataForCurrentList()
{
    // Refresh stored audit part to reflect updated transaction list.
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
        return false;
    } catch (IOError &e) {
        ioTransaction->rollback();
        mTrustLines->setTrustLineState(
            mContractorID,
            TrustLine::Active);
        warning() << "Attempt to update audit data failed. "
                  << "IO transaction can't be completed. Details are: " << e.what();
        throw e;
    }

    return true;
}

void BaseTrustLineTransaction::sendAuditMessageWithCurrentList()
{
    // Normalize list to ensure deterministic ordering for signatures.
    const auto normalizedList =
        AuditMessage::normalizedTransactionUUIDs(mCurrentTransactionList);

    sendMessage<AuditMessage>(
        mContractorID,
        mEquivalent,
        mContractorsManager->contractor(mContractorID),
        mTransactionUUID,
        mAuditNumber,
        mTrustLines->incomingTrustAmount(mContractorID),
        mTrustLines->outgoingTrustAmount(mContractorID),
        mOwnSignature,
        normalizedList);

    // Keep transaction in response processing stage after sending audit message.
    mStep = ResponseProcessing;
}

void BaseTrustLineTransaction::submitClaimVotesForAsymmetricTransactions(
    const vector<TransactionUUID> &transactionUUIDs)
{
    if (transactionUUIDs.empty()) {
        return;
    }

    // Normalize list to avoid duplicate submissions.
    const auto normalizedTransactions = AuditMessage::normalizedTransactionUUIDs(
        transactionUUIDs);

    try {
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto votesHandler = ioTransaction->paymentParticipantsVotesHandler();
        auto paymentKeysHandler = ioTransaction->paymentKeysHandler();
        auto paymentTransactionsHandler = ioTransaction->paymentTransactionsHandler();

        // Load own payment public key used to verify SubmitClaimVotes payload.
        sphincs::PublicKey::Shared publicKey;
        try {
            publicKey = paymentKeysHandler->getOwnPublicKey();
        } catch (NotFoundError &e) {
            warning() << "Missing payment public key for SubmitClaimVotes. Details are: " << e.what();
            return;
        } catch (IOError &e) {
            warning() << "Failed to load payment public key for SubmitClaimVotes. Details are: " << e.what();
            return;
        }

        // Submit vote signatures for each asymmetrically finalized transaction.
        for (const auto &transactionUUID : normalizedTransactions) {
            BlockNumber maxClaimBlockNumber = 0;
            try {
                maxClaimBlockNumber = paymentTransactionsHandler->maximalClaimingBlockNumber(
                    transactionUUID);
            } catch (NotFoundError &e) {
                warning() << "Missing payment transaction for SubmitClaimVotes "
                          << transactionUUID << ". Details are: " << e.what();
                continue;
            } catch (IOError &e) {
                warning() << "Failed to load maximal claim block number for "
                          << transactionUUID << ". Details are: " << e.what();
                continue;
            }

            auto votes = votesHandler->participantsSignatures(transactionUUID);
            if (votes.empty()) {
                warning() << "No participant signatures for SubmitClaimVotes "
                          << transactionUUID;
                continue;
            }

            auto serializedData = BaseTransaction::serializeSubmitClaimVotesForSigning(
                transactionUUID,
                maxClaimBlockNumber,
                votes,
                publicKey);

            auto signature = mKeysStore->signPaymentTransaction(
                ioTransaction,
                serializedData.first,
                serializedData.second);
            if (!signature.has_value()) {
                warning() << "Failed to sign SubmitClaimVotes for transaction "
                          << transactionUUID;
                continue;
            }

            info() << "Submitting votes for transaction " << transactionUUID;
            sendRpcRequest(
                make_shared<SubmitClaimVotesRpcRequest>(
                    currentTransactionUUID(),
                    transactionUUID,
                    maxClaimBlockNumber,
                    votes,
                    publicKey,
                    *signature));
        }
    } catch (const exception &e) {
        warning() << "Failed to submit claim votes: " << e.what();
    }
}

void BaseTrustLineTransaction::setTrustLineToConflict()
{
    auto ioTransaction = mStorageHandler->beginTransaction();
    mTrustLines->setTrustLineState(
        mContractorID,
        TrustLine::Conflict,
        ioTransaction);
    info() << "Trust line moved to Conflict state";
}
