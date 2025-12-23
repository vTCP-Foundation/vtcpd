#include "CompletedPaymentsObserverMonitoringTransaction.h"

#include "../../../../common/memory/MemoryUtils.h"
#include "../../../../network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "../../../../network/rpc/requests/GetClaimStatusesRpcRequest.h"
#include "../../../../network/rpc/requests/SubmitClaimVotesRpcRequest.h"
#include "../../../../network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "../../../../network/rpc/responses/GetClaimStatusesRpcResponse.h"
#include "../../../../network/rpc/responses/SubmitClaimVotesRpcResponse.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <sstream>

namespace {
/**
 * Reused error message for missing dependencies during monitoring.
 */
constexpr char kMissingDependenciesError[] = "StorageHandler or Keystore is not configured";

/**
 * Compares TransactionUUID values lexicographically by raw bytes.
 */
bool compareTransactionUUID(
    const TransactionUUID &left,
    const TransactionUUID &right)
{
    return memcmp(
        left.data,
        right.data,
        TransactionUUID::kBytesSize) < 0;
}

/**
 * Converts claim state enum to readable label for logging.
 */
const char *claimStateToString(
    GetClaimStatusesRpcResponse::ClaimState state)
{
    switch (state) {
    case GetClaimStatusesRpcResponse::ClaimState::NotFound:
        return "not found";
    case GetClaimStatusesRpcResponse::ClaimState::Observing:
        return "observing";
    case GetClaimStatusesRpcResponse::ClaimState::Approved:
        return "approved";
    case GetClaimStatusesRpcResponse::ClaimState::Rejected:
        return "rejected";
    default:
        return "unknown";
    }
}
}

CompletedPaymentsObserverMonitoringTransaction::CompletedPaymentsObserverMonitoringTransaction(
    StorageHandler *storageHandler,
    crypto::Keystore *keystore,
    Logger &log) :
    BaseTransaction(
        BaseTransaction::Payments_CompletedPaymentsObserverMonitoring,
        log),
    mStorageHandler(storageHandler),
    mKeystore(keystore),
    mCurrentBlockNumber(0),
    mTransactionsForMonitoring(),
    mOwnPublicKey(nullptr),
    mObservingClaims(),
    mPendingResponses(),
    mCurrentBlockRequestSent(false),
    mClaimStatusesRequestSent(false)
{}

TransactionResult::SharedConst CompletedPaymentsObserverMonitoringTransaction::run()
{
    switch (mStep) {
    case GetCurrentBlock:
        return runGetCurrentBlockStage();
    case GetTransactionsForMonitoring:
        return runGetTransactionsForMonitoringStage();
    case GetClaimStatuses:
        return runGetClaimStatusesStage();
    case ProcessClaimStatuses:
        return runProcessClaimStatusesStage();
    case WaitForVotesSubmissionResponses:
        return runWaitForVotesSubmissionResponsesStage();
    default:
        warning() << "Unknown step " << mStep << ", finishing transaction";
        logCompletion();
        return resultDone();
    }
}

const string CompletedPaymentsObserverMonitoringTransaction::logHeader() const
{
    stringstream s;
    s << "[CompletedPaymentsObserverMonitoringTA: "
      << currentTransactionUUID().stringUUID() << "] ";
    return s.str();
}

TransactionResult::SharedConst CompletedPaymentsObserverMonitoringTransaction::runGetCurrentBlockStage()
{
    // Send GetBlockNumber request once and wait for response.
    if (!mCurrentBlockRequestSent) {
        info() << "Requesting current block number from observer";
        sendRpcRequest(
            make_shared<GetBlockNumberRpcRequest>(
                currentTransactionUUID()));
        mCurrentBlockRequestSent = true;
        return resultWaitForRpcResponse(RpcMethod::GetBlockNumber);
    }

    // Timeout case: no response received within wait interval.
    if (!hasRpcResponse()) {
        warning() << "GetBlockNumber RPC timed out";
        logCompletion();
        return resultDone();
    }

    if (!hasExpectedRpcResponse(RpcMethod::GetBlockNumber)) {
        warning() << "Unexpected RPC response in GetCurrentBlock stage";
        logCompletion();
        return resultDone();
    }

    auto response = popRpcResponse<GetBlockNumberRpcResponse>();
    if (response->status() != RpcResponseStatus::Success) {
        warning() << "Observer unavailable while getting block number: "
                  << response->errorMessage();
        logCompletion();
        return resultDone();
    }

    mCurrentBlockNumber = response->blockNumber();
    info() << "Current block number received: " << mCurrentBlockNumber;

    mStep = GetTransactionsForMonitoring;
    return resultContinuePreviousState();
}

