#include "TransactionState.h"
#include <limits>

TransactionState::TransactionState(
    Message::MessageType requiredMessageType,
    bool flushToPermanentStorage,
    bool awakeOnMessage) :

    mAwakeningTimestamp(0),
    mFlushToPermanentStorage(flushToPermanentStorage),
    mMustBeAwakenedOnMessage(awakeOnMessage),
    mMustSavePreviousStateState(false),
    mIsWaitingForRpcResponse(false),
    mMustBeAwakenedOnRpcResponse(false)
{
    mRequiredMessageTypes.push_back(requiredMessageType);
}

TransactionState::TransactionState(
    GEOEpochTimestamp awakeningTimestamp,
    bool flushToPermanentStorage,
    bool awakeOnMessage) :

    mFlushToPermanentStorage(flushToPermanentStorage),
    mMustBeAwakenedOnMessage(awakeOnMessage),
    mAwakeningTimestamp(awakeningTimestamp),
    mMustSavePreviousStateState(false),
    mIsWaitingForRpcResponse(false),
    mMustBeAwakenedOnRpcResponse(false)
{}

TransactionState::TransactionState(
    GEOEpochTimestamp awakeningTimestamp,
    Message::MessageType requiredMessageType,
    bool flushToPermanentStorage,
    bool awakeOnMessage) :

    mFlushToPermanentStorage(flushToPermanentStorage),
    mMustBeAwakenedOnMessage(awakeOnMessage),
    mAwakeningTimestamp(awakeningTimestamp),
    mMustSavePreviousStateState(false),
    mIsWaitingForRpcResponse(false),
    mMustBeAwakenedOnRpcResponse(false)
{
    mRequiredMessageTypes.push_back(requiredMessageType);
}

TransactionState::TransactionState(
    bool mustSavePreviousState) :

    mMustSavePreviousStateState(mustSavePreviousState),
    mIsWaitingForRpcResponse(false),
    mMustBeAwakenedOnRpcResponse(false)
{}

/*!
 * Returns TransactionState that simply closes the transaction.
 *
 * WARNING:
 * Do not use 0 as value for awakeningTimestamp.
 * It will break scheduler logic for choosing next transaction for execution.
 */
TransactionState::SharedConst TransactionState::exit()
{
    return make_shared<TransactionState>(
               numeric_limits<GEOEpochTimestamp>::max());
}

TransactionState::SharedConst TransactionState::flushAndContinue()
{
    return make_shared<TransactionState>(
               microsecondsSinceGEOEpoch(
                   utc_now()),
               true);
}

/*!
 * Returns TransactionState with awakening timestamp set to current UTC;
 */
TransactionState::SharedConst TransactionState::awakeAsFastAsPossible()
{
    return make_shared<TransactionState>(
               microsecondsSinceGEOEpoch(
                   utc_now()));
}

/*!
 * Returns TransactionState with awakening timestamp set to current UTC + timeout;
 */
TransactionState::SharedConst TransactionState::awakeAfterMilliseconds(
    uint32_t milliseconds)
{
    return make_shared<TransactionState>(
               microsecondsSinceGEOEpoch(
                   utc_now() + pt::microseconds(milliseconds * 1000)));
}

/*!
 * Returns TransactionState that specifies what kind of messages transaction is waiting and accepting.
 * Optionally, may be initialised with deadline timeout.
 */
TransactionState::SharedConst TransactionState::waitForMessageTypes(
    vector<Message::MessageType> &&requiredMessageType,
    uint32_t noLongerThanMilliseconds)
{
    TransactionState::Shared state;
    if (noLongerThanMilliseconds == 0) {
        state = const_pointer_cast<TransactionState> (TransactionState::exit());

    } else {
        state = const_pointer_cast<TransactionState> (TransactionState::awakeAfterMilliseconds(
                    noLongerThanMilliseconds));
    }

    state->mRequiredMessageTypes = requiredMessageType;

    return const_pointer_cast<const TransactionState>(state);
}

TransactionState::SharedConst TransactionState::waitForMessageTypesAndAwakeAfterMilliseconds(
    vector<Message::MessageType> &&requiredMessageType,
    uint32_t noLongerThanMilliseconds)
{
    TransactionState::Shared state;
    if (noLongerThanMilliseconds == 0) {
        state = const_pointer_cast<TransactionState> (TransactionState::exit());

    } else {
        state = make_shared<TransactionState>(
                    microsecondsSinceGEOEpoch(
                        utc_now() + pt::microseconds(noLongerThanMilliseconds * 1000)),
                    false,
                    false);
    }

    state->mRequiredMessageTypes = requiredMessageType;

    return const_pointer_cast<const TransactionState>(state);
}

TransactionState::SharedConst TransactionState::waitForResourcesTypes(
    vector<BaseResource::ResourceType> &&requiredResourcesType,
    uint32_t noLongerThanMilliseconds)
{
    TransactionState::Shared state;
    if (noLongerThanMilliseconds == 0) {
        state = const_pointer_cast<TransactionState> (TransactionState::exit());

    } else {
        state = const_pointer_cast<TransactionState> (TransactionState::awakeAfterMilliseconds(
                    noLongerThanMilliseconds));
    }

    state->mRequiredResourcesTypes = requiredResourcesType;

    return const_pointer_cast<const TransactionState>(state);
}

