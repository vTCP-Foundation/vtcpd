#include "ListExchangeRatesCommand.h"

ListExchangeRatesCommand::ListExchangeRatesCommand(
    const CommandUUID &uuid,
    const string &commandBuffer) :
    BaseUserCommand(
        uuid,
        identifier())
{
    // LIST:RATES command has no parameters - just validate empty command buffer
    if (!commandBuffer.empty() && commandBuffer != "\n") {
        throw ValueError("ListExchangeRatesCommand: command should have no parameters.");
    }
}

const string &ListExchangeRatesCommand::identifier()
{
    static const string kIdentifier = "LIST:RATES";
    return kIdentifier;
}

CommandResult::SharedConst ListExchangeRatesCommand::resultOk(
    const string &exchangeRatesData) const
{
    return make_shared<const CommandResult>(
               identifier(),
               UUID(),
               200,
               exchangeRatesData);
}