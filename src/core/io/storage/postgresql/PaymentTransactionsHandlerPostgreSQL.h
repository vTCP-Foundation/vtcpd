#ifndef VTCPD_PAYMENTTRANSACTIONSHANDLERPOSTGRESQL_H
#define VTCPD_PAYMENTTRANSACTIONSHANDLERPOSTGRESQL_H

#include "../interfaces/PaymentTransactionsHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/Types.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <string>
#include <vector>

class PaymentTransactionsHandlerPostgreSQL : public PaymentTransactionsHandler
{
public:
    PaymentTransactionsHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRecord(
        const TransactionUUID &transactionUUID,
        BlockNumber maximalClaimingBlockNumber) override;

    void updateTransactionState(
        const TransactionUUID &transactionUUID,
        int observingTransactionState) override;

    std::vector<std::pair<TransactionUUID, BlockNumber>> transactionsWithUncertainObservingState() override;

    bool isTransactionPresent(
        const TransactionUUID &transactionUUID) override;

    void deleteRecord(
        const TransactionUUID &transactionUUID) override;

    std::vector<TransactionUUID> allTransactionsUUID() override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_PAYMENTTRANSACTIONSHANDLERPOSTGRESQL_H 