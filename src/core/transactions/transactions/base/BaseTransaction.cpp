#include "BaseTransaction.h"


BaseTransaction::BaseTransaction(
    const BaseTransaction::TransactionType type,
    Logger &log) :

    mType(type),
    mTransactionUUID(),
    mEquivalent(0),
    mContext(),
    mResources(),
    mStep(1),
    mTimeStarted(utc_now()),
    mLog(log)
{
}

BaseTransaction::BaseTransaction(
    const TransactionType type,
    const TransactionUUID &transactionUUID,
    Logger &log) :

    mType(type),
    mTransactionUUID(transactionUUID),
    mEquivalent(0),
    mContext(),
    mResources(),
    mStep(1),
    mTimeStarted(utc_now()),
    mLog(log)
{
}

BaseTransaction::BaseTransaction(
    BytesShared buffer,
    Logger &log) :
    mType(BaseTransaction::NoEquivalentType),
    mTransactionUUID(),
    mEquivalent(0),
    mContext(),
    mResources(),
    mStep(0),
    mTimeStarted(utc_now()),
    mLog(log)
{
    size_t bytesBufferOffset = 0;

    SerializedTransactionType transactionType;
    memcpy(
        &transactionType,
        buffer.get(),
        sizeof(SerializedTransactionType));
    mType = static_cast<TransactionType>(transactionType);
    bytesBufferOffset += sizeof(SerializedTransactionType);
    //-----------------------------------------------------
    memcpy(
        &mEquivalent,
        buffer.get() + bytesBufferOffset,
        sizeof(SerializedEquivalent));
    bytesBufferOffset += sizeof(SerializedEquivalent);
    //-----------------------------------------------------
    // Read UUID bytes in an alignment-safe way
    mTransactionUUID = TransactionUUID(
        reinterpret_cast<const uint8_t *>(buffer.get() + bytesBufferOffset));
    bytesBufferOffset += TransactionUUID::kBytesSize;
    //-----------------------------------------------------
    SerializedStep step;
    memcpy(
        &step,
        buffer.get() + bytesBufferOffset,
        sizeof(SerializedStep));
    mStep = step;
}

BaseTransaction::BaseTransaction(
    const TransactionType type,
    const SerializedEquivalent equivalent,
    Logger &log) :

    mType(type),
    mTransactionUUID(),
    mEquivalent(equivalent),
    mContext(),
    mResources(),
    mStep(1),
    mTimeStarted(utc_now()),
    mLog(log)
{
    mLog.debug("BaseTransaction") << "BaseTransaction. transactionUUID: " << mTransactionUUID.stringUUID() 
                                << " equivalent: " << mEquivalent;
}

BaseTransaction::BaseTransaction(
    const TransactionType type,
    const TransactionUUID &transactionUUID,
    const SerializedEquivalent equivalent,
    Logger &log) :

    mType(type),
    mTransactionUUID(transactionUUID),
    mEquivalent(equivalent),
    mContext(),
    mResources(),
    mStep(1),
    mTimeStarted(utc_now()),
    mLog(log)
{
}

void BaseTransaction::launchSubsidiaryTransaction(
    BaseTransaction::Shared transaction)
{
    runSubsidiaryTransactionSignal(
        transaction);
}

TransactionResult::Shared BaseTransaction::resultDone () const
{
    return make_shared<TransactionResult>(
               TransactionState::exit());
}

TransactionResult::Shared BaseTransaction::resultFlushAndContinue() const
{
    return make_shared<TransactionResult>(
               TransactionState::flushAndContinue());
}

// todo Change resultWaitForMessageTypes type to sharedConst
TransactionResult::Shared BaseTransaction::resultWaitForMessageTypes(
    vector<Message::MessageType> &&requiredMessagesTypes,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForMessageTypes(
                   move(requiredMessagesTypes),
                   noLongerThanMilliseconds));
}

TransactionResult::Shared BaseTransaction::resultWaitForResourceTypes(
    vector<BaseResource::ResourceType> &&requiredResourcesType,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForResourcesTypes(
                   move(requiredResourcesType),
                   noLongerThanMilliseconds));
}

TransactionResult::Shared BaseTransaction::resultWaitForResourceAndMessagesTypes(
    vector<BaseResource::ResourceType> &&requiredResourcesType,
    vector<Message::MessageType> &&requiredMessagesTypes,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForResourcesAndMessagesTypes(
                   move(requiredResourcesType),
                   move(requiredMessagesTypes),
                   noLongerThanMilliseconds));
}

