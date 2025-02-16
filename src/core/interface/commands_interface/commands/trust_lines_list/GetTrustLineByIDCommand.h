#ifndef VTCPD_GETTRUSTLINEBYIDCOMMAND_H
#define VTCPD_GETTRUSTLINEBYIDCOMMAND_H

#include "../BaseUserCommand.h"

class GetTrustLineByIDCommand : public BaseUserCommand
{

public:
    typedef shared_ptr<GetTrustLineByIDCommand> Shared;

public:
    GetTrustLineByIDCommand(
        const CommandUUID &commandUUID,
        const string &commandBuffer);

    static const string &identifier();

    const ContractorID contractorID() const;

    const SerializedEquivalent equivalent() const;

    CommandResult::SharedConst resultOk(
        string &neighbor) const;

private:
    ContractorID mContractorID;
    SerializedEquivalent mEquivalent;
};

#endif //VTCPD_GETTRUSTLINEBYIDCOMMAND_H
