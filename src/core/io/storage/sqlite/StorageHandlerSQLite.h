#ifndef VTCPD_STORAGEHANDLERSQLITE_H
#define VTCPD_STORAGEHANDLERSQLITE_H

#define SQLITE_DBCONFIG_RESET_DATABASE        1009 /* int int* */
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../interfaces/StorageHandler.h"
#include "IOTransactionSQLite.h"
#include "TrustLineHandlerSQLite.h"
#include "TransactionsHandlerSQLite.h"
#include "HistoryStorageSQLite.h"
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
#include <sqlite3.h>
#include <boost/filesystem.hpp>
#include <vector>
namespace fs = boost::filesystem;
class StorageHandlerSQLite : public StorageHandler
{
public:
    StorageHandlerSQLite(
        const string &directory,
        const string &dataBaseName,
        Logger &logger);
    ~StorageHandlerSQLite();
    IOTransaction::Shared beginTransaction() override;
    void vacuum() override;
    /**
     * Provides shared table name for payment transactions table.
     */
    static const char* paymentTransactionsTableName()
    {
        return kPaymentTransactionsTableName;
    }
private:
    static void checkDirectory(
        const string &directory);
    static sqlite3* connection(
        const string &directory,
        const string &dataBaseName,
        Logger &logger);
    LoggerStream info() const;
    LoggerStream warning() const;
    LoggerStream error() const;
    const string logHeader() const;
    const string kTrustLineTableName = "trust_lines";
    const string kTransactionTableName = "transactions";
    const string kHistoryMainTableName = "history";
    const string kHistoryAdditionalTableName = "history_additional";
    const string kOwnKeysTableName = "own_keys";
    const string kContractorKeysTableName = "contractor_keys";
    const string kOutgoingReceiptTableName = "outgoing_receipt";
    const string kIncomingReceiptTableName = "incoming_receipt";
    const string kAuditTableName = "audit";
    const string kPaymentKeysTableName = "payment_keys";
    const string kPaymentParticipantsVotesTableName = "payment_participants_votes";
    const string kContractorsTableName = "contractors";
    const string kContractorAddressesTableName = "contractors_addresses";
    const string kFeaturesTableName = "features";
    static sqlite3 *mDBConnection;
    static string mCurrentDatabasePath;
    Logger &mLog;
    TrustLineHandlerSQLite mTrustLineHandler;
    TransactionsHandlerSQLite mTransactionHandler;
    HistoryStorageSQLite mHistoryStorage;
    OwnKeysHandlerSQLite mOwnKeysHandler;
    ContractorKeysHandlerSQLite mContractorKeysHandler;
    AuditHandlerSQLite mAuditHandler;
    IncomingPaymentReceiptHandlerSQLite mIncomingPaymentReceiptHandler;
    OutgoingPaymentReceiptHandlerSQLite mOutgoingPaymentReceiptHandler;
    PaymentKeysHandlerSQLite mPaymentKeysHandler;
    PaymentParticipantsVotesHandlerSQLite mPaymentParticipantsVotesHandler;
    PaymentTransactionsHandlerSQLite mPaymentTransactionsHandler;
    ContractorsHandlerSQLite mContractorsHandler;
    AddressHandlerSQLite mAddressHandler;
    FeaturesHandlerSQLite mFeaturesHandler;
    string mDirectory;
    string mDataBaseName;

    // Table name for payment transactions used by storage handlers.
    static constexpr const char kPaymentTransactionsTableName[] = "payment_transactions";
};
#endif //VTCPD_STORAGEHANDLERSQLITE_H