TransactionResult::SharedConst CompletedPaymentsObserverMonitoringTransaction::runGetTransactionsForMonitoringStage()
{
    if (mStorageHandler == nullptr) {
        error() << "StorageHandler is not configured";
        logCompletion();
        return resultDone();
    }

    try {
        auto ioTransaction = mStorageHandler->beginTransaction();

        // Load own public key once for all signed RPC requests.
        mOwnPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
        if (mOwnPublicKey == nullptr) {
            error() << "Failed to load own public key for monitoring";
            logCompletion();
            return resultDone();
        }

        // Fetch transactions that are still eligible for observer monitoring.
        mTransactionsForMonitoring =
            ioTransaction->paymentTransactionsHandler()->transactionsForObserverMonitoring(
                mCurrentBlockNumber,
                kMaxTransactionsPerCycle);

        info() << "Transactions for monitoring: " << mTransactionsForMonitoring.size();
        if (mTransactionsForMonitoring.empty()) {
            info() << "No completed transactions require monitoring";
            logCompletion();
            return resultDone();
        }

        mStep = GetClaimStatuses;
        return resultContinuePreviousState();
    } catch (const exception &e) {
        error() << "Failed to load transactions for monitoring: " << e.what();
        logCompletion();
        return resultDone();
    }
}

TransactionResult::SharedConst CompletedPaymentsObserverMonitoringTransaction::runGetClaimStatusesStage()
{
    if (mStorageHandler == nullptr || mKeystore == nullptr) {
        error() << kMissingDependenciesError;
        logCompletion();
        return resultDone();
    }

    // Send GetClaimStatuses request once and wait for response.
    if (!mClaimStatusesRequestSent) {
        if (mOwnPublicKey == nullptr) {
            error() << "Own public key is missing for GetClaimStatuses signing";
            logCompletion();
            return resultDone();
        }

        ClaimsList sortedClaims = mTransactionsForMonitoring;
        sort(
            sortedClaims.begin(),
            sortedClaims.end(),
            [](const auto &left, const auto &right) {
                return compareTransactionUUID(left.first, right.first);
            });

        auto serializedData = serializeGetClaimStatusesForSigning(
            sortedClaims,
            mOwnPublicKey);

        auto ioTransaction = mStorageHandler->beginTransaction();
        auto signature = mKeystore->signPaymentTransaction(
            ioTransaction,
            serializedData.first,
            serializedData.second);
        if (!signature.has_value()) {
            error() << "Failed to sign GetClaimStatuses request";
            logCompletion();
            return resultDone();
        }

        vector<GetClaimStatusesRpcRequest::ClaimInfo> claimsInfo;
        claimsInfo.reserve(sortedClaims.size());
        for (const auto &claim : sortedClaims) {
            claimsInfo.push_back({claim.first, claim.second});
        }

        auto request = make_shared<GetClaimStatusesRpcRequest>(
            currentTransactionUUID(),
            claimsInfo);
        request->setPublicKey(mOwnPublicKey);
        request->setSignature(*signature);

        info() << "Submitting GetClaimStatuses request for "
               << claimsInfo.size() << " claims";
        sendRpcRequest(request);
        mClaimStatusesRequestSent = true;
        return resultWaitForRpcResponse(RpcMethod::GetClaimStatuses);
    }

    // Timeout case: no response received within wait interval.
    if (!hasRpcResponse()) {
        warning() << "GetClaimStatuses RPC timed out";
        logCompletion();
        return resultDone();
    }

    if (!hasExpectedRpcResponse(RpcMethod::GetClaimStatuses)) {
        warning() << "Unexpected RPC response in GetClaimStatuses stage";
        logCompletion();
        return resultDone();
    }

    auto response = popRpcResponse<GetClaimStatusesRpcResponse>();
    if (response->status() != RpcResponseStatus::Success) {
        warning() << "GetClaimStatuses RPC failed: " << response->errorMessage();
        logCompletion();
        return resultDone();
    }

    const auto &statuses = response->statuses();
    info() << "Claim statuses received: " << statuses.size();
    if (statuses.empty()) {
        info() << "No observing claims reported by observer";
        logCompletion();
        return resultDone();
    }

    // Filter observing claims for submission and log skipped statuses.
    mObservingClaims.clear();
    for (const auto &status : statuses) {
        if (status.state == GetClaimStatusesRpcResponse::ClaimState::Observing) {
            mObservingClaims.emplace_back(
                status.transactionUUID,
                status.maxClaimBlockNumber);
            continue;
        }
        info() << "Skipping claim " << status.transactionUUID << " with state "
               << claimStateToString(status.state);
    }

    // Ensure deterministic processing order for sequential response mapping.
    sort(
        mObservingClaims.begin(),
        mObservingClaims.end(),
        [](const auto &left, const auto &right) {
            return compareTransactionUUID(left.first, right.first);
        });

    if (mObservingClaims.empty()) {
        info() << "No observing claims to submit";
        logCompletion();
        return resultDone();
    }

    mStep = ProcessClaimStatuses;
    return resultContinuePreviousState();
}

