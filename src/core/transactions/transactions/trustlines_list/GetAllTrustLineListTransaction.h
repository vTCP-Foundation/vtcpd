#ifndef GEO_NETWORK_CLIENT_GETALLTRUSTLINESLISTTRANSACTION_H
#define GEO_NETWORK_CLIENT_GETALLTRUSTLINESLISTTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../interface/commands_interface/commands/trust_lines_list/GetAllTrustLineCommand.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../contractors/ContractorsManager.h"

class GetAllTrustLineListTransaction :
    public BaseTransaction {

public:
    typedef shared_ptr<GetAllTrustLineListTransaction> Shared;

public:
    GetAllTrustLineListTransaction(
        GetAllTrustLineCommand::Shared command,
        ContractorsManager* contractorsManager,
		EquivalentsSubsystemsRouter* equivalentRouter,
        Logger &logger) noexcept;

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    GetAllTrustLineCommand::Shared mCommand;
	EquivalentsSubsystemsRouter* mEquivalentsSubsystemsRouter;
    ContractorsManager *mContractorsManager;
};

#endif //GEO_NETWORK_CLIENT_GETALLTRUSTLINESLISTTRANSACTION_H
