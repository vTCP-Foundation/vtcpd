#include "ObservingHandler.h"

#include "../common/exceptions/NotFoundError.h"

ObservingHandler::ObservingHandler(
    vector<pair<string, string>> observersAddressesStr,
    IOCtx &ioCtx,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    Logger &logger) :
    LoggerMixin(logger),
    mObservingCommunicator(
        make_unique<ObservingCommunicator>(
            ioCtx,
            logger)),
    mObserverRPCClient(
        make_unique<ObserverRPCClient>(
            ioCtx,
            logger)),
    mBlockNumberRequestTimer(ioCtx),
    mClaimsTimer(ioCtx),
    mTransactionsTimer(ioCtx),
    mPaymentClaimsTimer(ioCtx),
    mSuccessfulTransactionsMonitorTimer(ioCtx),
    mRequestsTimer(ioCtx),
    mStorageHandler(storageHandler),
    mResourcesManager(resourcesManager)
{
    // Filter and add only IPv4 addresses to mObservers
    for (const auto &addressPair : observersAddressesStr) {
        const string &addressType = addressPair.first;
        const string &addressStr = addressPair.second;

        if (addressType == "ipv4") {
            try {
                auto ipv4Address = make_shared<IPv4WithPortAddress>(addressStr);
                mObservers.push_back(ipv4Address);
#ifdef DEBUG_LOG_OBSEVING_HANDLER
                debug() << "Added observer: " << addressStr;
#endif
            } catch (const std::exception &e) {
                warning() << "Failed to parse IPv4 address " << addressStr << ": " << e.what();
            }
        } else {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
            debug() << "Skipping non-IPv4 address type: " << addressType;
#endif
        }
    }

    if (mObservers.empty()) {
        warning() << "No valid IPv4 observers configured";
    } else {
        info() << "Configured " << mObservers.size() << " observer(s)";
    }

#ifdef TESTS
    mPaymentClaimsTimer.expires_after(
        chrono::seconds(kPaymentClaimProcessingPeriodSecondsTests));
#else
    mPaymentClaimsTimer.expires_after(
        chrono::seconds(kPaymentClaimProcessingPeriodSeconds));
#endif

    mPaymentClaimsTimer.async_wait(
        [this](const boost::system::error_code &e) {

            if (e == boost::asio::error::operation_aborted) {
            return;
        }

        processPaymentClaims();
        });

    // Start timer for monitoring successful transactions and related observer claims.
#ifdef TESTS
    mSuccessfulTransactionsMonitorTimer.expires_after(
        chrono::seconds(kSuccessfulTransactionsMonitoringPeriodSecondsTests));
#else
    mSuccessfulTransactionsMonitorTimer.expires_after(
        chrono::seconds(kSuccessfulTransactionsMonitoringPeriodSeconds));
#endif

    mSuccessfulTransactionsMonitorTimer.async_wait(
        [this](const boost::system::error_code &e) {
            if (e == boost::asio::error::operation_aborted) {
                return;
            }
            monitorSuccessfulTransactions();
        });
}

void ObservingHandler::sendClaimRequestToObservers(
    ObservingClaimAppendRequestMessage::Shared request)
{}

void ObservingHandler::addTransactionForChecking(
    const TransactionUUID &transactionUUID,
    BlockNumber maxBlockNumberForClaiming)
{}

void ObservingHandler::requestActualBlockNumber(
    const TransactionUUID &transactionUUID)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "requestActualBlockNumber for " << transactionUUID;
#endif
    mRequestsTimer.expires_after(
        std::chrono::milliseconds(
            5));
    mRequestsTimer.async_wait(
        boost::bind(
            &ObservingHandler::responseActualBlockNumber,
            this,
            transactionUUID));
}

void ObservingHandler::initialObservingRequest()
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "initialObservingRequest";
#endif
}

const DateTime ObservingHandler::closestClaimPerformingTimestamp() const
noexcept
{
    if (mClaims.empty()) {
        return utc_now() + boost::posix_time::seconds(2);
    }

    DateTime nextClearingDateTime = mClaims.begin()->second->nextActionDateTime();
    for (const auto &claim : mClaims) {
        const auto kQueueNextAttemptPlanned = claim.second->nextActionDateTime();
        if (kQueueNextAttemptPlanned < nextClearingDateTime) {
            nextClearingDateTime = kQueueNextAttemptPlanned;
        }
    }
    return nextClearingDateTime;
}

