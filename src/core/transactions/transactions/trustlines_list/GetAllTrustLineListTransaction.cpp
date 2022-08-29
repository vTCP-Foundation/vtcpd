#include "GetAllTrustLineListTransaction.h"

GetAllTrustLineListTransaction::GetAllTrustLineListTransaction(
    GetAllTrustLineCommand::Shared command,
    ContractorsManager* contractorsManager,
    EquivalentsSubsystemsRouter* equivalentRouter,
    Logger &logger)
	noexcept:
    BaseTransaction(
        BaseTransaction::AllEquivalentsTrustLinesList,
        0,
        logger),
    mCommand(command),
    mEquivalentsSubsystemsRouter(equivalentRouter),
	mContractorsManager(contractorsManager)
{}

TransactionResult::SharedConst GetAllTrustLineListTransaction::run() {
	
	stringstream ss;

	const auto kEquivalentsCount = mEquivalentsSubsystemsRouter->equivalents().size();
	ss << kEquivalentsCount;

	for(const auto& itEquivalent : this->mEquivalentsSubsystemsRouter->equivalents()) {
		TrustLinesManager* trustLineManager = mEquivalentsSubsystemsRouter->trustLinesManager(itEquivalent);

		ss <<kTokensSeparator<<itEquivalent;
		const auto kNeighborsCount = trustLineManager->trustLines().size();

		if(mCommand->from() > kNeighborsCount - 1) {
			ss << "0";
		}
		else {

			// todo discuss if exclude non active TLs
			auto resultRecordsCount = min(mCommand->count(), kNeighborsCount - mCommand->from());
			ss << kTokensSeparator << to_string(resultRecordsCount);
			size_t recordIdx = 0;
			size_t currentRecordsCount = 0;
			for(const auto &kNodeIDAndTrustLine : trustLineManager->trustLines()) {
				if(recordIdx < mCommand->from()) {
					recordIdx++;
					continue;
				}
				recordIdx++;
				ss << kTokensSeparator << kNodeIDAndTrustLine.first
					<< kTokensSeparator << mContractorsManager->contractor(kNodeIDAndTrustLine.first)->outputString()
					<< kTokensSeparator << kNodeIDAndTrustLine.second->state()
					<< kTokensSeparator << kNodeIDAndTrustLine.second->isOwnKeysPresent()
					<< kTokensSeparator << kNodeIDAndTrustLine.second->isContractorKeysPresent()
					<< kTokensSeparator << kNodeIDAndTrustLine.second->incomingTrustAmount()
					<< kTokensSeparator << kNodeIDAndTrustLine.second->outgoingTrustAmount()
					<< kTokensSeparator << kNodeIDAndTrustLine.second->balance();
				currentRecordsCount++;
				if(currentRecordsCount == mCommand->count()) {
					break;
				}
			}
		}

	}

	ss << kCommandsSeparator;
    string kResultInfo = ss.str();
    
	return transactionResultFromCommand(
        mCommand->resultOk(
            kResultInfo));
}

const string GetAllTrustLineListTransaction::logHeader() const
{
    stringstream s;
    s << "[GetAllTrustLineListTA: " << currentTransactionUUID() << "] ";
    return s.str();
}


// count_equivalents \t equivalent_1 \t GetTrustLinesListTransaction result for equivalent_1 \t equivalent_2 \t GetTrustLinesListTransaction result for equivalent_1 .....