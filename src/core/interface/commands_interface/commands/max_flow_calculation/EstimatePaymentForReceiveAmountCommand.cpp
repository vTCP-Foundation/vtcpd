#include "EstimatePaymentForReceiveAmountCommand.h"

EstimatePaymentForReceiveAmountCommand::EstimatePaymentForReceiveAmountCommand(
    const CommandUUID &uuid,
    const string &command):

    BaseUserCommand(
        uuid,
        identifier())
{
    std::string address, addressType;
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("EstimatePaymentForReceiveAmountCommand: input is empty.");
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
    auto addressAddToVector = [&](auto &ctx) {
        switch (std::atoi(addressType.c_str())) {
        case BaseAddress::IPv4_IncludingPort: {
            mContractorAddress = make_shared<IPv4WithPortAddress>(address);
            addressType.erase();
            break;
        }
        case BaseAddress::GNS: {
            mContractorAddress = make_shared<GNSAddress>(address);
            addressType.erase();
            break;
        }
        default:
            throw ValueError("EstimatePaymentForReceiveAmountCommand: cannot parse command. "
                             "Error occurred while parsing 'Contractor Address' token.");
        }
        address.erase();
    };
    auto receiveAmountParse = [&](auto &ctx) {
        mReceiveAmount = TrustLineAmount(_attr(ctx));
    };
    auto receiverEquivalentParse = [&](auto &ctx) {
        mReceiverEquivalent = _attr(ctx);
    };
    auto senderEquivalentParse = [&](auto &ctx) {
        mSenderEquivalent = _attr(ctx);
    };

    try {
        parse(
            command.begin(),
            command.end(),
            char_[check]);
        parse(
            command.begin(),
            command.end(),
            addressLexeme<
                decltype(addressAddChar),
                decltype(addressAddNumber),
                decltype(addressTypeParse),
                decltype(addressAddToVector)>(
                    1,
                    addressAddChar,
                    addressAddNumber,
                    addressTypeParse,
                    addressAddToVector)
            > ulong_[receiveAmountParse]
            > char_(kTokensSeparator)
            > int_[receiverEquivalentParse]
            > char_(kTokensSeparator)
            > int_[senderEquivalentParse]
            > eol > eoi);

        // Validate receive amount
        if (mReceiveAmount == TrustLineAmount(0)) {
            throw ValueError("EstimatePaymentForReceiveAmountCommand: receive amount must be greater than 0.");
        }
    } catch(...) {
        throw ValueError("EstimatePaymentForReceiveAmountCommand: cannot parse command.");
    }
}

const string &EstimatePaymentForReceiveAmountCommand::identifier()
{
    static const string identifier = "GET:contractors/transactions/estimate/payment";
    return identifier;
}

BaseAddress::Shared EstimatePaymentForReceiveAmountCommand::contractorAddress() const
{
    return mContractorAddress;
}

TrustLineAmount EstimatePaymentForReceiveAmountCommand::receiveAmount() const
{
    return mReceiveAmount;
}

SerializedEquivalent EstimatePaymentForReceiveAmountCommand::receiverEquivalent() const
{
    return mReceiverEquivalent;
}

SerializedEquivalent EstimatePaymentForReceiveAmountCommand::senderEquivalent() const
{
    return mSenderEquivalent;
}

CommandResult::SharedConst EstimatePaymentForReceiveAmountCommand::responseOk(
    const string &paymentAmount) const
{
    return CommandResult::SharedConst(
        new CommandResult(
            identifier(),
            UUID(),
            200,
            paymentAmount));
}
