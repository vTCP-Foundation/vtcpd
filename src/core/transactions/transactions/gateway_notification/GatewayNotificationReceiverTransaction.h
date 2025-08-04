#ifndef VTCPD_GATEWAYNOTIFICATIONRECEIVERTRANSACTION_H
#define VTCPD_GATEWAYNOTIFICATIONRECEIVERTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../io/storage/interfaces/StorageHandler.h"
#include "../../../network/messages/gateway_notification_and_routing_tables/GatewayNotificationMessage.h"
#include "../../../network/messages/base/transaction/ConfirmationMessage.h"
#include "../../../network/messages/gateway_notification_and_routing_tables/RoutingTableResponseMessage.h"

class GatewayNotificationReceiverTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<GatewayNotificationReceiverTransaction> Shared;

public:
    GatewayNotificationReceiverTransaction(
        GatewayNotificationMessage::Shared message,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    vector<BaseAddress::Shared> getNeighborsForEquivalent(
        const SerializedEquivalent equivalent) const;

private:
    GatewayNotificationMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    StorageHandler *mStorageHandler;
};


#endif //VTCPD_GATEWAYNOTIFICATIONRECEIVERTRANSACTION_H
