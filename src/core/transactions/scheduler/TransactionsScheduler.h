#ifndef VTCPD_TRANSACTIONSSCHEDULER_H
#define VTCPD_TRANSACTIONSSCHEDULER_H

#include "../../common/time/TimeUtils.h"

#include "../../network/messages/Message.hpp"
#include "../../network/messages/base/transaction/TransactionMessage.h"

#include "../transactions/base/BaseTransaction.h"
#include "../transactions/regular/payments/base/BasePaymentTransaction.h"
#include "../transactions/regular/payments/CoordinatorPaymentTransaction.h"
#include "../transactions/trust_lines/AuditSourceTransaction.h"
#include "../transactions/result/TransactionResult.h"

#include "../../resources/resources/BaseResource.h"

#include "../../common/exceptions/Exception.h"
#include "../../network/rpc/RpcResponse.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/ConflictError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../common/exceptions/RuntimeError.h"
#include "../../logger/Logger.h"

#include "../../subsystems_controller/TrustLinesInfluenceController.h"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind/bind.hpp>
#include <boost/signals2.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <stdint.h>


using namespace std;
using namespace boost::placeholders;

namespace as = boost::asio;
namespace signals = boost::signals2;


class TransactionsScheduler
{
public:
    typedef signals::signal<void(CommandResult::SharedConst)> CommandResultSignal;
    typedef signals::signal<void(BaseTransaction::Shared)> SerializeTransactionSignal;
    typedef signals::signal<void(const SerializedEquivalent equivalent)> CycleCloserTransactionWasFinishedSignal;
    // Alias for transaction type checks in scheduler queries.
    typedef BaseTransaction::TransactionType TransactionType;


public:
    TransactionsScheduler(
        as::io_context &IOCtx,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger);

    void run();

    void scheduleTransaction(
        BaseTransaction::Shared transaction);

    void postponeTransaction(
        BaseTransaction::Shared transaction,
        uint32_t millisecondsDelay);

    void awakeTransaction(
        BaseTransaction::Shared transaction);

    // TODO: add error log if unsuccessful
    // TODO: throw exception on failure,
    //       so the communicator would be able to know
    //       if contractor node doesn't follows the protocol.
    /*
     * This method used only for Transaction Messages
     */
    void tryAttachMessageToTransaction(
        Message::Shared message);

    /**
     * Attaches incoming RPC response to the matching transaction and awakens it if required.
     */
    bool tryAttachRpcResponseToTransaction(
        RpcResponse::Shared response);

    void tryAttachResourceToTransaction(
        BaseResource::Shared resource);

    friend const map<BaseTransaction::Shared, TransactionState::SharedConst>* transactions(
        TransactionsScheduler *scheduler);

    const BaseTransaction::Shared cycleClosingTransactionByUUID(
        const TransactionUUID &transactionUUID) const;

    bool isTransactionInProcess(
        const TransactionUUID &transactionUUID) const;

    const BaseTransaction::Shared paymentTransactionByCommandUUID(
        const CommandUUID &commandUUID) const;

    bool checkAuditTransactionPresence(
        SerializedEquivalent equivalent,
        ContractorID contractorId) const;

    /**
     * Task 15-03: Checks if any transaction of the specified type is active or scheduled.
     *
     * @param type The transaction type to search for.
     * @return true if a transaction of this type exists, false otherwise.
     */
    bool hasActiveTransactionOfType(
        TransactionType type) const;

protected:
    static string logHeader()
    noexcept;

    LoggerStream warning() const
    noexcept;

    LoggerStream error() const
    noexcept;

    LoggerStream info() const
    noexcept;

    LoggerStream debug() const
    noexcept;

private:
    void launchTransaction(
        BaseTransaction::Shared transaction);

    void handleTransactionResult(
        BaseTransaction::Shared transaction,
        TransactionResult::SharedConst result);

    void processCommandResult(
        BaseTransaction::Shared transaction,
        CommandResult::SharedConst result);

    void processTransactionState(
        BaseTransaction::Shared transaction,
        TransactionState::SharedConst state);

    void processCommandResultAndTransactionState(
        BaseTransaction::Shared transaction,
        CommandResult::SharedConst result,
        TransactionState::SharedConst state);

    void forgetTransaction(
        BaseTransaction::Shared transaction);

    void adjustAwakeningToNextTransaction();

    pair<BaseTransaction::Shared, GEOEpochTimestamp> transactionWithMinimalAwakeningTimestamp() const;

    BaseTransaction::Shared getEarlierTransaction(
        BaseTransaction::Shared transaction);


    void asyncWaitUntil(
        GEOEpochTimestamp nextAwakeningTimestamp);

    void handleAwakening(
        const boost::system::error_code &error);

public:
    mutable CommandResultSignal commandResultIsReadySignal;
    mutable SerializeTransactionSignal serializeTransactionSignal;
    // this signal used for notification of CyclesManager that CycleCloser transaction was finished
    // and it can try launch new CycleCloser transaction
    mutable CycleCloserTransactionWasFinishedSignal cycleCloserTransactionWasFinishedSignal;

private:
    as::io_context &mIOCtx;
    Logger &mLog;

    unique_ptr<as::steady_timer> mProcessingTimer;
    unique_ptr<map<BaseTransaction::Shared, TransactionState::SharedConst>> mTransactions;

    TrustLinesInfluenceController *mTrustLinesInfluenceController;
};

#endif //VTCPD_TRANSACTIONSSCHEDULER_H
