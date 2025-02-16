#ifndef VTCPD_AUDITRULECOUNTPAYMENTS_H
#define VTCPD_AUDITRULECOUNTPAYMENTS_H

#include "BaseAuditRule.h"

class AuditRuleCountPayments : public BaseAuditRule
{
public:
    typedef shared_ptr<AuditRuleCountPayments> Shared;

public:
    AuditRuleCountPayments(
        uint32_t countPayments);

    bool check(
        TrustLine::Shared trustLine,
        IOTransaction::Shared ioTransaction=nullptr) override;

    void reset() override;

    uint32_t countPayments() const;

private:
    uint32_t mCountPayments;
};


#endif //VTCPD_AUDITRULECOUNTPAYMENTS_H
