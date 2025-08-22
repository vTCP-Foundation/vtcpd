#ifndef VTCPD_GETEXCHANGERATECOMMAND_H
#define VTCPD_GETEXCHANGERATECOMMAND_H

#include "../BaseUserCommand.h"

class GetExchangeRateCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<GetExchangeRateCommand> Shared;

public:
    GetExchangeRateCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

    const SerializedEquivalent equivalentFrom() const;

    const SerializedEquivalent equivalentTo() const;

    CommandResult::SharedConst resultOk(
        const string &exchangeRateData) const;

private:
    SerializedEquivalent mEquivalentFrom;
    SerializedEquivalent mEquivalentTo;
};

#endif //VTCPD_GETEXCHANGERATECOMMAND_H