void ObservingHandler::rescheduleResending()
{
    if (mClaims.empty()) {

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        this->debug() << "There are no claims present. "
                         "Cleaning would not be scheduled any more.";
#endif

        return;
    }

    const auto kCleaningTimeout = closestClaimPerformingTimestamp() - utc_now();
    mClaimsTimer.expires_after(chrono::microseconds(kCleaningTimeout.total_microseconds()));
    mClaimsTimer.async_wait([this] (const boost::system::error_code &e) {

        if (e == boost::asio::error::operation_aborted) {
            return;
        }

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        this->debug() << "Actions performing started.";
#endif

        this->performActions();
        this->rescheduleResending();

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        this->debug() << "Actions performing finished.";
#endif
    });
}

void ObservingHandler::performActions()
{
    const auto now = utc_now();

    for (const auto &claim : mClaims) {
        if (claim.second->nextActionDateTime() > now) {
            // This claim's timeout is not fired up yet.
            continue;
        }

        if (claim.second->observingResponseType() == ObservingTransaction::ParticipantsVotesPresent) {
            // todo : need correct reaction
        }
        if (performOneClaim(claim.second)) {
            mClaims.erase(claim.first);
        }
    }
}

bool ObservingHandler::performOneClaim(
    ObservingTransaction::Shared observingTransaction)
{
    debug() << "performOneClaim " << observingTransaction->transactionUUID()
            << " type " << observingTransaction->observingResponseType()
            << " maximalBlockN " << observingTransaction->observingRequestMessage()->maximalClaimingBlockNumber();
    if (observingTransaction->observingResponseType() == ObservingTransaction::NoInfo) {
        sendClaimAgain(
            observingTransaction);
        return false;
    }

    vector<pair<TransactionUUID, BlockNumber>> requestedTransactions;
    requestedTransactions.emplace_back(
        observingTransaction->transactionUUID(),
        observingTransaction->observingRequestMessage()->maximalClaimingBlockNumber());
    auto claimCheck = make_shared<ObservingTransactionsRequestMessage>(
                          requestedTransactions);

    BytesShared observerResponse;
    for (const auto &observer : mObservers) {
        if (mObservers.size() > 1 and observer == observingTransaction->requestedObserver()) {
            // we check if claim in block on all observers except those which accepted claim
            continue;
        }
        try {
            observerResponse = mObservingCommunicator->sendRequestToObserver(
                                   observer,
                                   claimCheck);
        } catch (std::exception &e) {
            this->warning() << "Can't send request to observer " << e.what();
            continue;
        } catch (...) {
            this->warning() << "Can't send request to observer";
            continue;
        }
        ObservingTransaction::ObservingResponseType observingResponseType;
        try {
            auto actualTransactionStateResponse = make_shared<ObservingTransactionsResponseMessage>(
                    observerResponse);
            mLastUpdatedBlockNumber = make_pair(
                                          actualTransactionStateResponse->actualBlockNumber(),
                                          utc_now());

            debug() << "Actual block number " << actualTransactionStateResponse->actualBlockNumber();
            debug() << "Observer response " << actualTransactionStateResponse->transactionsResponses().size();
            if (actualTransactionStateResponse->transactionsResponses().size() > 1) {
                warning() << "Size of received transactions is invalid";
                continue;
            }
            observingResponseType = actualTransactionStateResponse->transactionsResponses().at(0);
        } catch (std::exception &e) {
            this->warning() << "Can't parse observer response " << e.what();
            continue;
        }
#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Transaction state " << observingResponseType;
#endif
        if (observingResponseType == ObservingTransaction::NoInfo) {
            debug() << "No Info";
            // todo : need correct reaction
        } else if (observingResponseType == ObservingTransaction::ClaimInPool or
                   observingResponseType == ObservingTransaction::ClaimInBlock) {
            info() << "ClaimInBlock";
            observingTransaction->setObservingResponseType(observingResponseType);
            if (mLastUpdatedBlockNumber.first >
                    observingTransaction->observingRequestMessage()->maximalClaimingBlockNumber()) {
                info() << "Claiming time has expired, transaction rejected";
                mRejectTransactionSignal(
                    observingTransaction->transactionUUID(),
                    observingTransaction->observingRequestMessage()->maximalClaimingBlockNumber());
                auto ioTransaction = mStorageHandler->beginTransaction();
                ioTransaction->paymentTransactionsHandler()->updateTransactionState(
                    observingTransaction->transactionUUID(),
                    ObservingTransaction::RejectedByObserving);
                return true;
            }
            observingTransaction->rescheduleNextActionTime();
            return false;
        } else if (observingResponseType == ObservingTransaction::ParticipantsVotesPresent) {
            info() << "ParticipantsVotesPresent";
            if (!getParticipantsVotes(
                        observingTransaction->transactionUUID(),
                        observingTransaction->observingRequestMessage()->maximalClaimingBlockNumber())) {
                observingTransaction->rescheduleNextActionSmallTime();
                return false;
            }
            return true;
        } else {
            warning() << "Unexpected transaction state " << observingResponseType;
            continue;
        }
    }

    warning() << "Can't send request to all observers";
    observingTransaction->rescheduleNextActionSmallTime();
    return false;
}

