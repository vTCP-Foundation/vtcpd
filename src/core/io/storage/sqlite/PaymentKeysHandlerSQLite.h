#ifndef VTCPD_PAYMENTKEYSHANDLERSQLITE_H
#define VTCPD_PAYMENTKEYSHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/PaymentKeysHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>

using namespace crypto::sphincs;

class PaymentKeysHandlerSQLite : public PaymentKeysHandler
{
public:
    PaymentKeysHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    void saveOwnKey(
        const TransactionUUID &transactionUUID,
        const PublicKey::Shared publicKey,
        const PrivateKey *privateKey);

    PrivateKey* getOwnPrivateKey(
        const TransactionUUID &transactionUUID);

    void deleteKeyByTransactionUUID(
        const TransactionUUID &transactionUUID);

    vector<TransactionUUID> allTransactionUUIDs();

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_PAYMENTKEYSHANDLERSQLITE_H
