#ifndef VTCPD_STORAGEHANDLERPOSTGRESQL_H
#define VTCPD_STORAGEHANDLERPOSTGRESQL_H

#include "../interfaces/StorageHandler.h"
#include "IOTransactionPostgreSQL.h"
#include "TrustLineHandlerPostgreSQL.h"
#include "TransactionsHandlerPostgreSQL.h"
#include "HistoryStoragePostgreSQL.h"
#include "OwnKeysHandlerPostgreSQL.h"
#include "ContractorKeysHandlerPostgreSQL.h"
#include "AuditHandlerPostgreSQL.h"
#include "IncomingPaymentReceiptHandlerPostgreSQL.h"
#include "OutgoingPaymentReceiptHandlerPostgreSQL.h"
#include "PaymentKeysHandlerPostgreSQL.h"
#include "PaymentParticipantsVotesHandlerPostgreSQL.h"
#include "PaymentTransactionsHandlerPostgreSQL.h"
#include "ContractorsHandlerPostgreSQL.h"
#include "AddressHandlerPostgreSQL.h"
#include "FeaturesHandlerPostgreSQL.h"
#include <libpq-fe.h>
#include <string>
#include <boost/filesystem.hpp>

class StorageHandlerPostgreSQL : public StorageHandler
{
public:
    StorageHandlerPostgreSQL(
        const std::string &connectionOptions,
        Logger &logger);
    ~StorageHandlerPostgreSQL();

    IOTransaction::Shared beginTransaction() override;
    void vacuum() override;

private:
    static PGconn* connection(
        const std::string &connectionOptions,
        Logger &logger);

    LoggerStream info() const;
    LoggerStream warning() const;
    LoggerStream error() const;
    const std::string logHeader() const;

    static PGconn *mDBConnection;
    Logger &mLog;

    // table names
    const std::string kContractorsTableName = "contractors";
    const std::string kContractorAddressesTableName = "contractors_addresses";
    const std::string kTrustLineTableName = "trust_lines";
    const std::string kTransactionTableName = "transactions";
    const std::string kHistoryMainTableName = "history";
    const std::string kHistoryAdditionalTableName = "history_additional";
    const std::string kOwnKeysTableName = "own_keys";
    const std::string kContractorKeysTableName = "contractor_keys";
    const std::string kOutgoingReceiptTableName = "outgoing_receipt";
    const std::string kIncomingReceiptTableName = "incoming_receipt";
    const std::string kAuditTableName = "audit";
    const std::string kPaymentKeysTableName = "payment_keys";
    const std::string kPaymentParticipantsVotesTableName = "payment_participants_votes";
    const std::string kPaymentTransactionsTableName = "payment_transactions";
    const std::string kFeaturesTableName = "features";

    // handlers
    ContractorsHandlerPostgreSQL mContractorsHandler;
    AddressHandlerPostgreSQL mAddressHandler;
    TrustLineHandlerPostgreSQL mTrustLineHandler;
    TransactionsHandlerPostgreSQL mTransactionHandler;
    HistoryStoragePostgreSQL mHistoryStorage;
    OwnKeysHandlerPostgreSQL mOwnKeysHandler;
    ContractorKeysHandlerPostgreSQL mContractorKeysHandler;
    AuditHandlerPostgreSQL mAuditHandler;
    IncomingPaymentReceiptHandlerPostgreSQL mIncomingPaymentReceiptHandler;
    OutgoingPaymentReceiptHandlerPostgreSQL mOutgoingPaymentReceiptHandler;
    PaymentKeysHandlerPostgreSQL mPaymentKeysHandler;
    PaymentParticipantsVotesHandlerPostgreSQL mPaymentParticipantsVotesHandler;
    PaymentTransactionsHandlerPostgreSQL mPaymentTransactionsHandler;
    FeaturesHandlerPostgreSQL mFeaturesHandler;

    std::string mConnectionOptions;
};

#endif // VTCPD_STORAGEHANDLERPOSTGRESQL_H 