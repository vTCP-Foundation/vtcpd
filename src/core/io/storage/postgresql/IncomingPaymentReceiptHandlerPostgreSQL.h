#ifndef VTCPD_INCOMINGPAYMENTRECEIPTHANDLERPOSTGRESQL_H
#define VTCPD_INCOMINGPAYMENTRECEIPTHANDLERPOSTGRESQL_H

#include "../interfaces/IncomingPaymentReceiptHandler.h"
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
#include <vector>
#include <map>

using namespace crypto::sphincs;

class IncomingPaymentReceiptHandlerPostgreSQL : public IncomingPaymentReceiptHandler
{
public:
    IncomingPaymentReceiptHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared contractorPublicKeyHash,
        const TrustLineAmount &amount,
        const Signature::Shared contractorSignature) override;

    std::map<TransactionUUID, TrustLineAmount> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    std::vector<ReceiptRecord::Shared> receiptsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    std::vector<ReceiptRecord::Shared> receiptsLessEqualThanAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

    /**
     * Retrieves receipts with zero audit number that belong to finalized transactions.
     */
    std::vector<ReceiptRecord::Shared> getFinalizedReceiptsWithZeroAuditNumber(
        const TrustLineID trustLineID) override;

    /**
     * Updates audit number for receipts identified by transaction UUIDs.
     */
    void updateAuditNumberByTransactionUUIDs(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const std::vector<TransactionUUID> &transactionUUIDs) override;

    void deleteRecords(
        const TransactionUUID &transactionUUID) override;

    void deleteRecords(
        const TrustLineID trustLineID) override;

    /**
     * Task 20-02: Deletes all receipt records for a trust line and audit number.
     */
    void deleteRecordsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) override;

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

#endif // VTCPD_INCOMINGPAYMENTRECEIPTHANDLERPOSTGRESQL_H 
