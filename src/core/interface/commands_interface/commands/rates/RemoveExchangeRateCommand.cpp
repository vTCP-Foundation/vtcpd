#include "RemoveExchangeRateCommand.h"

RemoveExchangeRateCommand::RemoveExchangeRateCommand(
    const CommandUUID &uuid,
    const string &commandBuffer) :
    BaseUserCommand(
        uuid,
        identifier())
{
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("RemoveExchangeRateCommand: input is empty.");
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
        throw ValueError("RemoveExchangeRateCommand: cannot parse command.");
    }
}

const string &RemoveExchangeRateCommand::identifier()
{
    static const string kIdentifier = "DEL:RATE";
    return kIdentifier;
}

const SerializedEquivalent RemoveExchangeRateCommand::equivalentFrom() const
{
    return mEquivalentFrom;
}

const SerializedEquivalent RemoveExchangeRateCommand::equivalentTo() const
{
    return mEquivalentTo;
}