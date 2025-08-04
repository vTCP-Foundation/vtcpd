#ifndef VTCPD_TRANSACTIONSHANDLERPOSTGRESQL_H
#define VTCPD_TRANSACTIONSHANDLERPOSTGRESQL_H

#include "../interfaces/TransactionsHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <string>
#include <vector>

class TransactionsHandlerPostgreSQL : public TransactionsHandler
{
public:
    TransactionsHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRecord(
        const TransactionUUID &transactionUUID,
        BytesShared transaction,
        size_t transactionBytesCount) override;

    BytesShared getTransaction(
        const TransactionUUID &transactionUUID) override;

    bool isTransactionSerialized(
        const TransactionUUID &transactionUUID) override;

    void deleteRecordIfExists(
        const TransactionUUID &transactionUUID) override;

    std::vector<BytesShared> allTransactions() override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_TRANSACTIONSHANDLERPOSTGRESQL_H 