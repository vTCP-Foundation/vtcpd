#ifndef VTCPD_PAYMENTPARTICIPANTSVOTESHANDLERSQLITE_H
#define VTCPD_PAYMENTPARTICIPANTSVOTESHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/PaymentParticipantsVotesHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>

using namespace crypto::sphincs;

class PaymentParticipantsVotesHandlerSQLite : public PaymentParticipantsVotesHandler
{
public:
    PaymentParticipantsVotesHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    void saveRecord(
        const TransactionUUID &transactionUUID,
        Contractor::Shared contractor,
        const PaymentNodeID paymentNodeID,
        const PublicKey::Shared publicKey,
        const Signature::Shared signature) override;

    map<PaymentNodeID, Signature::Shared> participantsSignatures(
        const TransactionUUID &transactionUUID) override;

    void deleteRecords(
        const TransactionUUID &transactionUUID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_PAYMENTPARTICIPANTSVOTESHANDLERSQLITE_H
