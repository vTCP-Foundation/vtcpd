cmake_minimum_required(VERSION 3.14)

find_package(Boost REQUIRED COMPONENTS system)

add_library(network__communicator
    Communicator.cpp
    Communicator.h
    internal/common/Types.h
    internal/common/Packet.hpp
    internal/incoming/IncomingMessagesHandler.h
    internal/incoming/IncomingMessagesHandler.cpp
    internal/incoming/IncomingRemoteNode.cpp
    internal/incoming/IncomingRemoteNode.h
    internal/incoming/IncomingNodesHandler.cpp
    internal/incoming/IncomingNodesHandler.h
    internal/incoming/IncomingChannel.cpp
    internal/incoming/IncomingChannel.h
    internal/incoming/MessageParser.h
    internal/incoming/MessageParser.cpp
    internal/outgoing/OutgoingRemoteBaseNode.h
    internal/outgoing/OutgoingRemoteBaseNode.cpp
    internal/outgoing/OutgoingMessagesHandler.h
    internal/outgoing/OutgoingMessagesHandler.cpp
    internal/outgoing/OutgoingNodesHandler.h
    internal/outgoing/OutgoingNodesHandler.cpp
    internal/queue/ConfirmationRequiredMessagesQueue.h
    internal/queue/ConfirmationRequiredMessagesQueue.cpp
    internal/queue/ConfirmationRequiredMessagesHandler.h
    internal/queue/ConfirmationRequiredMessagesHandler.cpp
    internal/queue/ConfirmationNotStronglyRequiredMessagesHandler.h
    internal/queue/ConfirmationNotStronglyRequiredMessagesHandler.cpp
    internal/queue/ConfirmationNotStronglyRequiredMessagesQueue.h
    internal/queue/ConfirmationNotStronglyRequiredMessagesQueue.cpp
    internal/queue/ConfirmationResponseMessagesHandler.h
    internal/queue/ConfirmationResponseMessagesHandler.cpp
    internal/queue/ConfirmationCachedResponseMessage.h
    internal/queue/ConfirmationCachedResponseMessage.cpp
    internal/queue/PingMessagesHandler.h
    internal/queue/PingMessagesHandler.cpp
    internal/incoming/TailManager.h
    internal/incoming/TailManager.cpp)

target_include_directories(network__communicator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(network__communicator
    PRIVATE
        cxx_std_20
)

target_link_libraries(network__communicator
    PUBLIC
        Boost::system
        common
        exceptions
        logger
        network__messages
        settings
        io__storage)