void ObservingHandler::sendClaimAgain(
    ObservingTransaction::Shared observingTransaction)
{
    debug() << "sendClaimAgain " << observingTransaction->transactionUUID();
    for (const auto &observer : mObservers) {
        BytesShared observingResponse;
        try {
            observingResponse = mObservingCommunicator->sendRequestToObserver(
                                    observer,
                                    observingTransaction->observingRequestMessage());
        } catch (std::exception &e) {
            this->warning() << "Can't send request to observer " << e.what();
            continue;
        } catch (...) {
            this->warning() << "Can't send request to observer";
            continue;
        }
        try {
            auto claimAppendResponse = make_shared<ObservingClaimAppendResponseMessage>(
                                           observingResponse);
            info() << "claimAppendResponse " << claimAppendResponse->observingResponse();

            if (claimAppendResponse->observingResponse() == ObservingTransaction::NoInfo) {
                continue;
            }
            // if claim period has expired, then transaction should stay serialized and hold reservations forever
            if (claimAppendResponse->observingResponse() == ObservingTransaction::ClaimTimeExpired) {
                info() << "Claim period has expired";
                mUncertainTransactionSignal(
                    observingTransaction->transactionUUID(),
                    observingTransaction->observingRequestMessage()->maximalClaimingBlockNumber());
                mClaims.erase(observingTransaction->transactionUUID());
                return;
            }

            observingTransaction->addRequestedObserver(
                observer);
            observingTransaction->setObservingResponseType(
                ObservingTransaction::ClaimInPool);
            observingTransaction->rescheduleNextActionTime();
            return;
        } catch (std::exception &e) {
            this->warning() << "Can't parse observer response " << e.what();
            continue;
        }
    }

    warning() << "Can't send claim to all observers";
    observingTransaction->rescheduleNextActionSmallTime();
}

bool ObservingHandler::getParticipantsVotes(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber)
{
    info() << "getParticipantsVotes " << transactionUUID;
    auto getTSLRequest = make_shared<ObservingParticipantsVotesRequestMessage>(
                             transactionUUID,
                             maximalClaimingBlockNumber);
    for (const auto &observer : mObservers) {
        BytesShared observerResponse;
        try {
            observerResponse = mObservingCommunicator->sendRequestToObserver(
                                   observer,
                                   getTSLRequest);
        } catch (std::exception &e) {
            this->warning() << "Can't send request to observer " << e.what();
            continue;
        } catch (...) {
            this->warning() << "Can't send request to observer";
            continue;
        }
        try {
            auto participantsVotesMessage = make_shared<ObservingParticipantsVotesResponseMessage>(
                                                observerResponse);
            if (!participantsVotesMessage->isParticipantsVotesPresent()) {
                warning() << "ParticipantsVotes are absent";
                continue;
            }
            info() << "Receive participants votes " << participantsVotesMessage->transactionUUID() << " "
                   << participantsVotesMessage->maximalClaimingBlockNumber() << " "
                   << participantsVotesMessage->participantsSignatures().size();
            // todo : check if participantsVotesMessage is correct
            mParticipantsVotesSignal(
                transactionUUID,
                maximalClaimingBlockNumber,
                participantsVotesMessage->participantsSignatures());
        } catch (std::exception &e) {
            this->warning() << "Can't parse observer response " << e.what();
            continue;
        }
        auto ioTransaction = mStorageHandler->beginTransaction();
        ioTransaction->paymentTransactionsHandler()->updateTransactionState(
            transactionUUID,
            ObservingTransaction::ParticipantsVotesPresent);
        return true;
    }

    warning() << "Can't get ParticipantsVotes from all observers";
    return false;
}

