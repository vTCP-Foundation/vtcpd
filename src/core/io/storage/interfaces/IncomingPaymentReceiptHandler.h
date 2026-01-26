#ifndef VTCPD_INTERFACES_INCOMINGPAYMENTRECEIPTHANDLER_H
#define VTCPD_INTERFACES_INCOMINGPAYMENTRECEIPTHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../crypto/sphincsscheme.h"
#include "../record/audit/ReceiptRecord.h"

using namespace crypto::sphincs;

class IncomingPaymentReceiptHandler
{
public:
    virtual ~IncomingPaymentReceiptHandler() = default;

    virtual void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared contractorPublicKeyHash,
        const TrustLineAmount &amount,
        const Signature::Shared contractorSignature) = 0;

    virtual map<TransactionUUID, TrustLineAmount> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    virtual vector<ReceiptRecord::Shared> receiptsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    virtual vector<ReceiptRecord::Shared> receiptsLessEqualThanAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    /**
     * Retrieves finalized receipts with zero audit number for specified trust line.
     */
    virtual vector<ReceiptRecord::Shared> getFinalizedReceiptsWithZeroAuditNumber(
        const TrustLineID trustLineID) = 0;

    /**
     * Updates audit number for receipts identified by transaction UUIDs.
     */
    virtual void updateAuditNumberByTransactionUUIDs(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const vector<TransactionUUID> &transactionUUIDs) = 0;

    virtual void deleteRecords(
        const TransactionUUID &transactionUUID) = 0;

    virtual void deleteRecords(
        const TrustLineID trustLineID) = 0;

    /**
     * Task 20-01: Deletes all receipt records for a trust line and audit number.
     */
    virtual void deleteRecordsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    virtual bool isContainsKeyHash(
        KeyHash::Shared keyHash) = 0;

    virtual bool isContainsTransaction(
        const TransactionUUID &transactionUUID) = 0;

    virtual size_t countReceiptsByNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;
};

#endif //VTCPD_INTERFACES_INCOMINGPAYMENTRECEIPTHANDLER_H 
