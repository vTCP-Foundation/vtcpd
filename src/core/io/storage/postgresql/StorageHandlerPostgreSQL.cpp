#include "StorageHandlerPostgreSQL.h"
#include <sstream>

PGconn* StorageHandlerPostgreSQL::mDBConnection = nullptr;

using namespace std;

namespace {
inline void checkConn(PGconn *conn, const string &prefix) {
    if (PQstatus(conn) != CONNECTION_OK) {
        string err = PQerrorMessage(conn);
        throw IOError(prefix + ": " + err);
    }
}
inline void checkCmd(PGconn *conn, PGresult *res, const string &prefix) {
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(conn);
        PQclear(res);
        throw IOError(prefix + ": " + err);
    }
}
}

StorageHandlerPostgreSQL::StorageHandlerPostgreSQL(
    const string &connectionOptions,
    Logger &logger) :
    mLog(logger),
    mTrustLineHandler(connection(connectionOptions, logger), kTrustLineTableName, logger),
    mTransactionHandler(connection(connectionOptions, logger), kTransactionTableName, logger),
    mHistoryStorage(connection(connectionOptions, logger), kHistoryMainTableName, kHistoryAdditionalTableName, logger),
    mOwnKeysHandler(connection(connectionOptions, logger), kOwnKeysTableName, logger),
    mContractorKeysHandler(connection(connectionOptions, logger), kContractorKeysTableName, logger),
    mAuditHandler(connection(connectionOptions, logger), kAuditTableName, logger),
    mIncomingPaymentReceiptHandler(connection(connectionOptions, logger), kIncomingReceiptTableName, logger),
    mOutgoingPaymentReceiptHandler(connection(connectionOptions, logger), kOutgoingReceiptTableName, logger),
    mPaymentTransactionsHandler(connection(connectionOptions, logger), kPaymentTransactionsTableName, logger),
    mPaymentKeysHandler(connection(connectionOptions, logger), kPaymentKeysTableName, logger),
    mPaymentParticipantsVotesHandler(connection(connectionOptions, logger), kPaymentParticipantsVotesTableName, logger),
    mFeaturesHandler(connection(connectionOptions, logger), kFeaturesTableName, logger),
    mContractorsHandler(connection(connectionOptions, logger), kContractorsTableName, logger),
    mAddressHandler(connection(connectionOptions, logger), kContractorAddressesTableName, logger),
    mConnectionOptions(connectionOptions)
{
}

StorageHandlerPostgreSQL::~StorageHandlerPostgreSQL()
{
    if (mDBConnection) {
        PQfinish(mDBConnection);
        mDBConnection = nullptr;
    }
}

PGconn* StorageHandlerPostgreSQL::connection(
    const string &connectionOptions,
    Logger &logger)
{
    if (mDBConnection)
        return mDBConnection;
    mDBConnection = PQconnectdb(connectionOptions.c_str());
    checkConn(mDBConnection, "StorageHandlerPostgreSQL::connection");
    // ensure foreign keys posture (not required in PG) but we can setup search_path later
    return mDBConnection;
}

IOTransaction::Shared StorageHandlerPostgreSQL::beginTransaction()
{
    return make_shared<IOTransactionPostgreSQL>(
        mDBConnection,
        &mTrustLineHandler,
        &mHistoryStorage,
        &mTransactionHandler,
        &mOwnKeysHandler,
        &mContractorKeysHandler,
        &mAuditHandler,
        &mIncomingPaymentReceiptHandler,
        &mOutgoingPaymentReceiptHandler,
        &mPaymentKeysHandler,
        &mPaymentParticipantsVotesHandler,
        &mPaymentTransactionsHandler,
        &mContractorsHandler,
        &mAddressHandler,
        &mFeaturesHandler,
        mLog);
}

void StorageHandlerPostgreSQL::vacuum()
{
    PGresult *res = PQexec(mDBConnection, "VACUUM;");
    checkCmd(mDBConnection, res, "StorageHandlerPostgreSQL::vacuum");
    PQclear(res);
}

LoggerStream StorageHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream StorageHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
LoggerStream StorageHandlerPostgreSQL::error() const { return mLog.error(logHeader()); }
const string StorageHandlerPostgreSQL::logHeader() const { stringstream s; s << "StorageHandlerPostgreSQL "; return s.str(); } 