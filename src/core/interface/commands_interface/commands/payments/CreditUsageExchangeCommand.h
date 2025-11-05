#ifndef VTCPD_CREDITUSAGEEXCHANGECOMMAND_H
#define VTCPD_CREDITUSAGEEXCHANGECOMMAND_H

#include "../BaseUserCommand.h"
#include "../../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../../common/exceptions/MemoryError.h"

class CreditUsageExchangeCommand :
    public BaseUserCommand
{
public:
    using Shared = shared_ptr<CreditUsageExchangeCommand>;

public:
    CreditUsageExchangeCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

    const TrustLineAmount &amount() const;

    vector<BaseAddress::Shared> contractorAddresses() const;

    const SerializedEquivalent equivalent() const;

    const vector<SerializedEquivalent> &exchangeEquivalents() const;

    const std::string payload() const;

    const TrustLineAmount& maxAllowablePaymentAmount() const;

public:
    CommandResult::SharedConst responseOK(
        string &transactionUUID) const;

    CommandResult::SharedConst responseAllowablePaymentAmountExceeded() const;

private:
    vector<BaseAddress::Shared> mContractorAddresses;
    TrustLineAmount mAmount;
    SerializedEquivalent mEquivalent;
    vector<SerializedEquivalent> mExchangeEquivalents;
    std::string mPayload;
    TrustLineAmount mMaxAllowablePaymentAmount;
};

#endif // VTCPD_CREDITUSAGEEXCHANGECOMMAND_H