TransactionResult::Shared BaseTransaction::resultAwakeAfterMilliseconds(
    uint32_t responseWaitTime) const
{
    return make_shared<TransactionResult>(
               TransactionState::awakeAfterMilliseconds(
                   responseWaitTime));
}

TransactionResult::Shared BaseTransaction::resultContinuePreviousState() const
{
    return make_shared<TransactionResult>(
               TransactionState::continueWithPreviousState());
}

TransactionResult::Shared BaseTransaction::resultWaitForMessageTypesAndAwakeAfterMilliseconds(
    vector<Message::MessageType> &&requiredMessagesTypes,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForMessageTypesAndAwakeAfterMilliseconds(
                   move(requiredMessagesTypes),
                   noLongerThanMilliseconds));
}

TransactionResult::Shared BaseTransaction::resultAwakeAsFastAsPossible() const
{
    return make_shared<TransactionResult>(
               TransactionState::awakeAsFastAsPossible());
}

TransactionResult::Shared BaseTransaction::transactionResultFromCommand(
    CommandResult::SharedConst result) const
{
    return make_shared<TransactionResult>(result);
}

TransactionResult::Shared BaseTransaction::transactionResultFromCommandAndWaitForMessageTypes(
    CommandResult::SharedConst result,
    vector<Message::MessageType> &&requiredMessagesTypes,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForMessageTypes(
                   move(requiredMessagesTypes),
                   noLongerThanMilliseconds),
               result);
}

TransactionResult::Shared BaseTransaction::transactionResultFromCommandAndAwakeAfterMilliseconds(
    CommandResult::SharedConst result,
    uint32_t responseWaitTime) const
{
    return make_shared<TransactionResult>(
               TransactionState::awakeAfterMilliseconds(
                   responseWaitTime),
               result);

}

const BaseTransaction::TransactionType BaseTransaction::transactionType() const
{
    return mType;
}

const TransactionUUID &BaseTransaction::currentTransactionUUID () const
{
    return mTransactionUUID;
}

const SerializedEquivalent BaseTransaction::equivalent() const
{
    return mEquivalent;
}

const DateTime BaseTransaction::timeStarted() const
{
    return mTimeStarted;
}

void BaseTransaction::pushContext(
    Message::Shared message)
{
    mContext.push_back(message);
}

void BaseTransaction::pushResource(
    BaseResource::Shared resource)
{
    mResources.push_back(resource);
}

void BaseTransaction::clearContext()
{
    mContext.clear();
}

pair<BytesShared, size_t> BaseTransaction::serializeToBytes() const
{
    size_t bytesCount = sizeof(SerializedTransactionType) +
                        TransactionUUID::kBytesSize +
                        sizeof(SerializedStep) +
                        sizeof(SerializedEquivalent);
    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;
    //-----------------------------------------------------
    SerializedTransactionType transactionType = mType;
    memcpy(
        dataBytesShared.get(),
        &transactionType,
        sizeof(SerializedTransactionType));
    dataBytesOffset += sizeof(SerializedTransactionType);
    //-----------------------------------------------------
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mEquivalent,
        sizeof(SerializedEquivalent));
    dataBytesOffset += sizeof(SerializedEquivalent);
    //-----------------------------------------------------
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mTransactionUUID.begin(),
        TransactionUUID::kBytesSize);
    dataBytesOffset += TransactionUUID::kBytesSize;
    //-----------------------------------------------------
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mStep,
        sizeof(SerializedStep));
    //-----------------------------------------------------
    return make_pair(
               dataBytesShared,
               bytesCount);
}

const size_t BaseTransaction::kOffsetToInheritedBytes()
{
    static const size_t offset = sizeof(SerializedTransactionType)
                                 + sizeof(SerializedEquivalent)
                                 + TransactionUUID::kBytesSize
                                 + sizeof(SerializedStep);
    return offset;
}

LoggerStream BaseTransaction::info() const
{
    return mLog.info(logHeader());
}

LoggerStream BaseTransaction::error() const
{
    return mLog.error(logHeader());
}

LoggerStream BaseTransaction::warning() const
{
    return mLog.warning(logHeader());
}

LoggerStream BaseTransaction::debug() const
{
    return mLog.debug(logHeader());
}

const int BaseTransaction::currentStep() const
{
    return mStep;
}

void BaseTransaction::recreateTransactionUUID()
{
    mTransactionUUID = TransactionUUID();
}
