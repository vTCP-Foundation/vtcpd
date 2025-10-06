#include "EstimateReceiveForPaymentAmountTransaction.h"
#include "../../../contractors/ContractorsManager.h"

EstimateReceiveForPaymentAmountTransaction::EstimateReceiveForPaymentAmountTransaction(
    EstimateReceiveForPaymentAmountCommand::Shared command,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangePathsManager *exchangePathsManager,
    Logger &logger) :

    BaseTransaction(
        BaseTransaction::EstimateReceiveForPaymentAmountTransactionType,
        command->senderEquivalent(),
        logger),
    mCommand(command),
    mContractorsManager(contractorsManager),
    mRouter(equivalentsSubsystemsRouter),
    mExchangePathsManager(exchangePathsManager)
{}

TransactionResult::SharedConst EstimateReceiveForPaymentAmountTransaction::run()
{
    debug() << "EstimateReceiveForPaymentAmount: payment=" << mCommand->paymentAmount()
            << ", sender_eq=" << mCommand->senderEquivalent()
            << ", receiver_eq=" << mCommand->receiverEquivalent();

    try {
        // Get contractor ID from address
        auto contractorID = mRouter->getOrCreateParticipantID(mCommand->contractorAddress());

        // Estimate receive amount using cached paths
        auto receiveAmount = estimateReceive(
            contractorID,
            mCommand->paymentAmount(),
            mCommand->senderEquivalent(),
            mCommand->receiverEquivalent());

        debug() << "EstimateReceiveForPaymentAmount: estimated receive=" << receiveAmount;
        return resultOK(receiveAmount);

    } catch (const runtime_error &e) {
        // Check for specific error messages
        string errorMsg = e.what();
        if (errorMsg.find("error 462") != string::npos) {
            warning() << "EstimateReceiveForPaymentAmount: " << errorMsg;
            return resultError(462);
        } else if (errorMsg.find("error 412") != string::npos) {
            warning() << "EstimateReceiveForPaymentAmount: " << errorMsg;
            return resultError(412);
        } else {
            error() << "EstimateReceiveForPaymentAmount: Unexpected error: " << errorMsg;
            return resultError(401);
        }
    } catch (const exception &e) {
        error() << "EstimateReceiveForPaymentAmount: Unexpected error: " << e.what();
        return resultError(401);
    }
}

TrustLineAmount EstimateReceiveForPaymentAmountTransaction::estimateReceive(
    ContractorID contractorID,
    TrustLineAmount paymentAmount,
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
    TrustLineAmount remainingPayment = paymentAmount;
    TrustLineAmount totalReceive = TrustLineAmount(0);

    set<pair<ContractorID, SerializedEquivalent>> appliedCommissions;
    map<EdgeKey, double> edgeRemainingCapacity;

    // Distribute payment across paths using forward simulation
    for (const auto &pathResult : *cachedPaths) {
        if (remainingPayment == TrustLineAmount(0)) {
            break;
        }

        // Determine how much payment to send through this path
        double maxPathInput = pathResult.optimal_flow.convert_to<double>();
        double pathInput = std::min(
            remainingPayment.convert_to<double>(),
            maxPathInput);

        // Forward simulate to get output amount
        double pathOutput = mExchangePathsManager->forwardSimulatePath(
            pathResult.path,
            pathInput,
            appliedCommissions,
            edgeRemainingCapacity);

        if (pathOutput <= 0.0) {
            continue; // Path produced no output
        }

        // Convert to TrustLineAmount safely
        TrustLineAmount pathOutputAmount(static_cast<uint64_t>(pathOutput));
        TrustLineAmount pathInputAmount(static_cast<uint64_t>(pathInput));

        totalReceive = totalReceive + pathOutputAmount;
        remainingPayment = remainingPayment - pathInputAmount;
    }

    // Check if all payment was consumed
    if (remainingPayment > TrustLineAmount(0)) {
        warning() << "Insufficient path capacity to consume full payment amount " << paymentAmount
                  << "; only " << (paymentAmount - remainingPayment) << " could be sent";
        throw runtime_error("Insufficient paths (error 412)");
    }

    return totalReceive;
}

TransactionResult::SharedConst EstimateReceiveForPaymentAmountTransaction::resultOK(
    const TrustLineAmount &receiveAmount) const
{
    stringstream ss;
    ss << receiveAmount;
    return transactionResultFromCommand(
        mCommand->responseOk(ss.str()));
}

TransactionResult::SharedConst EstimateReceiveForPaymentAmountTransaction::resultError(
    uint16_t errorCode) const
{
    auto commandResult = make_shared<CommandResult>(
        mCommand->identifier(),
        mCommand->UUID(),
        errorCode);
    return make_shared<TransactionResult>(commandResult);
}

const string EstimateReceiveForPaymentAmountTransaction::logHeader() const
{
    stringstream s;
    s << "[EstimateReceiveForPaymentAmountTA: " << currentTransactionUUID().stringUUID() << "]";
    return s.str();
}
