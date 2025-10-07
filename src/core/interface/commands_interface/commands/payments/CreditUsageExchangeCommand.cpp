#include "CreditUsageExchangeCommand.h"

#include <limits>

CreditUsageExchangeCommand::CreditUsageExchangeCommand(
    const CommandUUID &uuid,
    const string &commandBuffer):

    BaseUserCommand(
        uuid,
        identifier())
{
    std::string address;
    std::string amount;
    std::string addressType;
    size_t contractorAddressesCount;
    uint32_t amountDigitsCounter = 0;

    auto check = [&](auto &ctx) {
        if (_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("CreditUsageExchangeCommand: input is empty.");
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
        contractorAddressesCount = _attr(ctx);
    };
    auto amountAddNumber = [&](auto &ctx) {
        amount += _attr(ctx);
        amountDigitsCounter++;
        if (amountDigitsCounter == 1 && _attr(ctx) == '0') {
            throw ValueError("CreditUsageExchangeCommand: amount contains leading zero.");
        }
    };
    auto addressAddToVector = [&](auto &ctx) {
        switch (std::atoi(addressType.c_str())) {
        case BaseAddress::IPv4_IncludingPort: {
            mContractorAddresses.push_back(
                make_shared<IPv4WithPortAddress>(
                    address));
            addressType.clear();
            break;
        }
        case BaseAddress::GNS: {
            mContractorAddresses.push_back(
                make_shared<GNSAddress>(
                    address));
            addressType.clear();
            break;
        }
        default:
            throw ValueError(
                "CreditUsageExchangeCommand: cannot parse command. "
                "Error occurred while parsing 'Contractor Address' token.");
        }
        address.clear();
    };
    auto equivalentParse = [&](auto &ctx) {
        mEquivalent = _attr(ctx);
    };
    auto exchangeEquivalentParse = [&](auto &ctx) {
        mExchangeEquivalents.push_back(_attr(ctx));
    };
    auto payloadParse = [&](auto &ctx) {
        mPayload += _attr(ctx);
    };

    try {
        parse(
            commandBuffer.begin(),
            commandBuffer.end(),
            char_[check]);
        parse(
            commandBuffer.begin(),
            commandBuffer.end(),
            *(int_[addressesCountParse] - char_(kTokensSeparator)) > char_(kTokensSeparator));

        mContractorAddresses.reserve(contractorAddressesCount);

        std::string commandTail;
        auto tailCollector = [&](auto &ctx) {
            commandTail += _attr(ctx);
        };

        parse(
            commandBuffer.begin(),
            commandBuffer.end(),
            (
                *(int_)
                > char_(kTokensSeparator)
                > addressLexeme<
                    decltype(addressAddChar),
                    decltype(addressAddNumber),
                    decltype(addressTypeParse),
                    decltype(addressAddToVector)>(
                        contractorAddressesCount,
                        addressAddChar,
                        addressAddNumber,
                        addressTypeParse,
                        addressAddToVector)
                > *(char_[tailCollector])));

        parse(
            commandTail.begin(),
            commandTail.end(),
            (
                *(digit[amountAddNumber] > !alpha > !punct)
                > char_(kTokensSeparator)
                > +(int_[equivalentParse])
                > *(char_(kTokensSeparator) > int_[exchangeEquivalentParse])
                > -(char_(kTokensSeparator) > *(char_[payloadParse] - eol))
                > eol > eoi));

        if (mExchangeEquivalents.empty()) {
            throw ValueError(
                "CreditUsageExchangeCommand: exchangeEquivalents must not be empty.");
        }
        if (mExchangeEquivalents.size() > 5) {
            throw ValueError(
                "CreditUsageExchangeCommand: exchangeEquivalents limit exceeded (maximum 5 elements).");
        }

        mAmount = TrustLineAmount(amount);

        if (mPayload.length() > std::numeric_limits<PayloadLength>::max()) {
            throw ValueError("Payload length is too big");
        }
    } catch (...) {
        throw ValueError("CreditUsageExchangeCommand: cannot parse command.");
    }
}

const string &CreditUsageExchangeCommand::identifier()
{
    static const string identifier = "CREATE:contractors/transactions/exchange";
    return identifier;
}

vector<BaseAddress::Shared> CreditUsageExchangeCommand::contractorAddresses() const
{
    return mContractorAddresses;
}

const TrustLineAmount &CreditUsageExchangeCommand::amount() const
{
    return mAmount;
}

const SerializedEquivalent CreditUsageExchangeCommand::equivalent() const
{
    return mEquivalent;
}

const vector<SerializedEquivalent> &CreditUsageExchangeCommand::exchangeEquivalents() const
{
    return mExchangeEquivalents;
}

const std::string CreditUsageExchangeCommand::payload() const
{
    return mPayload;
}

CommandResult::SharedConst CreditUsageExchangeCommand::responseOK(
    string &transactionUUID) const
{
    return CommandResult::SharedConst(
        new CommandResult(
            identifier(),
            UUID(),
            201,
            transactionUUID));
}
