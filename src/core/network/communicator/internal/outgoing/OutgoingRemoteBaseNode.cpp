#include "OutgoingRemoteBaseNode.h"
#include <iostream> // For std::cout, replace with your logger if available and appropriate

OutgoingRemoteBaseNode::OutgoingRemoteBaseNode(
    UDPSocket &socket,
    IOCtx &ioCtx,
    IPv4WithPortAddress::Shared remoteAddress,
    Logger &logger) :

    mIOCtx(ioCtx),
    mSocket(socket),
    mLog(logger),
    mRemoteAddress(remoteAddress),
    mPacketsQueue(),
    mNextAvailableChannelIndex(0),
    mCyclesStats(std::chrono::steady_clock::now(), 0),
    mSendingDelayTimer(mIOCtx)
{
}

OutgoingRemoteBaseNode::~OutgoingRemoteBaseNode()
{
    // Clean up any remaining packets in the queue to prevent memory leaks
    while (!mPacketsQueue.empty()) {
        const auto packetDataAndSize = mPacketsQueue.front();
        free(packetDataAndSize.first);  // Revert to free for consistency
        mPacketsQueue.pop();
    }
}

PacketHeader::ChannelIndex OutgoingRemoteBaseNode::nextChannelIndex() noexcept
{
    // Integer overflow is normal here.
    return mNextAvailableChannelIndex++;
}

void OutgoingRemoteBaseNode::sendMessage(
    pair<BytesShared, size_t> bytesAndBytesCount) noexcept
{
    try {
        // In case if queue already contains packets -
        // then async handler is already scheduled.
        // Otherwise - it must be initialised.
        bool packetsSendingAlreadyScheduled = !mPacketsQueue.empty();

        if (bytesAndBytesCount.second > Message::maxSize()) {
            errors() << "Message is too big to be transferred via the network";
            return;
        }

#ifdef DEBUG_LOG_NETWORK_COMMUNICATOR
        debug() << "Message postponed for the sending";
#endif

        populateQueueWithNewPackets(
            bytesAndBytesCount.first.get(),
            bytesAndBytesCount.second);

        if (not packetsSendingAlreadyScheduled) {
            beginPacketsSending();
        }
    } catch (exception &e) {
        errors()
                << "Exception occurred: "
                << e.what();
    }
}

bool OutgoingRemoteBaseNode::containsPacketsInQueue() const
{
    return !mPacketsQueue.empty();
}

uint32_t OutgoingRemoteBaseNode::crc32Checksum(
    byte_t* data,
    size_t bytesCount) const
noexcept
{
    boost::crc_32_type result;
    result.process_bytes(data, bytesCount);
    return result.checksum();
}

