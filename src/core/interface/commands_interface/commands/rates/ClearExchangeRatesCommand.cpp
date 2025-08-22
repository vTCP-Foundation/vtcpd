#include "ClearExchangeRatesCommand.h"

ClearExchangeRatesCommand::ClearExchangeRatesCommand(
    const CommandUUID &uuid,
    const string &commandBuffer) :
    BaseUserCommand(
        uuid,
        identifier())
{
    // CLEAR:RATES command has no parameters - just validate empty command buffer
    if (!commandBuffer.empty() && commandBuffer != "\n") {
        throw ValueError("ClearExchangeRatesCommand: command should have no parameters.");
    }
}

const string &ClearExchangeRatesCommand::identifier()
{
    static const string kIdentifier = "CLEAR:RATES";
    return kIdentifier;
}