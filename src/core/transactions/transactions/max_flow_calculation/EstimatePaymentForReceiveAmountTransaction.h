#ifndef VTCPD_ESTIMATEPAYMENTFORRECEIVEAMOUNTTRANSACTION_H
#define VTCPD_ESTIMATEPAYMENTFORRECEIVEAMOUNTTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/max_flow_calculation/EstimatePaymentForReceiveAmountCommand.h"
#include "../../../paths/ExchangePathsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"

class EstimatePaymentForReceiveAmountTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<EstimatePaymentForReceiveAmountTransaction> Shared;

public:
    EstimatePaymentForReceiveAmountTransaction(
        EstimatePaymentForReceiveAmountCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangePathsManager *exchangePathsManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TrustLineAmount estimatePayment(
        ContractorID contractorID,
        TrustLineAmount receiveAmount,
        SerializedEquivalent senderEquivalent,
        SerializedEquivalent receiverEquivalent);

    TransactionResult::SharedConst resultOK(
        const TrustLineAmount &paymentAmount) const;

    TransactionResult::SharedConst resultError(
        uint16_t errorCode) const;

private:
    EstimatePaymentForReceiveAmountCommand::Shared mCommand;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mRouter;
    ExchangePathsManager *mExchangePathsManager;
};

#endif //VTCPD_ESTIMATEPAYMENTFORRECEIVEAMOUNTTRANSACTION_H
