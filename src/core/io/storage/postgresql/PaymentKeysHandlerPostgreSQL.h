#ifndef VTCPD_PAYMENTKEYSHANDLERPOSTGRESQL_H
#define VTCPD_PAYMENTKEYSHANDLERPOSTGRESQL_H

#include "../interfaces/PaymentKeysHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../crypto/sphincskeys.h"
#include <libpq-fe.h>
#include <string>
#include <vector>

using namespace crypto::sphincs;

class PaymentKeysHandlerPostgreSQL : public PaymentKeysHandler
{
public:
    PaymentKeysHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveOwnKey(
        const TransactionUUID &transactionUUID,
        const PublicKey::Shared publicKey,
        const PrivateKey *privateKey) override;

    PrivateKey* getOwnPrivateKey(
        const TransactionUUID &transactionUUID) override;

    void deleteKeyByTransactionUUID(
        const TransactionUUID &transactionUUID) override;

    std::vector<TransactionUUID> allTransactionUUIDs() override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_PAYMENTKEYSHANDLERPOSTGRESQL_H 