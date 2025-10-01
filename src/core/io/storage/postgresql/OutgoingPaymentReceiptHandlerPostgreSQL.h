#ifndef VTCPD_OUTGOINGPAYMENTRECEIPTHANDLERPOSTGRESQL_H
#define VTCPD_OUTGOINGPAYMENTRECEIPTHANDLERPOSTGRESQL_H

#include "../interfaces/OutgoingPaymentReceiptHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../record/audit/ReceiptRecord.h"

#include <libpq-fe.h>
#include <string>
#include <map>
#include <vector>

using namespace crypto::sphincs;

class OutgoingPaymentReceiptHandlerPostgreSQL : public OutgoingPaymentReceiptHandler
{
public:
    OutgoingPaymentReceiptHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared ownPublicKeyHash,
        const TrustLineAmount &amount) override;

    std::map<TransactionUUID, TrustLineAmount> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    std::vector<ReceiptRecord::Shared> receiptsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    std::vector<ReceiptRecord::Shared> receiptsLessEqualThanAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    void deleteRecords(
        const TransactionUUID &transactionUUID) override;

    void deleteRecords(
        const TrustLineID trustLineID) override;

    bool isContainsKeyHash(
        KeyHash::Shared keyHash) override;

    bool isContainsTransaction(
        const TransactionUUID &transactionUUID) override;

    size_t countReceiptsByNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_OUTGOINGPAYMENTRECEIPTHANDLERPOSTGRESQL_H 