#ifndef VTCPD_LISTEXCHANGERATESCOMMAND_H
#define VTCPD_LISTEXCHANGERATESCOMMAND_H

#include "../BaseUserCommand.h"

class ListExchangeRatesCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<ListExchangeRatesCommand> Shared;

public:
    ListExchangeRatesCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

    CommandResult::SharedConst resultOk(
        const string &exchangeRatesData) const;

private:
    // No parameters for LIST:RATES
};

#endif //VTCPD_LISTEXCHANGERATESCOMMAND_H