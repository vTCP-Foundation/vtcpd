#include "InitiateMaxFlowExchangeCalculationCommand.h"

InitiateMaxFlowExchangeCalculationCommand::InitiateMaxFlowExchangeCalculationCommand(
    const CommandUUID &uuid,
    const string &command):

    BaseUserCommand(
        uuid,
        identifier())
{
    std::string address, addressType;
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("InitiateMaxFlowExchangeCalculationCommand: input is empty.");
        }
    };
    auto addressTypeParse = [&](auto &ctx) {
        addressType += _attr(ctx);
    };
    auto addressAddChar = [&](auto &ctx) {
        address += _attr(ctx);
    };
    auto addressAddNumber = [&](auto &ctx) {
        address += std::to_string(_attr(ctx));
    };
    auto addressesCountParse = [&](auto &ctx) {
        mContractorsCount = _attr(ctx);
    };
    auto addressAddToVector = [&](auto &ctx) {
        switch (std::atoi(addressType.c_str())) {
        case BaseAddress::IPv4_IncludingPort: {
            mContractorAddresses.push_back(
                make_shared<IPv4WithPortAddress>(
                    address));
            addressType.erase();
            break;
        }
        case BaseAddress::GNS: {
            mContractorAddresses.push_back(
                make_shared<GNSAddress>(
                    address));
            addressType.erase();
            break;
        }
        default:
            throw ValueError("InitiateMaxFlowExchangeCalculationCommand: cannot parse command. "
                             "Error occurred while parsing 'Contractor Address' token.");
        }
        address.erase();
    };
    auto equivalentParse = [&](auto &ctx) {
        mEquivalent = _attr(ctx);
    };
    auto exchangeEquivalentParse = [&](auto &ctx) {
        mExchangeEquivalents.push_back(_attr(ctx));
    };

    try {
        parse(
            command.begin(),
            command.end(),
            char_[check]);
        parse(
            command.begin(),
            command.end(),
            *(int_[addressesCountParse]-char_(kTokensSeparator)) > char_(kTokensSeparator));
        
        mContractorAddresses.reserve(mContractorsCount);
        parse(
            command.begin(),
            command.end(),
            *(int_)
            > char_(kTokensSeparator)
            > addressLexeme<
            decltype(addressAddChar),
            decltype(addressAddNumber),
            decltype(addressTypeParse),
            decltype(addressAddToVector)>(
                mContractorsCount,
                addressAddChar,
                addressAddNumber,
                addressTypeParse,
                addressAddToVector)
            > int_[equivalentParse]
            > *(char_(kTokensSeparator) > int_[exchangeEquivalentParse]) > eol > eoi);
            
        // Validate exchangeEquivalents limit after parsing
        if (mExchangeEquivalents.size() > 5) {
            throw ValueError("InitiateMaxFlowExchangeCalculationCommand: exchangeEquivalents limit exceeded (maximum 5 elements).");
        }
    } catch(...) {
        throw ValueError("InitiateMaxFlowExchangeCalculationCommand: cannot parse command.");
    }
}

const string &InitiateMaxFlowExchangeCalculationCommand::identifier()
{
    static const string identifier = "GET:contractors/transactions/max/exchange";
    return identifier;
}

const vector<BaseAddress::Shared>& InitiateMaxFlowExchangeCalculationCommand::contractorAddresses() const
{
    return mContractorAddresses;
}

const SerializedEquivalent InitiateMaxFlowExchangeCalculationCommand::equivalent() const
{
    return mEquivalent;
}

const vector<SerializedEquivalent>& InitiateMaxFlowExchangeCalculationCommand::exchangeEquivalents() const
{
    return mExchangeEquivalents;
}

CommandResult::SharedConst InitiateMaxFlowExchangeCalculationCommand::responseOk(
    string &maxFlowAmount) const
{
    return CommandResult::SharedConst(
               new CommandResult(
                   identifier(),
                   UUID(),
                   200,
                   maxFlowAmount));
}