void ObservingHandler::runTransactionsChecking(
    const boost::system::error_code &errorCode)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "runTransactionsChecking";
#endif
}

bool ObservingHandler::sendParticipantsVoteMessageToObservers(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber)
{
    info() << "sendParticipantsVoteMessageToObservers " << transactionUUID << " " << maximalClaimingBlockNumber;
    auto ioTransaction = mStorageHandler->beginTransaction();
    auto participantsSignatures = ioTransaction->paymentParticipantsVotesHandler()->participantsSignatures(
                                      transactionUUID);
    if (participantsSignatures.empty()) {
        warning() << "Empty participants signatures";
        // todo : need correct reaction
        return false;
    }
    auto participantsVotesAppendRequest = make_shared<ObservingParticipantsVotesAppendRequestMessage>(
            transactionUUID,
            maximalClaimingBlockNumber,
            participantsSignatures);
    BytesShared observerResponse;
    for (const auto &observer : mObservers) {
        try {
            observerResponse = mObservingCommunicator->sendRequestToObserver(
                                   observer,
                                   participantsVotesAppendRequest);
        } catch (std::exception &e) {
            this->warning() << "Can't send request to observers " << e.what();
            continue;
        } catch (...) {
            this->warning() << "Can't send request to observer";
            continue;
        }
        try {
            auto participantsVotesAppendResponse = make_shared<ObservingParticipantsVotesAppendResponseMessage>(
                    observerResponse);
            info() << "participantsVotesAppendResponse " << participantsVotesAppendResponse->observingResponse();
            if (participantsVotesAppendResponse->observingResponse() == ObservingTransaction::ClaimInPool or
                    participantsVotesAppendResponse->observingResponse() == ObservingTransaction::ClaimInBlock) {
                info() << "ParticipantsVotes put into blockchain";
                return true;
            } else if (participantsVotesAppendResponse->observingResponse() == ObservingTransaction::NoInfo) {
                info() << "No Info";
                continue;
            } else if (participantsVotesAppendResponse->observingResponse() == ObservingTransaction::RejectedByObserving) {
                info() << "RejectedByObserving";
                // if ParticipantsVotes sending period has expired,
                // then node should check if transaction is not rejected
                // and if yes reject transaction on it side
                mCancelTransactionSignal(
                    transactionUUID,
                    maximalClaimingBlockNumber);
                return true;
            } else {
                warning() << "Wrong observer response " << participantsVotesAppendResponse->observingResponse();
                continue;
            }
        } catch (std::exception &e) {
            this->warning() << "Can't parse observer response " << e.what();
            continue;
        }
    }

    this->warning() << "Can't send ParticipantsVotesMessage to all observers";
    return false;
}

void ObservingHandler::responseActualBlockNumber(
    const TransactionUUID &transactionUUID)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "responseActualBlockNumber " << transactionUUID;
#endif
    mRequestsTimer.cancel();

    mResourcesManager->putResource(
        make_shared<BlockNumberRecourse>(
            transactionUUID,
            kDefaultBlockNumber));
}

void ObservingHandler::addPaymentClaim(
    const TransactionUUID &transactionUUID,
    BlockNumber maxBlockNumberForClaiming,
    const map<PaymentNodeID, sphincs::PublicKey::Shared> &participantsPublicKeys,
    sphincs::PublicKey::Shared publicKey,
    sphincs::Signature::Shared signature)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Adding payment claim: " << transactionUUID
            << " maxBlockNumber: " << maxBlockNumberForClaiming;
#endif

    auto claim = make_shared<ObservingPaymentClaim>(
        transactionUUID,
        maxBlockNumberForClaiming,
        participantsPublicKeys,
        publicKey,
        signature);

    auto key = make_pair(transactionUUID, maxBlockNumberForClaiming);
    mPaymentClaims[key] = claim;

#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Payment claims map size: " << mPaymentClaims.size();
#endif
}

