#ifndef VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONCOMMAND_H
#define VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONCOMMAND_H

#include "../BaseUserCommand.h"

class InitiateMaxFlowExchangeCalculationCommand : public BaseUserCommand
{

public:
    typedef shared_ptr<InitiateMaxFlowExchangeCalculationCommand> Shared;

public:
    InitiateMaxFlowExchangeCalculationCommand(
        const CommandUUID &uuid,
        const string &command);

    static const string &identifier();

    const vector<BaseAddress::Shared> &contractorAddresses() const;

    const SerializedEquivalent equivalent() const;

    const vector<SerializedEquivalent> &exchangeEquivalents() const;

    CommandResult::SharedConst responseOk(
        string &maxFlowAmount) const;

private:
    size_t mContractorsCount;
    vector<BaseAddress::Shared> mContractorAddresses;
    SerializedEquivalent mEquivalent;
    vector<SerializedEquivalent> mExchangeEquivalents;
};

#endif //VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONCOMMAND_H