TransactionResult::SharedConst CompletedPaymentsObserverMonitoringTransaction::runProcessClaimStatusesStage()
{
    if (mStorageHandler == nullptr || mKeystore == nullptr) {
        error() << kMissingDependenciesError;
        logCompletion();
        return resultDone();
    }

    mPendingResponses.clear();
    if (mObservingClaims.empty()) {
        info() << "No observing claims to process";
        logCompletion();
        return resultDone();
    }

    try {
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto votesHandler = ioTransaction->paymentParticipantsVotesHandler();

        // Submit votes for each observing claim.
        for (const auto &claim : mObservingClaims) {
            const auto &transactionUUID = claim.first;
            const auto maxClaimBlockNumber = claim.second;

            auto votes = votesHandler->participantsSignatures(transactionUUID);
            if (votes.empty()) {
                warning() << "No participant signatures for claim "
                          << transactionUUID;
                continue;
            }

            auto serializedData = serializeSubmitClaimVotesForSigning(
                transactionUUID,
                maxClaimBlockNumber,
                votes,
                mOwnPublicKey);

            auto signature = mKeystore->signPaymentTransaction(
                ioTransaction,
                serializedData.first,
                serializedData.second);
            if (!signature.has_value()) {
                warning() << "Failed to sign SubmitClaimVotes for claim "
                          << transactionUUID;
                continue;
            }

            info() << "Submitting votes for claim " << transactionUUID;
            sendRpcRequest(
                make_shared<SubmitClaimVotesRpcRequest>(
                    currentTransactionUUID(),
                    transactionUUID,
                    maxClaimBlockNumber,
                    votes,
                    mOwnPublicKey,
                    *signature));

            mPendingResponses[transactionUUID] = false;
        }
    } catch (const exception &e) {
        error() << "Failed to submit claim votes: " << e.what();
        logCompletion();
        return resultDone();
    }

    if (mPendingResponses.empty()) {
        info() << "No vote submissions were sent";
        logCompletion();
        return resultDone();
    }

    mStep = WaitForVotesSubmissionResponses;
    return resultWaitForRpcResponse(RpcMethod::SubmitClaimVotes);
}

TransactionResult::SharedConst CompletedPaymentsObserverMonitoringTransaction::runWaitForVotesSubmissionResponsesStage()
{
    if (mPendingResponses.empty()) {
        info() << "No pending SubmitClaimVotes responses";
        logCompletion();
        return resultDone();
    }

    // Timeout case: no response received within wait interval.
    if (!hasRpcResponse()) {
        warning() << "SubmitClaimVotes RPC timed out";
        for (auto &pending : mPendingResponses) {
            pending.second = true;
            warning() << "Vote submission timed out for claim " << pending.first;
        }
        logCompletion();
        return resultDone();
    }

    // Process all queued responses in FIFO order.
    while (hasRpcResponse()) {
        if (!hasExpectedRpcResponse(RpcMethod::SubmitClaimVotes)) {
            warning() << "Unexpected RPC response in WaitForVotesSubmissionResponses stage";
            mRpcContext.pop_front();
            continue;
        }

        auto response = popRpcResponse<SubmitClaimVotesRpcResponse>();
        TransactionUUID claimUUID;
        if (!markNextPendingResponseReceived(claimUUID)) {
            warning() << "Received SubmitClaimVotes response without pending claim";
            continue;
        }

        if (response->status() != RpcResponseStatus::Success) {
            warning() << "Vote submission RPC failed for claim " << claimUUID
                      << ": " << response->errorMessage();
            continue;
        }

        if (response->success()) {
            info() << "Vote submission accepted for claim " << claimUUID;
        } else {
            warning() << "Vote submission rejected for claim " << claimUUID
                      << ": " << response->message();
        }
    }

    const bool allResponsesReceived = all_of(
        mPendingResponses.begin(),
        mPendingResponses.end(),
        [](const auto &entry) { return entry.second; });

    if (allResponsesReceived) {
        info() << "All vote submission responses received";
        logCompletion();
        return resultDone();
    }

    return resultWaitForRpcResponse(RpcMethod::SubmitClaimVotes);
}