void ObservingHandler::processPaymentClaims()
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Processing payment claims, count: " << mPaymentClaims.size();
#endif

    vector<pair<TransactionUUID, BlockNumber>> claimsToRemove;

    for (auto &[key, claim] : mPaymentClaims) {
        try {
            switch (claim->status()) {
            case ObservingPaymentClaim::NoInfo:
                sendClaim(claim);
                break;
            case ObservingPaymentClaim::Observing:
                checkTransaction(claim);
                break;
            case ObservingPaymentClaim::ParticipantsVotesPresent:
                getParticipantsSignatures(claim);
                break;
            case ObservingPaymentClaim::RejectedByObserving:
                rejectTransaction(claim);
                break;
            case ObservingPaymentClaim::Done:
                claimsToRemove.push_back(key);
                break;
            default:
                break;
            }
        } catch (const std::exception &e) {
            warning() << "Error processing claim " << claim->transactionUUID()
                      << ": " << e.what();
        } catch (...) {
            warning() << "Unknown error processing claim " << claim->transactionUUID();
        }
    }

    for (const auto &key : claimsToRemove) {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Removing completed claim: " << key.first
                << " block: " << key.second;
#endif
        mPaymentClaims.erase(key);
    }

#ifdef TESTS
    mPaymentClaimsTimer.expires_after(
        chrono::seconds(kPaymentClaimProcessingPeriodSecondsTests));
#else
    mPaymentClaimsTimer.expires_after(
        chrono::seconds(kPaymentClaimProcessingPeriodSeconds));
#endif

    mPaymentClaimsTimer.async_wait(
        [this](const boost::system::error_code &e) {

            if (e == boost::asio::error::operation_aborted) {
                return;
            }

            processPaymentClaims();
        });
}

// Reschedules the periodic monitoring of successful transactions.
void ObservingHandler::rescheduleSuccessfulTransactionsMonitor()
{
#ifdef TESTS
    mSuccessfulTransactionsMonitorTimer.expires_after(
        chrono::seconds(kSuccessfulTransactionsMonitoringPeriodSecondsTests));
#else
    mSuccessfulTransactionsMonitorTimer.expires_after(
        chrono::seconds(kSuccessfulTransactionsMonitoringPeriodSeconds));
#endif

    mSuccessfulTransactionsMonitorTimer.async_wait(
        [this](const boost::system::error_code &e) {
            if (e == boost::asio::error::operation_aborted) {
                return;
            }
            monitorSuccessfulTransactions();
        });
}

// Main monitoring cycle for finalized transactions that may assist observers.
void ObservingHandler::monitorSuccessfulTransactions()
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Starting successful transactions monitoring cycle";
#endif

    try {
        BlockNumber currentBlockNumber;
        try {
            // Fetch current observer block number to bound claiming window checks.
            currentBlockNumber = getActualBlockNumber();
        } catch (const std::exception &e) {
            mLog.error(logHeader()) << "Failed to get actual block number: " << e.what();
            rescheduleSuccessfulTransactionsMonitor();
            return;
        }

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Current observer block number: " << currentBlockNumber;
#endif

        // Retrieve transactions that are still within claiming period.
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto transactions = ioTransaction->paymentTransactionsHandler()
                                ->transactionsForObserverMonitoring(
                                    currentBlockNumber,
                                    kMaxTransactionsPerMonitoringCycle);

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Retrieved " << transactions.size()
                << " transactions for monitoring";
#endif

        if (transactions.empty()) {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
            debug() << "No transactions to monitor, rescheduling";
#endif
            rescheduleSuccessfulTransactionsMonitor();
            return;
        }

        // Step 3: Check for relevant observer claims and submit signatures when needed.
        checkForRelevantClaims(transactions);

        info() << "Monitoring cycle completed: processed " << transactions.size()
               << " transactions";

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Error in successful transactions monitoring: " << e.what();
    } catch (...) {
        mLog.error(logHeader()) << "Unknown error in successful transactions monitoring";
    }

    rescheduleSuccessfulTransactionsMonitor();
}

// Checks observer for claims related to finalized transactions and submits signatures.
void ObservingHandler::checkForRelevantClaims(
    const vector<pair<TransactionUUID, BlockNumber>>& transactions)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Checking observer for relevant claims across "
            << transactions.size() << " transactions";