void OutgoingRemoteBaseNode::populateQueueWithNewPackets(
    byte_t* messageData,
    const size_t messageBytesCount)
{
    const auto kMessageContentWithCRC32BytesCount = messageBytesCount + sizeof(uint32_t);

    PacketHeader::TotalPacketsCount kTotalPacketsCount = 0;
    for (size_t bytesLeft = kMessageContentWithCRC32BytesCount; bytesLeft; ++kTotalPacketsCount) {
        bytesLeft -= std::min(bytesLeft, Packet::kMaxSize - PacketHeader::kSize);
    }

    PacketHeader::ChannelIndex channelIndex = nextChannelIndex();
    uint32_t crcChecksum = crc32Checksum(
                               messageData,
                               messageBytesCount);

    size_t messageContentBytesProcessed = 0;
    size_t messageCrc32ChecksumBytesProcessed = 0;
    Packet::Index packetIndex = 0;

    if (kTotalPacketsCount > 1) {
        for (; packetIndex < kTotalPacketsCount - 1; ++packetIndex) {

            // ToDo: remove all previously created packets from the queue to prevent memory leak.
            // Zero-initialize to avoid leaking uninitialized bytes over the network
            auto buffer = static_cast<byte_t*>(calloc(1, Packet::kMaxSize));
            if (buffer == nullptr) {
                // Memory error occurred.
                // Current packet can't be enqueued, so there is no reason to try to enqueue the rest packets.
                // Already created packets would be removed from the queue on the cleaning stage.
                throw bad_alloc();
            }

            memcpy(
                buffer,
                &Packet::kMaxSize,
                sizeof(Packet::kMaxSize));

            memcpy(
                buffer + PacketHeader::kChannelIndexOffset,
                &channelIndex,
                sizeof(channelIndex));

            memcpy(
                buffer + PacketHeader::kPacketsCountOffset,
                &kTotalPacketsCount,
                sizeof(kTotalPacketsCount));

            memcpy(
                buffer + PacketHeader::kPacketIndexOffset,
                &packetIndex,
                sizeof(packetIndex));

            size_t usefulBytesCount = Packet::kMaxSize - PacketHeader::kSize;
            size_t messageLeftover = std::min(usefulBytesCount,
                                              messageBytesCount - messageContentBytesProcessed);

            memcpy(
                buffer + PacketHeader::kDataOffset,
                messageData + messageContentBytesProcessed,
                messageLeftover);

            messageContentBytesProcessed += messageLeftover;

            size_t bytesLeftover = usefulBytesCount - messageLeftover;
            size_t checksumPartial = std::min(bytesLeftover, (size_t)sizeof(crcChecksum));

            memcpy(
                buffer + PacketHeader::kDataOffset + messageLeftover,
                &crcChecksum,
                checksumPartial);
            messageCrc32ChecksumBytesProcessed += checksumPartial;

            // Any remaining bytes (if checksum doesn't fill the entire space) stay zeroed by calloc
            const auto packetMaxSize = Packet::kMaxSize;
            mPacketsQueue.push(
                make_pair(
                    buffer,
                    packetMaxSize));
        }
    }

    // Writing last packet
    // Remaining payload is message tail plus CRC bytes not yet written.
    const size_t remainingMessageBytes = messageBytesCount - messageContentBytesProcessed;
    const size_t remainingCrcBytes = sizeof(crcChecksum) - messageCrc32ChecksumBytesProcessed;
    const size_t remainingPayloadBytes = remainingMessageBytes + remainingCrcBytes;
    const PacketHeader::PacketSize kLastPacketSize =
        static_cast<PacketHeader::PacketSize>(remainingPayloadBytes + PacketHeader::kSize);

    // Round up to next 8-byte boundary to prevent alignment issues
    size_t alignedSize = (kLastPacketSize + 7) & ~7;

    // Zero-initialize to avoid any tail bytes being sent uninitialized
    byte_t* buffer = static_cast<byte_t*>(calloc(1, alignedSize));
    if (buffer == nullptr) {
        throw bad_alloc();
    }

    memcpy(
        buffer,
        &kLastPacketSize,
        sizeof(kLastPacketSize));

    memcpy(
        buffer + PacketHeader::kChannelIndexOffset,
        &channelIndex,
        sizeof(channelIndex));

    memcpy(
        buffer + PacketHeader::kPacketsCountOffset,
        &kTotalPacketsCount,
        sizeof(kTotalPacketsCount));

    memcpy(
        buffer + PacketHeader::kPacketIndexOffset,
        &packetIndex,
        sizeof(packetIndex));

    size_t usefulBytesCount = kLastPacketSize - PacketHeader::kSize;
    size_t messageLeftover = std::min(usefulBytesCount, remainingMessageBytes);

    memcpy(
        buffer + PacketHeader::kDataOffset,
        messageData + messageContentBytesProcessed,
        messageLeftover);

    messageContentBytesProcessed += messageLeftover;

    // Copying CRC32 checksum
    auto checksumLeftover = (size_t)(sizeof(crcChecksum) - messageCrc32ChecksumBytesProcessed);

    memcpy(
        buffer + PacketHeader::kDataOffset + messageLeftover,
        (uint8_t*)&crcChecksum + messageCrc32ChecksumBytesProcessed,
        checksumLeftover);

    mPacketsQueue.push(
        make_pair(
            buffer,
            kLastPacketSize));
}

