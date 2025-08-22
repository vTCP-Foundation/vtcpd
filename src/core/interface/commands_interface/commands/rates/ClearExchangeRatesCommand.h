#ifndef VTCPD_CLEAREXCHANGERATESCOMMAND_H
#define VTCPD_CLEAREXCHANGERATESCOMMAND_H

#include "../BaseUserCommand.h"

class ClearExchangeRatesCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<ClearExchangeRatesCommand> Shared;

public:
    ClearExchangeRatesCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

private:
    // No parameters for CLEAR:RATES
};

#endif //VTCPD_CLEAREXCHANGERATESCOMMAND_H