#endif

    if (transactions.empty()) {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "No transactions provided for claim check";
#endif
        return;
    }

    if (mObservers.empty()) {
        warning() << "Cannot check for relevant claims: no observers configured";
        return;
    }

    auto firstObserver = mObservers[0];

    try {
        auto statuses = mObserverRPCClient->getClaimStatuses(
            firstObserver,
            transactions);
        size_t relevantClaims = 0;

        for (const auto &status : statuses) {
            if (status.state == "observing") {
                ++relevantClaims;
                submitFinalizedSignatures(
                    status.transactionUUID,
                    status.maxClaimBlockNumber);
            } else {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
                debug() << "Transaction " << status.transactionUUID
                        << " returned non-observing state: " << status.state;
#endif
            }
        }

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Relevant observing claims detected: " << relevantClaims;
#endif
        info() << "Found " << relevantClaims << " relevant claims to assist";

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Failed to check for relevant claims: " << e.what();
    } catch (...) {
        mLog.error(logHeader()) << "Unknown error while checking for relevant claims";
    }
}

// Submits finalized participant signatures to observer for claim resolution.
void ObservingHandler::submitFinalizedSignatures(
    const TransactionUUID& transactionUUID,
    BlockNumber maxBlockNumber)
{
    info() << "Submitting finalized signatures for transaction: " << transactionUUID;

    try {
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto participantsSignatures = ioTransaction->paymentParticipantsVotesHandler()
                                          ->participantsSignatures(transactionUUID);

        if (participantsSignatures.empty()) {
            warning() << "No participant signatures found for transaction: " << transactionUUID;
            return;
        }

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Retrieved " << participantsSignatures.size()
                << " participant signatures for " << transactionUUID;
#endif

        if (mObservers.empty()) {
            warning() << "Cannot submit finalized signatures: no observers configured";
            return;
        }

        auto firstObserver = mObservers[0];

        string responseMessage;
        bool success = mObserverRPCClient->submitClaimVotes(
            firstObserver,
            transactionUUID,
            maxBlockNumber,
            participantsSignatures,
            responseMessage);

        if (success) {
            info() << "Successfully submitted finalized signatures for transaction: "
                   << transactionUUID;
        } else {
            warning() << "Observer rejected signature submission for transaction "
                      << transactionUUID << ": " << responseMessage;
        }

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Failed to submit finalized signatures for transaction "
                                << transactionUUID << ": " << e.what();
    } catch (...) {
        mLog.error(logHeader()) << "Failed to submit finalized signatures for transaction "
                                << transactionUUID << ": unknown error";
    }
}

void ObservingHandler::sendClaim(
    ObservingPaymentClaim::Shared claim)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Sending claim to observer: " << claim->transactionUUID();
#endif

    if (mObservers.empty()) {
        warning() << "Cannot send claim: no observers configured";
        throw NotFoundError("No observers configured");
    }

    auto firstObserver = mObservers[0];

    try {
        if (mObserverRPCClient->acceptClaim(
                    firstObserver,
                    *claim)) {
            claim->setStatus(ObservingPaymentClaim::Observing);
            info() << "Claim accepted by observer: " << claim->transactionUUID();
            return;
        }

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Failed to send claim to observer " << firstObserver->fullAddress()
                                << ": " << e.what();
        throw;
    } catch (...) {
        mLog.error(logHeader()) << "Failed to send claim to observer " << firstObserver->fullAddress()
                                << ": unknown error";
        throw;
    }
}

void ObservingHandler::checkTransaction(
    ObservingPaymentClaim::Shared claim)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "Checking transaction status: " << claim->transactionUUID();
#endif

    if (mObservers.empty()) {
        warning() << "Cannot check transaction: no observers configured";
        throw NotFoundError("No observers configured");
    }

    auto firstObserver = mObservers[0];

    try {
        auto status = mObserverRPCClient->getClaimStatus(
            firstObserver,
            claim->transactionUUID(),
            claim->maxBlockNumberForClaiming());

        if (status == ObservingPaymentClaim::NoInfo) {
            claim->setStatus(ObservingPaymentClaim::NoInfo);
            info() << "Claim not found on observer, will retry: "
                   << claim->transactionUUID();
        } else if (status == ObservingPaymentClaim::Observing) {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
            debug() << "Claim still observing: " << claim->transactionUUID();
#endif
        } else if (status == ObservingPaymentClaim::ParticipantsVotesPresent) {
            claim->setStatus(ObservingPaymentClaim::ParticipantsVotesPresent);
            info() << "Claim approved by observer: " << claim->transactionUUID();
        } else if (status == ObservingPaymentClaim::RejectedByObserving) {
            claim->setStatus(ObservingPaymentClaim::RejectedByObserving);
            info() << "Claim rejected by observer: " << claim->transactionUUID();
        }

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Failed to check transaction status from observer "
                                << firstObserver->fullAddress() << ": " << e.what();
        throw;
    } catch (...) {
        mLog.error(logHeader()) << "Failed to check transaction status from observer "
                                << firstObserver->fullAddress() << ": unknown error";
        throw;
    }
}

