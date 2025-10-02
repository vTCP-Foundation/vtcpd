#include "EstimatePaymentForReceiveAmountTransaction.h"
#include "../../../contractors/ContractorsManager.h"

EstimatePaymentForReceiveAmountTransaction::EstimatePaymentForReceiveAmountTransaction(
    EstimatePaymentForReceiveAmountCommand::Shared command,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangePathsManager *exchangePathsManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::EstimatePaymentForReceiveAmountTransactionType,
        command->senderEquivalent(),
        logger),
    mCommand(command),
    mContractorsManager(contractorsManager),
    mRouter(equivalentsSubsystemsRouter),
    mExchangePathsManager(exchangePathsManager)
{}

TransactionResult::SharedConst EstimatePaymentForReceiveAmountTransaction::run()
{
    debug() << "EstimatePaymentForReceiveAmount: receive=" << mCommand->receiveAmount()
            << ", sender_eq=" << mCommand->senderEquivalent()
            << ", receiver_eq=" << mCommand->receiverEquivalent();

    try {
        // Get contractor ID from address
        auto contractorID = mRouter->getOrCreateParticipantID(mCommand->contractorAddress());

        // Estimate payment amount using cached paths
        auto paymentAmount = estimatePayment(
            contractorID,
            mCommand->receiveAmount(),
            mCommand->senderEquivalent(),
            mCommand->receiverEquivalent());

        debug() << "EstimatePaymentForReceiveAmount: estimated payment=" << paymentAmount;
        return resultOK(paymentAmount);

    } catch (const runtime_error &e) {
        // Check for specific error messages
        string errorMsg = e.what();
        if (errorMsg.find("error 462") != string::npos) {
            warning() << "EstimatePaymentForReceiveAmount: " << errorMsg;
            return resultError(462);
        } else if (errorMsg.find("error 412") != string::npos) {
            warning() << "EstimatePaymentForReceiveAmount: " << errorMsg;
            return resultError(412);
        }
        error() << "EstimatePaymentForReceiveAmount: Unexpected error: " << e.what();
        return resultError(401);
    } catch (const exception &e) {
        error() << "EstimatePaymentForReceiveAmount: Unexpected error: " << e.what();
        return resultError(401);
    }
}

TrustLineAmount EstimatePaymentForReceiveAmountTransaction::estimatePayment(
    ContractorID contractorID,
    TrustLineAmount receiveAmount,
    SerializedEquivalent senderEquivalent,
    SerializedEquivalent receiverEquivalent)
{
    // Construct cache key
    PathCacheKey key{contractorID, senderEquivalent, receiverEquivalent};

    // Retrieve cached paths
    auto cachedPaths = mExchangePathsManager->retrievePaths(key);
    if (!cachedPaths) {
        warning() << "No cached optimal paths for contractor " << contractorID
                  << " with sender_eq=" << senderEquivalent
                  << " and receiver_eq=" << receiverEquivalent;
        throw runtime_error("No cached paths (error 462)");
    }

    // Initialize tracking structures
    TrustLineAmount remainingReceive = receiveAmount;
    TrustLineAmount totalPayment = TrustLineAmount(0);

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeRemainingCapacity;

    // Iterate through paths using inverse simulation
    for (const auto &pathResult : *cachedPaths) {
        if (remainingReceive == TrustLineAmount(0)) {
            break;
        }

        // Determine target output for this path
        double targetForPath = std::min(
            remainingReceive.convert_to<double>(),
            pathResult.received_amount.convert_to<double>());

        // Inverse simulate to calculate required input
        double requiredInput = mExchangePathsManager->inverseSimulatePath(
            pathResult.path,
            targetForPath,
            appliedCommissions,
            edgeRemainingCapacity);

        if (requiredInput <= 0.0) {
            continue; // Path cannot be used
        }

        // Convert to TrustLineAmount safely
        TrustLineAmount requiredInputAmount(static_cast<uint64_t>(requiredInput));
        TrustLineAmount deliveredAmount(static_cast<uint64_t>(targetForPath));

        totalPayment = totalPayment + requiredInputAmount;
        remainingReceive = remainingReceive - deliveredAmount;
    }

    // Check if target was achieved
    if (remainingReceive > TrustLineAmount(0)) {
        warning() << "Insufficient paths to deliver " << receiveAmount
                  << " to contractor " << contractorID
                  << "; delivered only " << (receiveAmount - remainingReceive);
        throw runtime_error("Insufficient paths (error 412)");
    }

    return totalPayment;
}

TransactionResult::SharedConst EstimatePaymentForReceiveAmountTransaction::resultOK(
    const TrustLineAmount &paymentAmount) const
{
    stringstream ss;
    ss << paymentAmount;
    return transactionResultFromCommand(
        mCommand->responseOk(ss.str()));
}

TransactionResult::SharedConst EstimatePaymentForReceiveAmountTransaction::resultError(
    uint16_t errorCode) const
{
    auto commandResult = make_shared<CommandResult>(
        mCommand->identifier(),
        mCommand->UUID(),
        errorCode);
    return make_shared<TransactionResult>(commandResult);
}

const string EstimatePaymentForReceiveAmountTransaction::logHeader() const
{
    stringstream s;
    s << "[EstimatePaymentForReceiveAmountTA: " << currentTransactionUUID().stringUUID() << "]";
    return s.str();
}
