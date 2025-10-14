#ifndef VTCPD_ESTIMATERECEIVEFORPAYMENTAMOUNTTRANSACTION_H
#define VTCPD_ESTIMATERECEIVEFORPAYMENTAMOUNTTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/max_flow_calculation/EstimateReceiveForPaymentAmountCommand.h"
#include "../../../paths/ExchangePathsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"

class EstimateReceiveForPaymentAmountTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<EstimateReceiveForPaymentAmountTransaction> Shared;

public:
    EstimateReceiveForPaymentAmountTransaction(
        EstimateReceiveForPaymentAmountCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangePathsManager *exchangePathsManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    TrustLineAmount estimateReceive(
        ContractorID contractorID,
        TrustLineAmount paymentAmount,
        SerializedEquivalent senderEquivalent,
        SerializedEquivalent receiverEquivalent);

    TransactionResult::SharedConst resultOK(
        const TrustLineAmount &receiveAmount) const;

    TransactionResult::SharedConst resultNoRoutes() const;

    TransactionResult::SharedConst resultInsufficientFunds() const;

    TransactionResult::SharedConst resultUnexpectedError() const;

private:
    EstimateReceiveForPaymentAmountCommand::Shared mCommand;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mRouter;
    ExchangePathsManager *mExchangePathsManager;
};

#endif //VTCPD_ESTIMATERECEIVEFORPAYMENTAMOUNTTRANSACTION_H
