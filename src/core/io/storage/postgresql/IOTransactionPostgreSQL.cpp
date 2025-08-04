#include "IOTransactionPostgreSQL.h"
#include <sstream>

using namespace std;

namespace {
inline void checkCmd(PGconn *db, PGresult *res, const string &prefix) {
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(db);
        PQclear(res);
        throw IOError(prefix + ": " + err);
    }
}
}

IOTransactionPostgreSQL::IOTransactionPostgreSQL(
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
    Logger &logger) :
    mDBConnection(dbConnection),
    mTrustLinesHandler(trustLinesHandler),
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

IOTransactionPostgreSQL::~IOTransactionPostgreSQL()
{
    commit();
}

// Getters (each checks transaction state)
#define ENSURE_BEGIN if(!mIsTransactionBegin) throw IOError("IOTransactionPostgreSQL: transaction finished");

TrustLineHandler *IOTransactionPostgreSQL::trustLinesHandler() { ENSURE_BEGIN return mTrustLinesHandler; }
HistoryStorage *IOTransactionPostgreSQL::historyStorage() { ENSURE_BEGIN return mHistoryStorage; }
TransactionsHandler *IOTransactionPostgreSQL::transactionHandler() { ENSURE_BEGIN return mTransactionHandler; }
OwnKeysHandler *IOTransactionPostgreSQL::ownKeysHandler() { ENSURE_BEGIN return mOwnKeysHandler; }
ContractorKeysHandler *IOTransactionPostgreSQL::contractorKeysHandler() { ENSURE_BEGIN return mContractorKeysHandler; }
AuditHandler *IOTransactionPostgreSQL::auditHandler() { ENSURE_BEGIN return mAuditHandler; }
IncomingPaymentReceiptHandler *IOTransactionPostgreSQL::incomingPaymentReceiptHandler() { ENSURE_BEGIN return mIncomingPaymentReceiptHandler; }
OutgoingPaymentReceiptHandler *IOTransactionPostgreSQL::outgoingPaymentReceiptHandler() { ENSURE_BEGIN return mOutgoingPaymentReceiptHandler; }
PaymentKeysHandler *IOTransactionPostgreSQL::paymentKeysHandler() { ENSURE_BEGIN return mPaymentKeysHandler; }
PaymentParticipantsVotesHandler *IOTransactionPostgreSQL::paymentParticipantsVotesHandler() { ENSURE_BEGIN return mPaymentParticipantsVotesHandler; }
PaymentTransactionsHandler *IOTransactionPostgreSQL::paymentTransactionsHandler() { ENSURE_BEGIN return mPaymentTransactionsHandler; }
ContractorsHandler *IOTransactionPostgreSQL::contractorsHandler() { ENSURE_BEGIN return mContractorsHandler; }
AddressHandler *IOTransactionPostgreSQL::addressHandler() { ENSURE_BEGIN return mAddressHandler; }
FeaturesHandler *IOTransactionPostgreSQL::featuresHandler() { ENSURE_BEGIN return mFeaturesHandler; }

void IOTransactionPostgreSQL::commit()
{
    if (!mIsTransactionBegin) {
        warning() << "transaction don't commit, because it wasn't started";
        return;
    }
    PGresult *res = PQexec(mDBConnection, "COMMIT;");
    checkCmd(mDBConnection, res, "IOTransactionPostgreSQL::commit");
    PQclear(res);
    mIsTransactionBegin = false;
}

void IOTransactionPostgreSQL::rollback()
{
    PGresult *res = PQexec(mDBConnection, "ROLLBACK;");
    checkCmd(mDBConnection, res, "IOTransactionPostgreSQL::rollback");
    PQclear(res);
    mIsTransactionBegin = false;
}

void IOTransactionPostgreSQL::beginTransactionQuery()
{
    PGresult *res = PQexec(mDBConnection, "BEGIN;");
    checkCmd(mDBConnection, res, "IOTransactionPostgreSQL::begin");
    PQclear(res);
}

LoggerStream IOTransactionPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream IOTransactionPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string IOTransactionPostgreSQL::logHeader() const { stringstream s; s << "IOTransactionPostgreSQL "; return s.str(); } 