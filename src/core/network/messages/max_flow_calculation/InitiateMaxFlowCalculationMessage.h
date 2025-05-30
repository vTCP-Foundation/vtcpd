#ifndef VTCPD_INITIATEMAXFLOWCALCULATIONMESSAGE_H
#define VTCPD_INITIATEMAXFLOWCALCULATIONMESSAGE_H

#include "../SenderMessage.h"

/**
 * @brief Message to initiate max flow calculation between nodes
 */
class InitiateMaxFlowCalculationMessage : public SenderMessage
{
public:
    using Shared = std::shared_ptr<InitiateMaxFlowCalculationMessage>;

    /**
     * @brief Construct a new message
     * @param equivalent Serialized equivalent data
     * @param senderAddresses Vector of sender addresses
     * @param isSenderGateway Whether the sender is a gateway
     * @param hopsCount Number of hops for the calculation
     */
    InitiateMaxFlowCalculationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared>& senderAddresses,
        bool isSenderGateway,
        uint8_t hopsCount);

    /**
     * @brief Construct from serialized bytes
     * @param buffer Serialized message data
     */
    explicit InitiateMaxFlowCalculationMessage(BytesShared buffer);

    /**
     * @return true if sender is gateway, false otherwise
     */
    bool isSenderGateway() const;

    /**
     * @return Number of hops for max flow calculation
     */
    uint8_t getHopsCount() const;

    /**
     * @brief Get the message type identifier
     * @return Message type enum value
     */
    const MessageType typeID() const override;

    /**
     * @brief Serialize message to bytes
     * @return Pair of byte buffer and size
     */
    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    uint8_t mHopsCount;      // Number of hops for calculation
    bool mIsSenderGateway;   // Whether sender is a gateway node
};

#endif //VTCPD_INITIATEMAXFLOWCALCULATIONMESSAGE_H
