#ifndef VTCPD_INTERFACES_IOTRANSACTION_H
#define VTCPD_INTERFACES_IOTRANSACTION_H

#include "../../../common/Types.h"
#include "TrustLineHandler.h"
#include "HistoryStorage.h"
#include "TransactionsHandler.h"
#include "OwnKeysHandler.h"
#include "ContractorKeysHandler.h"
#include "AuditHandler.h"
#include "IncomingPaymentReceiptHandler.h"
#include "OutgoingPaymentReceiptHandler.h"
#include "PaymentKeysHandler.h"
#include "PaymentParticipantsVotesHandler.h"
#include "PaymentTransactionsHandler.h"
#include "ContractorsHandler.h"
#include "AddressHandler.h"
#include "FeaturesHandler.h"

#include <memory>

using namespace std;

class IOTransaction
{
public:
    typedef shared_ptr<IOTransaction> Shared;

    virtual ~IOTransaction() = default;

    virtual TrustLineHandler *trustLinesHandler() = 0;
    virtual HistoryStorage *historyStorage() = 0;
    virtual TransactionsHandler *transactionHandler() = 0;
    virtual OwnKeysHandler *ownKeysHandler() = 0;
    virtual ContractorKeysHandler *contractorKeysHandler() = 0;
    virtual AuditHandler *auditHandler() = 0;
    virtual IncomingPaymentReceiptHandler *incomingPaymentReceiptHandler() = 0;
    virtual OutgoingPaymentReceiptHandler *outgoingPaymentReceiptHandler() = 0;
    virtual PaymentKeysHandler *paymentKeysHandler() = 0;
    virtual PaymentParticipantsVotesHandler *paymentParticipantsVotesHandler() = 0;
    virtual PaymentTransactionsHandler *paymentTransactionsHandler() = 0;
    virtual ContractorsHandler *contractorsHandler() = 0;
    virtual AddressHandler *addressHandler() = 0;
    virtual FeaturesHandler *featuresHandler() = 0;
    virtual void rollback() = 0;
};

#endif //VTCPD_INTERFACES_IOTRANSACTION_H 