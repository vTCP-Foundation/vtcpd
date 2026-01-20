#include "BaseTrustLineTransaction.h"

#include <algorithm>
#include <cstring>

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
