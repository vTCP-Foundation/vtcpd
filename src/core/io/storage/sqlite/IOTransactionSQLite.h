#ifndef VTCPD_IOTRANSACTIONSQLITE_H
#define VTCPD_IOTRANSACTIONSQLITE_H

#include "../interfaces/IOTransaction.h"
#include "HistoryStorageSQLite.h"
#include "TransactionsHandlerSQLite.h"
#include "OwnKeysHandlerSQLite.h"
#include "ContractorKeysHandlerSQLite.h"
#include "AuditHandlerSQLite.h"
#include "IncomingPaymentReceiptHandlerSQLite.h"
#include "OutgoingPaymentReceiptHandlerSQLite.h"
#include "PaymentKeysHandlerSQLite.h"
#include "PaymentParticipantsVotesHandlerSQLite.h"
#include "PaymentTransactionsHandlerSQLite.h"
#include "ContractorsHandlerSQLite.h"
#include "AddressHandlerSQLite.h"
#include "FeaturesHandlerSQLite.h"
#include "../../../common/exceptions/IOError.h"
#include <sqlite3.h>
#include <memory>

class IOTransactionSQLite : public IOTransaction
{
public:
    IOTransactionSQLite(
        sqlite3 *dbConnection,
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

    ~IOTransactionSQLite();

    TrustLineHandler *trustLinesHandler();

    HistoryStorage *historyStorage();

    TransactionsHandler *transactionHandler();

    OwnKeysHandler *ownKeysHandler();

    ContractorKeysHandler *contractorKeysHandler();

    AuditHandler *auditHandler();

    IncomingPaymentReceiptHandler *incomingPaymentReceiptHandler();

    OutgoingPaymentReceiptHandler *outgoingPaymentReceiptHandler();

    PaymentKeysHandler *paymentKeysHandler();

    PaymentParticipantsVotesHandler *paymentParticipantsVotesHandler();

    PaymentTransactionsHandler *paymentTransactionsHandler();

    ContractorsHandler *contractorsHandler();

    AddressHandler *addressHandler();

    FeaturesHandler *featuresHandler();

    void rollback();

    void commit();

    void beginTransactionQuery();

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDBConnection = nullptr;
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

#endif //VTCPD_IOTRANSACTIONSQLITE_H
