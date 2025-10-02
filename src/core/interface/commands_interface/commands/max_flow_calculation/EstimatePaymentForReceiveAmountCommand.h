#ifndef VTCPD_ESTIMATEPAYMENTFORRECEIVEAMOUNTCOMMAND_H
#define VTCPD_ESTIMATEPAYMENTFORRECEIVEAMOUNTCOMMAND_H

#include "../BaseUserCommand.h"

class EstimatePaymentForReceiveAmountCommand : public BaseUserCommand
{
public:
    typedef shared_ptr<EstimatePaymentForReceiveAmountCommand> Shared;

public:
    EstimatePaymentForReceiveAmountCommand(
        const CommandUUID &uuid,
        const string &command);

    static const string &identifier();

    BaseAddress::Shared contractorAddress() const;

    TrustLineAmount receiveAmount() const;

    SerializedEquivalent receiverEquivalent() const;

    SerializedEquivalent senderEquivalent() const;

    CommandResult::SharedConst responseOk(
        const string &paymentAmount) const;

private:
    BaseAddress::Shared mContractorAddress;
    TrustLineAmount mReceiveAmount;
    SerializedEquivalent mReceiverEquivalent;
    SerializedEquivalent mSenderEquivalent;
};

#endif //VTCPD_ESTIMATEPAYMENTFORRECEIVEAMOUNTCOMMAND_H
