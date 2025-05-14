#ifndef GEO_NETWORK_CLIENT_ALL_TRUST_LINE_COMMAND_H
#define GEO_NETWORK_CLIENT_ALL_TRUST_LINE_COMMAND_H

#include "../BaseUserCommand.h"

class GetAllTrustLineCommand : public BaseUserCommand {

public:
    typedef shared_ptr<GetAllTrustLineCommand> Shared;

public:
    GetAllTrustLineCommand(
        const CommandUUID &uuid,
        const string &commandBuffer);

    static const string &identifier();

    const size_t from() const;

    const size_t count() const;

    CommandResult::SharedConst resultOk(
        string &neighbors) const;

private:

    size_t mFrom;
    size_t mCount;
};

#endif //GEO_NETWORK_CLIENT_ALL_TRUST_LINE_COMMAND_H
