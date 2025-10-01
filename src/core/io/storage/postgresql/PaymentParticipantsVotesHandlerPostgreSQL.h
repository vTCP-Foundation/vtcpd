#ifndef VTCPD_PAYMENTPARTICIPANTSVOTESHANDLERPOSTGRESQL_H
#define VTCPD_PAYMENTPARTICIPANTSVOTESHANDLERPOSTGRESQL_H

#include "../interfaces/PaymentParticipantsVotesHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../contractors/Contractor.h"
#include <libpq-fe.h>
#include <string>
#include <map>

using namespace crypto::sphincs;

class PaymentParticipantsVotesHandlerPostgreSQL : public PaymentParticipantsVotesHandler
{
public:
    PaymentParticipantsVotesHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRecord(
        const TransactionUUID &transactionUUID,
        Contractor::Shared contractor,
        const PaymentNodeID paymentNodeID,
        const PublicKey::Shared publicKey,
        const Signature::Shared signature) override;

    std::map<PaymentNodeID, Signature::Shared> participantsSignatures(
        const TransactionUUID &transactionUUID) override;

    void deleteRecords(
        const TransactionUUID &transactionUUID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_PAYMENTPARTICIPANTSVOTESHANDLERPOSTGRESQL_H 