void OutgoingRemoteBaseNode::beginPacketsSending()
{
    if (mPacketsQueue.empty()) {
        return;
    }

    UDPEndpoint endpoint;
    try {
        endpoint = as::ip::udp::endpoint(
                       boost::asio::ip::make_address_v4(
                           mRemoteAddress->host()),
                       mRemoteAddress->port());
        debug() << "Endpoint address " << endpoint.address().to_string();
        debug() << "Endpoint port " << endpoint.port();
    } catch (exception &) {
        errors()
                << "Endpoint can't be fetched from Contractor. "
                << "No messages can be sent. Outgoing queue cleared.";

        while (!mPacketsQueue.empty()) {
            const auto packetDataAndSize = mPacketsQueue.front();
            free(packetDataAndSize.first);
            mPacketsQueue.pop();
        }

        return;
    }

    // The next code inserts delay between sending packets in case of high traffic.
    const auto kShortSendingTimeInterval = std::chrono::milliseconds(20);
    const auto kTimeoutFromLastSending = std::chrono::steady_clock::now() - mCyclesStats.first;
    if (kTimeoutFromLastSending < kShortSendingTimeInterval) {
        // Increasing short sendings counter.
        mCyclesStats.second += 1;

        const auto kMaxShortSendings = 30;
        if (mCyclesStats.second > kMaxShortSendings) {
            mCyclesStats.second = 0;
            mSendingDelayTimer.expires_after(kShortSendingTimeInterval);
            mSendingDelayTimer.async_wait([this](const boost::system::error_code &_) {
                this->beginPacketsSending();
                debug() << "Sending delayed";
            });
            return;
        }
    } else {
        mCyclesStats.second = 0;
    }

    const auto packetDataAndSize = mPacketsQueue.front();
    mSocket.async_send_to(
        boost::asio::buffer(
            packetDataAndSize.first,
            packetDataAndSize.second),
        endpoint,
    [this, endpoint](const boost::system::error_code &error, const size_t bytesTransferred) {
        const auto packetDataAndSize = mPacketsQueue.front();
        if (bytesTransferred != packetDataAndSize.second) {
            if (error) {
                errors() << "beginPacketsSending: "
                         << "Next packet can't be sent to the node (" << mRemoteAddress->fullAddress() << "). "
                         << "Error code: " << error.value();
            }

            // Removing packet from the memory
            free(packetDataAndSize.first);  // Revert to free for consistency
            mPacketsQueue.pop();
            if (!mPacketsQueue.empty()) {
                beginPacketsSending();
            }

            return;
        }

#ifdef DEBUG_LOG_NETWORK_COMMUNICATOR
        const PacketHeader::ChannelIndex channelIndex =
            *(reinterpret_cast<PacketHeader::ChannelIndex*>(packetDataAndSize.first + PacketHeader::kChannelIndexOffset));

        const PacketHeader::PacketIndex packetIndex =
            *(reinterpret_cast<PacketHeader::PacketIndex*>(packetDataAndSize.first + PacketHeader::kPacketIndexOffset)) + 1;

        const PacketHeader::TotalPacketsCount totalPacketsCount =
            *(reinterpret_cast<PacketHeader::TotalPacketsCount*>(packetDataAndSize.first + PacketHeader::kPacketsCountOffset));

        this->debug() << setw(4) << bytesTransferred << "B TX [ => ] "
                      << endpoint.address() << ":" << endpoint.port() << "; "
                      << "Channel: " << setw(10) << static_cast<size_t>(channelIndex) << "; "
                      << "Packet: " << setw(3) << static_cast<size_t>(packetIndex)
                      << "/" << static_cast<size_t>(totalPacketsCount);
#endif

        // Removing packet from the memory
        free(packetDataAndSize.first);  // Revert to free for consistency
        mPacketsQueue.pop();

        mCyclesStats.first = std::chrono::steady_clock::now();
        if (!mPacketsQueue.empty()) {
            beginPacketsSending();
        }
    });
}

LoggerStream OutgoingRemoteBaseNode::errors() const
{
    return mLog.warning(
               string("Communicator / OutgoingRemoteNode [") + mRemoteAddress->fullAddress() + string("]"));
}

LoggerStream OutgoingRemoteBaseNode::debug() const
{
#ifdef DEBUG_LOG_NETWORK_COMMUNICATOR
    return mLog.debug(
               string("Communicator / OutgoingRemoteNode [") + mRemoteAddress->fullAddress() + string("]"));
#endif

#ifndef DEBUG_LOG_NETWORK_COMMUNICATOR
    return LoggerStream::dummy();
#endif
}
