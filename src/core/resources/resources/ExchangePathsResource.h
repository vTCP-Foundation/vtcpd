#ifndef VTCPD_EXCHANGEPATHSRESOURCE_H
#define VTCPD_EXCHANGEPATHSRESOURCE_H

#include "BaseResource.h"
#include "../../transactions/transactions/base/TransactionUUID.h"

class ExchangePathsResource : public BaseResource
{
public:
    typedef shared_ptr<ExchangePathsResource> Shared;

public:
    ExchangePathsResource(
        const TransactionUUID &transactionUUID);

    const TransactionUUID& transactionUUID() const;

private:
    TransactionUUID mTransactionUUID;
};


#endif //VTCPD_EXCHANGEPATHSRESOURCE_H