TransactionState::SharedConst TransactionState::waitForResourcesAndMessagesTypes(
    vector<BaseResource::ResourceType> &&requiredResourcesType,
    vector<Message::MessageType> &&requiredMessageType,
    uint32_t noLongerThanMilliseconds)
{
    TransactionState::Shared state;
    if (noLongerThanMilliseconds == 0) {
        state = const_pointer_cast<TransactionState> (TransactionState::exit());

    } else {
        state = const_pointer_cast<TransactionState> (TransactionState::awakeAfterMilliseconds(
                    noLongerThanMilliseconds));
    }

    state->mRequiredResourcesTypes = requiredResourcesType;
    state->mRequiredMessageTypes = requiredMessageType;

    return const_pointer_cast<const TransactionState>(state);
}

/*!
 * Returns TransactionState that waits for a specific RPC response.
 * Transaction awakens on RPC response arrival or timeout expiration.
 */
TransactionState::SharedConst TransactionState::waitForRpcResponse(
    RpcMethod requiredRpcMethod,
    uint32_t noLongerThanMilliseconds)
{
    TransactionState::Shared state;
    if (noLongerThanMilliseconds == 0) {
        state = const_pointer_cast<TransactionState>(
                    TransactionState::exit());
        state->mIsWaitingForRpcResponse = false;
        state->mMustBeAwakenedOnRpcResponse = false;
        state->mRequiredRpcMethods.clear();
        state->mMustBeAwakenedOnMessage = false;
        return const_pointer_cast<const TransactionState>(state);
    }

    state = const_pointer_cast<TransactionState>(
                TransactionState::awakeAfterMilliseconds(
                    noLongerThanMilliseconds));

    state->mRequiredRpcMethods.clear();
    state->mRequiredRpcMethods.push_back(requiredRpcMethod);
    state->mIsWaitingForRpcResponse = true;
    state->mMustBeAwakenedOnRpcResponse = true;
    state->mMustBeAwakenedOnMessage = false;

    return const_pointer_cast<const TransactionState>(state);
}

TransactionState::SharedConst TransactionState::waitForRcpResponseAndMessagesTypes(
    RpcMethod requiredRpcMethod,
    vector<Message::MessageType> &&requiredMessageType,
    uint32_t noLongerThanMilliseconds)
{
    TransactionState::Shared state;
    if (noLongerThanMilliseconds == 0) {
        state = const_pointer_cast<TransactionState> (TransactionState::exit());

    } else {
        state = const_pointer_cast<TransactionState> (TransactionState::awakeAfterMilliseconds(
                    noLongerThanMilliseconds));
    }

    state->mRequiredMessageTypes = requiredMessageType;

    state->mRequiredRpcMethods.clear();
    state->mRequiredRpcMethods.push_back(requiredRpcMethod);
    state->mIsWaitingForRpcResponse = true;
    state->mMustBeAwakenedOnRpcResponse = true;

    return const_pointer_cast<const TransactionState>(state);
}

TransactionState::SharedConst TransactionState::continueWithPreviousState()
{
    TransactionState::Shared state;
    state = make_shared<TransactionState>(true);
    return const_pointer_cast<const TransactionState>(state);
}

const GEOEpochTimestamp TransactionState::awakeningTimestamp() const
{
    return mAwakeningTimestamp;
}

const vector<Message::MessageType>& TransactionState::acceptedMessagesTypes() const
{
    return mRequiredMessageTypes;
}

const vector<BaseResource::ResourceType> &TransactionState::acceptedResourcesTypes() const
{
    return mRequiredResourcesTypes;
}

const bool TransactionState::needSerialize() const
{
    return mFlushToPermanentStorage;
}

const bool TransactionState::mustBeRescheduled() const
{
    return
        (mAwakeningTimestamp != numeric_limits<GEOEpochTimestamp>::max()) ||
        (!acceptedMessagesTypes().empty()) ||
        mIsWaitingForRpcResponse;
}

const bool TransactionState::mustExit() const
{
    return !mustBeRescheduled();
}

const bool TransactionState::mustBeAwakenedOnMessage() const
{
    return mMustBeAwakenedOnMessage;
}

/*!
 * Indicates whether transaction is currently in RPC wait mode.
 */
const bool TransactionState::isWaitingForRpcResponse() const
{
    return mIsWaitingForRpcResponse;
}

/*!
 * Exposes the list of RPC methods the transaction expects.
 */
const vector<RpcMethod>& TransactionState::requiredRpcMethods() const
{
    return mRequiredRpcMethods;
}

/*!
 * Convenience accessor for the primary expected RPC method.
 */
const RpcMethod TransactionState::requiredRpcMethod() const
{
    if (mRequiredRpcMethods.empty()) {
        return RpcMethod::Unknown;
    }
    return mRequiredRpcMethods.front();
}

/*!
 * Shows if the scheduler should awaken the transaction on RPC delivery.
 */
const bool TransactionState::mustBeAwakenedOnRpcResponse() const
{
    return mMustBeAwakenedOnRpcResponse;
}

const bool TransactionState::mustSavePreviousStateState() const
{
    return mMustSavePreviousStateState;
}
