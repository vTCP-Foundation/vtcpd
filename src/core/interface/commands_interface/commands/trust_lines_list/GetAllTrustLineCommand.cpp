#include "GetAllTrustLineCommand.h"

GetAllTrustLineCommand::GetAllTrustLineCommand(
    const CommandUUID &uuid,
    const string &commandBuffer):

    BaseUserCommand(
        uuid,
        identifier())
{
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("GetAllTrustLineCommand: there is no input ");
        }
    };
    auto trustLinesFromParse = [&](auto &ctx){
        mFrom = _attr(ctx);
    };
    auto trustLinesCountParse = [&](auto &ctx) {
        mCount = _attr(ctx);
    };

    try {
        parse(
            commandBuffer.begin(),
            commandBuffer.end(),
            char_[check]);
        parse(
            commandBuffer.begin(),
            commandBuffer.end(), (
                *(int_[trustLinesFromParse])
                > char_(kTokensSeparator)
                > *(int_[trustLinesCountParse])
                > eol > eoi));
    } catch(...) {
        throw ValueError("GetTrustLinesCommand: cannot parse command.");
    }
}

const string &GetAllTrustLineCommand::identifier()
{
    static const string kIdentifier = " GET:contractors/trust-lines-all";
    return kIdentifier;
}

const size_t GetAllTrustLineCommand::from() const
{
    return mFrom;
}

const size_t GetAllTrustLineCommand::count() const
{
    return mCount;
}

CommandResult::SharedConst GetAllTrustLineCommand::resultOk(
    string &neighbors) const
{
    return make_shared<const CommandResult>(
        identifier(),
        UUID(),
        200,
        neighbors);
}
