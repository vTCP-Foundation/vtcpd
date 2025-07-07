#ifndef VTCPD_IOTRANSACTIONPOSTGRESQL_H
#define VTCPD_IOTRANSACTIONPOSTGRESQL_H

#include "../interfaces/IOTransaction.h"
#include "TrustLineHandlerPostgreSQL.h"
#include "HistoryStoragePostgreSQL.h"
#include "TransactionsHandlerPostgreSQL.h"
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
#include "../../../common/exceptions/IOError.h"
#include "../../../logger/Logger.h"

class IOTransactionPostgreSQL : public IOTransaction
{
public:
    IOTransactionPostgreSQL(
        PGconn *dbConnection,
        TrustLineHandler *trustLinesHandler,
        HistoryStorage *historyStorage,
        TransactionsHandler *transactionHandler,
        OwnKeysHandler *ownKeysHandler,
        ContractorKeysHandler *contractorKeysHandler,
        AuditHandler *auditHandler,
        IncomingPaymentReceiptHandler *incomingPaymentReceiptHandler,
        OutgoingPaymentReceiptHandler *outgoingPaymentReceiptHandler,
        PaymentKeysHandler *paymentKeysHandler,
        PaymentParticipantsVotesHandler *paymentParticipantsVotesHandler,
        PaymentTransactionsHandler *paymentTransactionsHandler,
        ContractorsHandler *contractorsHandler,
        AddressHandler *addressHandler,
        FeaturesHandler *featuresHandler,
        Logger &logger);

    ~IOTransactionPostgreSQL();

    // getters
    TrustLineHandler *trustLinesHandler() override;
    HistoryStorage *historyStorage() override;
    TransactionsHandler *transactionHandler() override;
    OwnKeysHandler *ownKeysHandler() override;
    ContractorKeysHandler *contractorKeysHandler() override;
    AuditHandler *auditHandler() override;
    IncomingPaymentReceiptHandler *incomingPaymentReceiptHandler() override;
    OutgoingPaymentReceiptHandler *outgoingPaymentReceiptHandler() override;
    PaymentKeysHandler *paymentKeysHandler() override;
    PaymentParticipantsVotesHandler *paymentParticipantsVotesHandler() override;
    PaymentTransactionsHandler *paymentTransactionsHandler() override;
    ContractorsHandler *contractorsHandler() override;
    AddressHandler *addressHandler() override;
    FeaturesHandler *featuresHandler() override;

    void rollback() override;

    void commit();
    void beginTransactionQuery();

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDBConnection = nullptr;
    TrustLineHandler *mTrustLinesHandler = nullptr;
    HistoryStorage *mHistoryStorage = nullptr;
    TransactionsHandler *mTransactionHandler = nullptr;
    OwnKeysHandler *mOwnKeysHandler = nullptr;
    ContractorKeysHandler *mContractorKeysHandler = nullptr;
    AuditHandler *mAuditHandler = nullptr;
    IncomingPaymentReceiptHandler *mIncomingPaymentReceiptHandler = nullptr;
    OutgoingPaymentReceiptHandler *mOutgoingPaymentReceiptHandler = nullptr;
    PaymentKeysHandler *mPaymentKeysHandler = nullptr;
    PaymentParticipantsVotesHandler *mPaymentParticipantsVotesHandler = nullptr;
    PaymentTransactionsHandler *mPaymentTransactionsHandler = nullptr;
    ContractorsHandler *mContractorsHandler = nullptr;
    AddressHandler *mAddressHandler = nullptr;
    FeaturesHandler *mFeaturesHandler = nullptr;

    Logger &mLog;
    bool mIsTransactionBegin = false;
};

#endif // VTCPD_IOTRANSACTIONPOSTGRESQL_H 