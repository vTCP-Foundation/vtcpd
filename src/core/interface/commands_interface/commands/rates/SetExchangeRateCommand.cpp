#include "SetExchangeRateCommand.h"

SetExchangeRateCommand::SetExchangeRateCommand(
    const CommandUUID &uuid,
    const string &commandBuffer) :
    BaseUserCommand(
        uuid,
        identifier())
{
    string exchangeRateStr, minExchangeAmountStr, maxExchangeAmountStr;
    uint32_t flagExchangeRate = 0, flagMinExchangeAmount = 0, flagMaxExchangeAmount = 0;

    auto check = [&](auto &ctx) {
        if(_attr(ctx) == kCommandsSeparator || _attr(ctx) == kTokensSeparator) {
            throw ValueError("SetExchangeRateCommand: input is empty.");
        }
    };

    auto equivalentFromParse = [&](auto &ctx) {
        mEquivalentFrom = _attr(ctx);
    };

    auto equivalentToParse = [&](auto &ctx) {
        mEquivalentTo = _attr(ctx);
    };

    auto exchangeRateAddNumber = [&](auto &ctx) {
        if(exchangeRateStr.front() == '0' && isdigit(exchangeRateStr.back())) {
            throw ValueError("SetExchangeRateCommand: exchangeRate contains leading zero.");
        }
        exchangeRateStr += _attr(ctx);
        flagExchangeRate++;
        if (flagExchangeRate > 1 && exchangeRateStr.front() == '0') {
            throw ValueError("SetExchangeRateCommand: exchangeRate contains leading zero.");
        }
    };

    auto exchangeRateShiftParse = [&](auto &ctx) {
        mExchangeRateShift = _attr(ctx);
    };

    auto minExchangeAmountAddNumber = [&](auto &ctx) {
        if(minExchangeAmountStr.front() == '0' && isdigit(minExchangeAmountStr.back())) {
            throw ValueError("SetExchangeRateCommand: minExchangeAmount contains leading zero.");
        }
        minExchangeAmountStr += _attr(ctx);
        flagMinExchangeAmount++;
        if (flagMinExchangeAmount > 1 && minExchangeAmountStr.front() == '0') {
            throw ValueError("SetExchangeRateCommand: minExchangeAmount contains leading zero.");
        }
    };

    auto maxExchangeAmountAddNumber = [&](auto &ctx) {
        if(maxExchangeAmountStr.front() == '0' && isdigit(maxExchangeAmountStr.back())) {
            throw ValueError("SetExchangeRateCommand: maxExchangeAmount contains leading zero.");
        }
        maxExchangeAmountStr += _attr(ctx);
        flagMaxExchangeAmount++;
        if (flagMaxExchangeAmount > 1 && maxExchangeAmountStr.front() == '0') {
            throw ValueError("SetExchangeRateCommand: maxExchangeAmount contains leading zero.");
        }
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
            > char_(kTokensSeparator)
            > *(digit [exchangeRateAddNumber] > !alpha > !punct)
            > char_(kTokensSeparator)
            > int_[exchangeRateShiftParse]
            > char_(kTokensSeparator)
            > *(digit [minExchangeAmountAddNumber] > !alpha > !punct)
            > char_(kTokensSeparator)
            > *(digit [maxExchangeAmountAddNumber] > !alpha > !punct)
            > eol > eoi);

        mExchangeRate = TrustLineAmount(exchangeRateStr);
        mMinExchangeAmount = TrustLineAmount(minExchangeAmountStr);
        mMaxExchangeAmount = TrustLineAmount(maxExchangeAmountStr);

    } catch(...) {
        throw ValueError("SetExchangeRateCommand: cannot parse command.");
    }
}

const string &SetExchangeRateCommand::identifier()
{
    static const string kIdentifier = "SET:RATE";
    return kIdentifier;
}

const TrustLineAmount& SetExchangeRateCommand::maxExchangeAmount() const
{
    return mMaxExchangeAmount;
}

const SerializedEquivalent SetExchangeRateCommand::equivalentFrom() const
{
    return mEquivalentFrom;
}

const SerializedEquivalent SetExchangeRateCommand::equivalentTo() const
{
    return mEquivalentTo;
}

const TrustLineAmount& SetExchangeRateCommand::exchangeRate() const
{
    return mExchangeRate;
}

const int16_t SetExchangeRateCommand::exchangeRateShift() const
{
    return mExchangeRateShift;
}

const TrustLineAmount& SetExchangeRateCommand::minExchangeAmount() const
{
    return mMinExchangeAmount;
}