void ObservingHandler::getParticipantsSignatures(
    ObservingPaymentClaim::Shared claim)
{
    info() << "Getting participants signatures: " << claim->transactionUUID();

    if (mObservers.empty()) {
        warning() << "Cannot get participants signatures: no observers configured";
        throw NotFoundError("No observers configured");
    }

    auto firstObserver = mObservers[0];

    try {
        auto signaturesMap = mObserverRPCClient->getClaimVotes(
            firstObserver,
            claim->transactionUUID(),
            claim->maxBlockNumberForClaiming());

        info() << "Retrieved " << signaturesMap.size()
               << " participant signatures from observer";

        mParticipantsVotesSignal(
            claim->transactionUUID(),
            claim->maxBlockNumberForClaiming(),
            signaturesMap);

        claim->setStatus(ObservingPaymentClaim::Done);

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Failed to get participants signatures from observer "
                                << firstObserver->fullAddress() << ": " << e.what();
        throw;
    } catch (...) {
        mLog.error(logHeader()) << "Failed to get participants signatures from observer "
                                << firstObserver->fullAddress() << ": unknown error";
        throw;
    }
}

void ObservingHandler::rejectTransaction(
    ObservingPaymentClaim::Shared claim)
{
    info() << "Getting rejection signature: " << claim->transactionUUID();

    if (mObservers.empty()) {
        warning() << "Cannot get rejection signature: no observers configured";
        throw NotFoundError("No observers configured");
    }

    auto firstObserver = mObservers[0];

    try {
        auto signature = mObserverRPCClient->getRejectionSignature(
            firstObserver,
            claim->transactionUUID(),
            claim->maxBlockNumberForClaiming());

        if (signature.has_value()) {
            info() << "Received rejection signature from observer for "
                   << claim->transactionUUID();

            mRejectTransactionSignal(
                claim->transactionUUID(),
                claim->maxBlockNumberForClaiming());

            claim->setStatus(ObservingPaymentClaim::Done);
        } else {
#ifdef DEBUG_LOG_OBSEVING_HANDLER
            debug() << "No rejection signature available yet for "
                    << claim->transactionUUID();
#endif
        }

    } catch (const std::exception &e) {
        mLog.error(logHeader()) << "Failed to get rejection signature from observer "
                                << firstObserver->fullAddress() << ": " << e.what();
        throw;
    } catch (...) {
        mLog.error(logHeader()) << "Failed to get rejection signature from observer "
                                << firstObserver->fullAddress() << ": unknown error";
        throw;
    }
}

BlockNumber ObservingHandler::getActualBlockNumber() const
{
    if (mObservers.empty()) {
        warning() << "Cannot get actual block number: no observers configured";
        throw NotFoundError("Cannot get actual block number: no observers configured");
    }

    auto firstObserver = mObservers[0];

    try {
        BlockNumber blockNumber = mObserverRPCClient->getBlockNumber(firstObserver);
        info() << "Actual block number from observer " << firstObserver->fullAddress()
               << ": " << blockNumber;

        return blockNumber;

    } catch (const std::exception &e) {
        warning() << "Failed to get block number from observer "
                  << firstObserver->fullAddress() << ": " << e.what();
        throw NotFoundError("Failed to get block number from observer " + firstObserver->fullAddress() + ": " + e.what());
    } catch (...) {
        warning() << "Unknown error while getting block number from observer "
                  << firstObserver->fullAddress();
        throw NotFoundError("Unknown error while getting block number from observer " + firstObserver->fullAddress());
    } 
}

const string ObservingHandler::logHeader() const
{
    return "ObservingHandler";
}
