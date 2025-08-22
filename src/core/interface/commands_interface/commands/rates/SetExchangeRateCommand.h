#ifndef VTCPD_SETEXCHANGERATECOMMAND_H
#define VTCPD_SETEXCHANGERATECOMMAND_H

#include "../BaseUserCommand.h"
#include "../../../../common/multiprecision/MultiprecisionUtils.h"

class SetExchangeRateCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<SetExchangeRateCommand> Shared;

public:
    SetExchangeRateCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

    const SerializedEquivalent equivalentFrom() const;

    const SerializedEquivalent equivalentTo() const;

    const TrustLineAmount& exchangeRate() const;

    const int16_t exchangeRateShift() const;

    const TrustLineAmount& minExchangeAmount() const;

    const TrustLineAmount& maxExchangeAmount() const;

private:
    SerializedEquivalent mEquivalentFrom;
    SerializedEquivalent mEquivalentTo;
    TrustLineAmount mExchangeRate;
    int16_t mExchangeRateShift;
    TrustLineAmount mMinExchangeAmount;
    TrustLineAmount mMaxExchangeAmount;
};

#endif //VTCPD_SETEXCHANGERATECOMMAND_H