pair<BytesShared, size_t>
CompletedPaymentsObserverMonitoringTransaction::serializeGetClaimStatusesForSigning(
    const ClaimsList &claims,
    const sphincs::PublicKey::Shared &publicKey)
{
    constexpr size_t kClaimsCountSize = sizeof(uint32_t);
    constexpr size_t kUuidSize = TransactionUUID::kBytesSize;
    constexpr size_t kBlockNumberSize = sizeof(BlockNumber);
    constexpr size_t kPublicKeySize = sphincs::PublicKey::keySize();

    ClaimsList sortedClaims = claims;
    sort(
        sortedClaims.begin(),
        sortedClaims.end(),
        [](const auto &left, const auto &right) {
            return compareTransactionUUID(left.first, right.first);
        });

    const uint32_t claimsCount = static_cast<uint32_t>(sortedClaims.size());
    const size_t serializedDataSize =
        kClaimsCountSize + (sortedClaims.size() * (kUuidSize + kBlockNumberSize)) + kPublicKeySize;
    BytesShared serializedData = tryMalloc(serializedDataSize);

    size_t bytesBufferOffset = 0;
    memcpy(
        serializedData.get() + bytesBufferOffset,
        &claimsCount,
        kClaimsCountSize);
    bytesBufferOffset += kClaimsCountSize;

    for (const auto &claim : sortedClaims) {
        memcpy(
            serializedData.get() + bytesBufferOffset,
            claim.first.data,
            kUuidSize);
        bytesBufferOffset += kUuidSize;

        memcpy(
            serializedData.get() + bytesBufferOffset,
            &claim.second,
            kBlockNumberSize);
        bytesBufferOffset += kBlockNumberSize;
    }

    memcpy(
        serializedData.get() + bytesBufferOffset,
        publicKey->data(),
        kPublicKeySize);

    return make_pair(
        serializedData,
        serializedDataSize);
}

pair<BytesShared, size_t>
CompletedPaymentsObserverMonitoringTransaction::serializeSubmitClaimVotesForSigning(
    const TransactionUUID &transactionUUID,
    const BlockNumber maxClaimBlockNumber,
    const map<PaymentNodeID, sphincs::Signature::Shared> &votes,
    const sphincs::PublicKey::Shared &publicKey)
{
    constexpr size_t kUuidSize = TransactionUUID::kBytesSize;
    constexpr size_t kBlockNumberSize = sizeof(BlockNumber);
    constexpr size_t kVotesCountSize = sizeof(uint32_t);
    constexpr size_t kPaymentNodeIdSize = sizeof(PaymentNodeID);
    constexpr size_t kSignatureSize = sphincs::Signature::signatureSize();
    constexpr size_t kPublicKeySize = sphincs::PublicKey::keySize();

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

bool CompletedPaymentsObserverMonitoringTransaction::hasExpectedRpcResponse(
    RpcMethod method) const
{
    if (mRpcContext.empty()) {
        return false;
    }
    return mRpcContext.front()->method() == method;
}

bool CompletedPaymentsObserverMonitoringTransaction::markNextPendingResponseReceived(
    TransactionUUID &claimUUID)
{
    for (auto &pending : mPendingResponses) {
        if (!pending.second) {
            pending.second = true;
            claimUUID = pending.first;
            return true;
        }
    }
    return false;
}

void CompletedPaymentsObserverMonitoringTransaction::logCompletion() const
{
    info() << "Completed payments observer monitoring transaction completed";
}
