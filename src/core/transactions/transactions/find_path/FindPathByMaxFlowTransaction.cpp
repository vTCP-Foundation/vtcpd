#include "FindPathByMaxFlowTransaction.h"

FindPathByMaxFlowTransaction::FindPathByMaxFlowTransaction(
    BaseAddress::Shared contractorAddress,
    const TransactionUUID &requestedTransactionUUID,
    const SerializedEquivalent equivalent,
    ContractorsManager *contractorsManager,
    ResourcesManager *resourcesManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    TailManager *tailManager,
    Logger &logger,
    HopsCount_t hopsCount) :

    BaseCollectTopologyTransaction(
        BaseTransaction::FindPathByMaxFlowTransactionType,
        equivalent,
        contractorsManager,
        equivalentsSubsystemsRouter,
        tailManager,
        logger),
    mHopsCnt(hopsCount),
    mContractorAddress(contractorAddress),
    mRequestedTransactionUUID(requestedTransactionUUID),
    mPathsManager(equivalentsSubsystemsRouter->pathsManager(equivalent)),
    mResourcesManager(resourcesManager),
    mIamGateway(equivalentsSubsystemsRouter->iAmGateway(equivalent))
{}

TransactionResult::SharedConst FindPathByMaxFlowTransaction::sendRequestForCollectingTopology()
{
    debug() << "Build paths to " << mContractorAddress->fullAddress();
    try {
        vector<BaseAddress::Shared> contractors;
        contractors.push_back(mContractorAddress);
        mContractorID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
                            mContractorAddress);
        info() << "ContractorID " << mContractorID;
        const auto kTransaction = make_shared<CollectTopologyTransaction>(
                                      mEquivalent,
                                      contractors,
                                      mContractorsManager,
                                      mEquivalentsSubsystemsRouter,
                                      mTrustLinesManager,
                                      mTopologyTrustLineManager,
                                      mTopologyCacheManager,
                                      mMaxFlowCacheManager,
                                      mIamGateway,
                                      mLog,
                                      mHopsCnt);

        mTopologyTrustLineManager->setPreventDeleting(true);
        launchSubsidiaryTransaction(kTransaction);
    } catch (...) {
        warning() << "Can not launch Collecting Topology transaction";
    }

    mCountProcessCollectingTopologyRun = 0;
    return resultAwakeAfterMilliseconds(
               kTopologyCollectingMillisecondsTimeout);
}

TransactionResult::SharedConst FindPathByMaxFlowTransaction::processCollectingTopology()
{
    auto const contextSize = mTailManager->getFlowTail().size();
    fillTopology();
    mCountProcessCollectingTopologyRun++;
    if (contextSize > 0 && mCountProcessCollectingTopologyRun <= kCountRunningProcessCollectingTopologyStage) {
        return resultAwakeAfterMilliseconds(
                   kTopologyCollectingAgainMillisecondsTimeout);
    }

    mPathsManager->buildPaths(
        mContractorAddress,
        mContractorID);

    mResourcesManager->putResource(
        make_shared<PathsResource>(
            mRequestedTransactionUUID,
            mPathsManager->pathCollection()));

    mPathsManager->clearPathsCollection();
    mTopologyTrustLineManager->setPreventDeleting(false);
    return resultDone();
}

const string FindPathByMaxFlowTransaction::logHeader() const
{
    stringstream s;
    s << "[FindPathByMaxFlowTA: " << currentTransactionUUID() << " " << mEquivalent << "]";
    return s.str();
}
