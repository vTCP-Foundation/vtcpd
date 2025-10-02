#ifndef VTCPD_ESTIMATERECEIVEFORPAYMENTAMOUNTCOMMAND_H
#define VTCPD_ESTIMATERECEIVEFORPAYMENTAMOUNTCOMMAND_H

#include "../BaseUserCommand.h"

class EstimateReceiveForPaymentAmountCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<EstimateReceiveForPaymentAmountCommand> Shared;

public:
    EstimateReceiveForPaymentAmountCommand(
        const CommandUUID &uuid,
        const string &command);

    static const string &identifier();

    BaseAddress::Shared contractorAddress() const;

    TrustLineAmount paymentAmount() const;

    SerializedEquivalent senderEquivalent() const;

    SerializedEquivalent receiverEquivalent() const;

    CommandResult::SharedConst responseOk(
        const string &receiveAmount) const;

private:
    BaseAddress::Shared mContractorAddress;
    TrustLineAmount mPaymentAmount;
    SerializedEquivalent mSenderEquivalent;
    SerializedEquivalent mReceiverEquivalent;
};

#endif //VTCPD_ESTIMATERECEIVEFORPAYMENTAMOUNTCOMMAND_H
