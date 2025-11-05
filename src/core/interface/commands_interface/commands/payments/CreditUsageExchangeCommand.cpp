#include "CreditUsageExchangeCommand.h"

#include <algorithm>
#include <cctype>
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
    std::string maxAllowableAmount;
    std::string limitToken;

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
    auto limitTokenParse = [&](auto &ctx) {
        limitToken += _attr(ctx);
    };

    std::string commandTail;

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

        std::vector<std::string> tokens;
        tokens.reserve(8);

        size_t start = 0;
        while (start < commandTail.size()) {
            size_t separatorPos = commandTail.find(kTokensSeparator, start);
            if (separatorPos == std::string::npos) {
                std::string token = commandTail.substr(start);
                while (!token.empty() && (token.back() == '\n' || token.back() == '\r')) {
                    token.pop_back();
                }
                tokens.emplace_back(std::move(token));
                break;
            }

            tokens.emplace_back(commandTail.substr(start, separatorPos - start));
            start = separatorPos + 1;
        }

        if (tokens.size() < 4) {
            throw ValueError(
                "CreditUsageExchangeCommand: command tail has insufficient tokens.");
        }

        const auto &amountToken = tokens[0];
        const auto &equivalentToken = tokens[1];

        auto findLimitTokenIndex = [&tokens]() -> size_t {
            for (size_t idx = tokens.size(); idx-- > 2;) {
                if (tokens[idx].rfind("limit=", 0) == 0) {
                    return idx;
                }
            }
            return std::string::npos;
        };

        const size_t limitIndex = findLimitTokenIndex();
        if (limitIndex == std::string::npos) {
            throw ValueError(
                "CreditUsageExchangeCommand: maxAllowablePaymentAmount token is missing.");
        }

        if (limitIndex <= 2) {
            throw ValueError(
                "CreditUsageExchangeCommand: exchangeEquivalents must not be empty.");
        }

        auto ensureDigits = [](const std::string &value, const char *fieldName) {
            if (value.empty() ||
                !std::all_of(value.begin(), value.end(), ::isdigit)) {
                throw ValueError(
                    string("CreditUsageExchangeCommand: ") + fieldName +
                    " must contain digits only.");
            }
        };

        ensureDigits(amountToken, "amount");
        if (amountToken.size() == 1 && amountToken[0] == '0') {
            throw ValueError(
                "CreditUsageExchangeCommand: amount can't be 0.");
        }
        if (amountToken.size() > 1 && amountToken[0] == '0') {
            throw ValueError(
                "CreditUsageExchangeCommand: amount contains leading zero.");
        }
        mAmount = TrustLineAmount(amountToken);

        ensureDigits(equivalentToken, "equivalent");
        try {
            mEquivalent = static_cast<SerializedEquivalent>(std::stoul(equivalentToken));
        } catch (...) {
            throw ValueError(
                "CreditUsageExchangeCommand: equivalent value is out of range.");
        }

        mExchangeEquivalents.clear();
        for (size_t idx = 2; idx < limitIndex; ++idx) {
            ensureDigits(tokens[idx], "exchangeEquivalent");
            try {
                mExchangeEquivalents.push_back(
                    static_cast<SerializedEquivalent>(std::stoul(tokens[idx])));
            } catch (...) {
                throw ValueError(
                    "CreditUsageExchangeCommand: exchangeEquivalent value is out of range.");
            }
        }

        if (mExchangeEquivalents.empty()) {
            throw ValueError(
                "CreditUsageExchangeCommand: exchangeEquivalents must not be empty.");
        }
        if (mExchangeEquivalents.size() > 5) {
            throw ValueError(
                "CreditUsageExchangeCommand: exchangeEquivalents limit exceeded (maximum 5 elements).");
        }

        std::string maxAllowableToken = tokens[limitIndex];
        if (maxAllowableToken.rfind("limit=", 0) != 0) {
            throw ValueError(
                "CreditUsageExchangeCommand: maxAllowablePaymentAmount token must start with 'limit='.");
        }

        maxAllowableAmount = maxAllowableToken.substr(6);
        if (maxAllowableAmount.empty()) {
            throw ValueError(
                "CreditUsageExchangeCommand: maxAllowablePaymentAmount must not be empty.");
        }
        if (!std::all_of(maxAllowableAmount.begin(), maxAllowableAmount.end(), ::isdigit)) {
            throw ValueError(
                "CreditUsageExchangeCommand: maxAllowablePaymentAmount must contain digits only.");
        }
        if (maxAllowableAmount.size() > 1 && maxAllowableAmount.front() == '0') {
            throw ValueError(
                "CreditUsageExchangeCommand: maxAllowablePaymentAmount contains leading zero.");
        }

        if (limitIndex + 1 < tokens.size()) {
            mPayload = tokens[limitIndex + 1];
        }

        if (mPayload.length() > std::numeric_limits<PayloadLength>::max()) {
            throw ValueError("Payload length is too big");
        }

        if (maxAllowableAmount == "0") {
            mMaxAllowablePaymentAmount = std::numeric_limits<TrustLineAmount>::max();
        } else {
            mMaxAllowablePaymentAmount = TrustLineAmount(maxAllowableAmount);
        }
    } catch (const ValueError &) {
        throw;
    } catch (const boost::spirit::x3::expectation_failure<std::string::const_iterator> &e) {
        const auto errorOffset = static_cast<size_t>(
            std::distance(commandTail.cbegin(), e.where()));
        std::string context;
        if (errorOffset < commandTail.size()) {
            context = commandTail.substr(errorOffset);
        }
        throw ValueError(
            string("CreditUsageExchangeCommand: parse expectation failure near '")
            + context + "'.");
    } catch (const std::exception &e) {
        throw ValueError(
            string("CreditUsageExchangeCommand: cannot parse command. Reason: ")
            + e.what());
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

const TrustLineAmount& CreditUsageExchangeCommand::maxAllowablePaymentAmount() const
{
    return mMaxAllowablePaymentAmount;
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

CommandResult::SharedConst CreditUsageExchangeCommand::responseAllowablePaymentAmountExceeded() const
{
    return CommandResult::SharedConst(
        new CommandResult(
            identifier(),
            UUID(),
            415,
            "Allowable payment amount has been exceeded"));
}
