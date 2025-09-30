#ifndef VTCPD_INTERFACES_OUTGOINGPAYMENTRECEIPTHANDLER_H
#define VTCPD_INTERFACES_OUTGOINGPAYMENTRECEIPTHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../crypto/sphincsscheme.h"
#include "../record/audit/ReceiptRecord.h"

using namespace crypto::sphincs;

class OutgoingPaymentReceiptHandler
{
public:
    virtual ~OutgoingPaymentReceiptHandler() = default;

    virtual void saveRecord(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const KeyHash::Shared ownPublicKeyHash,
        const TrustLineAmount &amount) = 0;

    virtual map<TransactionUUID, TrustLineAmount> auditAmounts(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    virtual vector<ReceiptRecord::Shared> receiptsByAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    virtual vector<ReceiptRecord::Shared> receiptsLessEqualThanAuditNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;

    virtual void deleteRecords(
        const TransactionUUID &transactionUUID) = 0;

    virtual void deleteRecords(
        const TrustLineID trustLineID) = 0;

    virtual bool isContainsKeyHash(
        KeyHash::Shared keyHash) = 0;

    virtual bool isContainsTransaction(
        const TransactionUUID &transactionUUID) = 0;

    virtual size_t countReceiptsByNumber(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber) = 0;
};

#endif //VTCPD_INTERFACES_OUTGOINGPAYMENTRECEIPTHANDLER_H 