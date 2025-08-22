#ifndef VTCPD_REMOVEEXCHANGERATECOMMAND_H
#define VTCPD_REMOVEEXCHANGERATECOMMAND_H

#include "../BaseUserCommand.h"

class RemoveExchangeRateCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<RemoveExchangeRateCommand> Shared;

public:
    RemoveExchangeRateCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

    const SerializedEquivalent equivalentFrom() const;

    const SerializedEquivalent equivalentTo() const;

private:
    SerializedEquivalent mEquivalentFrom;
    SerializedEquivalent mEquivalentTo;
};

#endif //VTCPD_REMOVEEXCHANGERATECOMMAND_H