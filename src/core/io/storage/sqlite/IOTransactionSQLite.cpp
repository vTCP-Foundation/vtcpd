#include "IOTransactionSQLite.h"

IOTransactionSQLite::IOTransactionSQLite(
    sqlite3 *dbConnection,
    TrustLineHandler *trustLineHandler,
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
    Logger &logger) :

    mDBConnection(dbConnection),
    mTrustLinesHandler(trustLineHandler),
    mHistoryStorage(historyStorage),
    mTransactionHandler(transactionHandler),
    mOwnKeysHandler(ownKeysHandler),
    mContractorKeysHandler(contractorKeysHandler),
    mAuditHandler(auditHandler),
    mIncomingPaymentReceiptHandler(incomingPaymentReceiptHandler),
    mOutgoingPaymentReceiptHandler(outgoingPaymentReceiptHandler),
    mPaymentKeysHandler(paymentKeysHandler),
    mPaymentParticipantsVotesHandler(paymentParticipantsVotesHandler),
    mPaymentTransactionsHandler(paymentTransactionsHandler),
    mContractorsHandler(contractorsHandler),
    mAddressHandler(addressHandler),
    mFeaturesHandler(featuresHandler),
    mIsTransactionBegin(true),
    mLog(logger)
{
    beginTransactionQuery();
}

IOTransactionSQLite::~IOTransactionSQLite()
{
    commit();
}

TrustLineHandler* IOTransactionSQLite::trustLinesHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::trustLineHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mTrustLinesHandler;
}

HistoryStorage* IOTransactionSQLite::historyStorage()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::historyStorage: "
                      "transaction was rollback, it can't be use now");
    }
    return mHistoryStorage;
}

TransactionsHandler* IOTransactionSQLite::transactionHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::transactionHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mTransactionHandler;
}

OwnKeysHandler* IOTransactionSQLite::ownKeysHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::ownKeysHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mOwnKeysHandler;
}

ContractorKeysHandler* IOTransactionSQLite::contractorKeysHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::contractorKeysHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mContractorKeysHandler;
}

AuditHandler* IOTransactionSQLite::auditHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::auditHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mAuditHandler;
}

IncomingPaymentReceiptHandler* IOTransactionSQLite::incomingPaymentReceiptHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::incomingPaymentReceiptHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mIncomingPaymentReceiptHandler;
}

OutgoingPaymentReceiptHandler* IOTransactionSQLite::outgoingPaymentReceiptHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::outgoingPaymentReceiptHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mOutgoingPaymentReceiptHandler;
}

PaymentKeysHandler* IOTransactionSQLite::paymentKeysHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::PaymentKeysHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mPaymentKeysHandler;
}

PaymentParticipantsVotesHandler* IOTransactionSQLite::paymentParticipantsVotesHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::paymentParticipantsVotesHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mPaymentParticipantsVotesHandler;
}

PaymentTransactionsHandler* IOTransactionSQLite::paymentTransactionsHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::paymentTransactionsHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mPaymentTransactionsHandler;
}

ContractorsHandler* IOTransactionSQLite::contractorsHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::contractorsHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mContractorsHandler;
}

AddressHandler* IOTransactionSQLite::addressHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::addressHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mAddressHandler;
}

FeaturesHandler* IOTransactionSQLite::featuresHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("IOTransactionSQLite::featuresHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mFeaturesHandler;
}

void IOTransactionSQLite::commit()
{
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "commit";
#endif
    if (!mIsTransactionBegin) {
        warning() << "transaction don't commit, because it wasn't started";
        return;
    }
    string query = "COMMIT TRANSACTION;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2( mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("IOTransactionSQLite::commit: Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw IOError("IOTransactionSQLite::commit: Run query; sqlite error: " + to_string(rc));
    }
    mIsTransactionBegin = false;
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "transaction commit";
#endif
}

void IOTransactionSQLite::rollback()
{
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "rollback";
#endif
    string query = "ROLLBACK TRANSACTION;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("IOTransactionSQLite::rollback: Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw IOError("IOTransactionSQLite::rollback: Run query; sqlite error: " + to_string(rc));
    }
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "rollBack done";
#endif
    mIsTransactionBegin = false;
}

LoggerStream IOTransactionSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream IOTransactionSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string IOTransactionSQLite::logHeader() const
{
    stringstream s;
    s << "IOTransaction ";
    return s.str();
}

void IOTransactionSQLite::beginTransactionQuery()
{
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "beginTransactionQuery";
#endif
    string query = "BEGIN TRANSACTION;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("IOTransactionSQLite::prepareInserted: Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw IOError("IOTransactionSQLite::prepareInserted: Run query; sqlite error: " + to_string(rc));
    }
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "transaction begin";
#endif
}
