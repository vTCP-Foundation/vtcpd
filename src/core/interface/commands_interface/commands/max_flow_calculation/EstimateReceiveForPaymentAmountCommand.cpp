#include "EstimateReceiveForPaymentAmountCommand.h"

EstimateReceiveForPaymentAmountCommand::EstimateReceiveForPaymentAmountCommand(
    const CommandUUID &uuid,
    const string &command):

    BaseUserCommand(
        uuid,
        identifier())
{
    std::string address, addressType;
    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("EstimateReceiveForPaymentAmountCommand: input is empty.");
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
            throw ValueError("EstimateReceiveForPaymentAmountCommand: cannot parse command. "
                             "Error occurred while parsing 'Contractor Address' token.");
        }
        address.erase();
    };
    auto paymentAmountParse = [&](auto &ctx) {
        mPaymentAmount = TrustLineAmount(_attr(ctx));
    };
    auto senderEquivalentParse = [&](auto &ctx) {
        mSenderEquivalent = _attr(ctx);
    };
    auto receiverEquivalentParse = [&](auto &ctx) {
        mReceiverEquivalent = _attr(ctx);
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
            > ulong_[paymentAmountParse]
            > char_(kTokensSeparator)
            > int_[senderEquivalentParse]
            > char_(kTokensSeparator)
            > int_[receiverEquivalentParse]
            > eol > eoi);

        // Validate payment amount
        if (mPaymentAmount == TrustLineAmount(0)) {
            throw ValueError("EstimateReceiveForPaymentAmountCommand: payment amount must be greater than 0.");
        }
    } catch(...) {
        throw ValueError("EstimateReceiveForPaymentAmountCommand: cannot parse command.");
    }
}

const string &EstimateReceiveForPaymentAmountCommand::identifier()
{
    static const string identifier = "GET:contractors/transactions/estimate/receive";
    return identifier;
}

BaseAddress::Shared EstimateReceiveForPaymentAmountCommand::contractorAddress() const
{
    return mContractorAddress;
}

TrustLineAmount EstimateReceiveForPaymentAmountCommand::paymentAmount() const
{
    return mPaymentAmount;
}

SerializedEquivalent EstimateReceiveForPaymentAmountCommand::senderEquivalent() const
{
    return mSenderEquivalent;
}

SerializedEquivalent EstimateReceiveForPaymentAmountCommand::receiverEquivalent() const
{
    return mReceiverEquivalent;
}

CommandResult::SharedConst EstimateReceiveForPaymentAmountCommand::responseOk(
    const string &receiveAmount) const
{
    return CommandResult::SharedConst(
        new CommandResult(
            identifier(),
            UUID(),
            200,
            receiveAmount));
}
