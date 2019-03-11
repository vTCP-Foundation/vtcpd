#include "InitTrustLineCommand.h"

InitTrustLineCommand::InitTrustLineCommand(
    const CommandUUID &commandUUID,
    const string &command) :
    BaseUserCommand(
        commandUUID,
        identifier())
{
    std::string address;
    uint32_t addressType, addressesCount, equivalentID;
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator) {
            throw ValueError("InitTrustLineCommand: there is no input ");
        }
    };
    auto parserType = [&](auto &ctx) {
        addressType = _attr(ctx);
    };
    auto address_add = [&](auto &ctx) {
        address += _attr(ctx);
    };
    auto address_number_add = [&](auto &ctx) {
        address += std::to_string(_attr(ctx));
    };
    auto address_Count = [&](auto &ctx) {
        addressesCount = _attr(ctx);
    };
    auto address_vector = [&](auto &ctx) {
        switch (addressType) {
            case BaseAddress::IPv4_IncludingPort: {
                mContractorAddresses.push_back(
                    make_shared<IPv4WithPortAddress>(
                        address));
                break;
            }
            default:
                throw ValueError("InitTrustLineCommand: can't parse command. "
                    "Error occurred while parsing 'Contractor Address' token.");
        }
        address.erase();
    };
    auto equivalentID_add = [&](auto &ctx) {
        equivalentID = _attr(ctx);
    };

    try {
        parse(
            command.begin(),
            command.end(),
            char_[check]);
        parse(
            command.begin(),
            command.end(),
            *(int_[address_Count]-char_(kTokensSeparator)) > char_(kTokensSeparator));
        mContractorAddresses.reserve(addressesCount);
        parse(
            command.begin(),
            command.end(), (
                *(int_[address_Count]) > char_(kTokensSeparator)
                > repeat(addressesCount)[*(int_[parserType] - char_(kTokensSeparator)) > char_(kTokensSeparator)
                > repeat(3)[int_[address_number_add]> char_('.') [address_add]]
                > int_[address_number_add] > char_(':') [address_add]
                > int_[address_number_add] > char_(kTokensSeparator) [address_vector]]
                > +(int_[equivalentID_add]) > eol));
    } catch(...) {
        throw ValueError("InitTrustLineCommand: can't parse command.");
    }
    mEquivalent = equivalentID;
}

const string &InitTrustLineCommand::identifier()
noexcept
{
    static const string identifier = "INIT:contractors/trust-line";
    return identifier;
}

const SerializedEquivalent InitTrustLineCommand::equivalent() const
noexcept
{
    return mEquivalent;
}

vector<BaseAddress::Shared> InitTrustLineCommand::contractorAddresses() const
noexcept
{
    return mContractorAddresses;
}