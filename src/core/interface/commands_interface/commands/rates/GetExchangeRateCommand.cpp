#include "GetExchangeRateCommand.h"

GetExchangeRateCommand::GetExchangeRateCommand(
    const CommandUUID &uuid,
    const string &commandBuffer) :
    BaseUserCommand(
        uuid,
        identifier())
{
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("GetExchangeRateCommand: input is empty.");
        }
    };

    auto equivalentFromParse = [&](auto &ctx) {
        mEquivalentFrom = _attr(ctx);
    };

    auto equivalentToParse = [&](auto &ctx) {
        mEquivalentTo = _attr(ctx);
    };

    try {
        parse(
            commandBuffer.begin(),
            commandBuffer.end(),
            char_[check]);
        parse(
            commandBuffer.begin(),
            commandBuffer.end(),
            *(int_[equivalentFromParse])
            > char_(kTokensSeparator)
            > *(int_[equivalentToParse])
            > eol > eoi);

    } catch(...) {
        throw ValueError("GetExchangeRateCommand: cannot parse command.");
    }
}

const string &GetExchangeRateCommand::identifier()
{
    static const string kIdentifier = "GET:RATE";
    return kIdentifier;
}

const SerializedEquivalent GetExchangeRateCommand::equivalentFrom() const
{
    return mEquivalentFrom;
}

const SerializedEquivalent GetExchangeRateCommand::equivalentTo() const
{
    return mEquivalentTo;
}

CommandResult::SharedConst GetExchangeRateCommand::resultOk(
    const string &exchangeRateData) const
{
    return make_shared<const CommandResult>(
               identifier(),
               UUID(),
               200,
               exchangeRateData);
}