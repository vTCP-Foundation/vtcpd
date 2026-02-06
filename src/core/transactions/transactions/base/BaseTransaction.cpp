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

/**
 * Emits RPC request signal so the caller can route it to the observer.
 */
void BaseTransaction::sendRpcRequest(
    RpcRequest::Shared request) const
{
    outgoingRpcRequestSignal(
        request);
}

/**
 * Serializes claim vote data for SubmitClaimVotes signing in canonical order.
 */
pair<BytesShared, size_t> BaseTransaction::serializeSubmitClaimVotesForSigning(
    const TransactionUUID &transactionUUID,
    BlockNumber maxClaimBlockNumber,
    const map<PaymentNodeID, crypto::sphincs::Signature::Shared> &votes,
    const crypto::sphincs::PublicKey::Shared &publicKey)
{
    constexpr size_t kUuidSize = TransactionUUID::kBytesSize;
    constexpr size_t kBlockNumberSize = sizeof(BlockNumber);
    constexpr size_t kVotesCountSize = sizeof(uint32_t);
    constexpr size_t kPaymentNodeIdSize = sizeof(PaymentNodeID);
    constexpr size_t kSignatureSize = crypto::sphincs::Signature::signatureSize();
    constexpr size_t kPublicKeySize = crypto::sphincs::PublicKey::keySize();

    const uint32_t votesCount = static_cast<uint32_t>(votes.size());
    const size_t serializedDataSize =
        kUuidSize + kBlockNumberSize + kVotesCountSize +
        (votes.size() * (kPaymentNodeIdSize + kSignatureSize)) + kPublicKeySize;
    BytesShared serializedData = tryMalloc(serializedDataSize);

    size_t bytesBufferOffset = 0;
    memcpy(
        serializedData.get() + bytesBufferOffset,
        transactionUUID.data,
        kUuidSize);
    bytesBufferOffset += kUuidSize;

    memcpy(
        serializedData.get() + bytesBufferOffset,
        &maxClaimBlockNumber,
        kBlockNumberSize);
    bytesBufferOffset += kBlockNumberSize;

    memcpy(
        serializedData.get() + bytesBufferOffset,
        &votesCount,
        kVotesCountSize);
    bytesBufferOffset += kVotesCountSize;

    for (const auto &vote : votes) {
        memcpy(
            serializedData.get() + bytesBufferOffset,
            &vote.first,
            kPaymentNodeIdSize);
        bytesBufferOffset += kPaymentNodeIdSize;

        memcpy(
            serializedData.get() + bytesBufferOffset,
            vote.second->data(),
            kSignatureSize);
        bytesBufferOffset += kSignatureSize;
    }

    memcpy(
        serializedData.get() + bytesBufferOffset,
        publicKey->data(),
        kPublicKeySize);

    return make_pair(
        serializedData,
        serializedDataSize);
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

/**
 * Constructs TransactionResult that waits for a specific RPC response.
 */
TransactionResult::Shared BaseTransaction::resultWaitForRpcResponse(
    RpcMethod method,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForRpcResponse(
                   method,
                   noLongerThanMilliseconds));
}

TransactionResult::Shared BaseTransaction::resultWaitForRpcResponseAndMessagesTypes(
    RpcMethod method,
    vector<Message::MessageType> &&requiredMessagesTypes,
    uint32_t noLongerThanMilliseconds) const
{
    return make_shared<TransactionResult>(
               TransactionState::waitForRcpResponseAndMessagesTypes(
                   method,
                   move(requiredMessagesTypes),
                   noLongerThanMilliseconds));
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

/**
 * Adds an RPC response to the FIFO queue. Returns false when the queue is full.
 */
bool BaseTransaction::pushRpcResponse(
    RpcResponse::Shared response)
{
    if (!response) {
        throw RuntimeError(
            "BaseTransaction::pushRpcResponse: response is null");
    }

    if (mRpcContext.size() >= kMaxRpcResponsesPerTransaction) {
        // Preserve bounded memory while still surfacing overflow as an error to the transaction.
        // When we receive a synthesized RpcError due to overflow, clear the queue and store it
        // so the transaction can react to the failure immediately.
        if (response->status() == RpcResponseStatus::RpcError) {
            mRpcContext.clear();
            mRpcContext.push_back(response);
        }
        return false;
    }

    mRpcContext.push_back(response);
    return true;
}

/**
 * Checks if there is at least one pending RPC response.
 */
bool BaseTransaction::hasRpcResponse() const
{
    return !mRpcContext.empty();
}

/**
 * Returns the number of queued RPC responses.
 */
size_t BaseTransaction::rpcResponsesCount() const
{
    return mRpcContext.size();
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
