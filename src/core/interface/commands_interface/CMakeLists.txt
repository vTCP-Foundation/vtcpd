﻿cmake_minimum_required(VERSION 3.6)

find_package(Boost COMPONENTS system REQUIRED)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/core/interface/)
set(SOURCE_FILES
        interface/CommandsInterface.cpp
        interface/CommandsInterface.h
        

        commands/BaseUserCommand.h
        commands/BaseUserCommand.cpp

        commands/ErrorUserCommand.h

        commands/trust_line_channels/InitChannelCommand.h
        commands/trust_line_channels/InitChannelCommand.cpp
        commands/trust_line_channels/ContractorListCommand.h
        commands/trust_line_channels/ContractorListCommand.cpp
        commands/trust_line_channels/GetChannelInfoCommand.h
        commands/trust_line_channels/GetChannelInfoCommand.cpp
        commands/trust_line_channels/GetChannelInfoByAddressesCommand.h
        commands/trust_line_channels/GetChannelInfoByAddressesCommand.cpp
        commands/trust_line_channels/SetChannelContractorAddressesCommand.h
        commands/trust_line_channels/SetChannelContractorAddressesCommand.cpp
        commands/trust_line_channels/SetChannelContractorCryptoKeyCommand.h
        commands/trust_line_channels/SetChannelContractorCryptoKeyCommand.cpp
        commands/trust_line_channels/RegenerateChannelCryptoKeyCommand.h
        commands/trust_line_channels/RegenerateChannelCryptoKeyCommand.cpp

        commands/trust_lines/InitTrustLineCommand.h
        commands/trust_lines/InitTrustLineCommand.cpp
        commands/trust_lines/SetOutgoingTrustLineCommand.h
        commands/trust_lines/SetOutgoingTrustLineCommand.cpp
        commands/trust_lines/CloseIncomingTrustLineCommand.h
        commands/trust_lines/CloseIncomingTrustLineCommand.cpp
        commands/trust_lines/ShareKeysCommand.h
        commands/trust_lines/ShareKeysCommand.cpp
        commands/trust_lines/RemoveTrustLineCommand.h
        commands/trust_lines/RemoveTrustLineCommand.cpp
        commands/trust_lines/ResetTrustLineCommand.h
        commands/trust_lines/ResetTrustLineCommand.cpp

        commands/payments/CreditUsageCommand.cpp
        commands/payments/CreditUsageCommand.h

        commands/max_flow_calculation/InitiateMaxFlowCalculationCommand.h
        commands/max_flow_calculation/InitiateMaxFlowCalculationCommand.cpp
        commands/max_flow_calculation/InitiateMaxFlowCalculationFullyCommand.h
        commands/max_flow_calculation/InitiateMaxFlowCalculationFullyCommand.cpp

        commands/total_balances/TotalBalancesCommand.h
        commands/total_balances/TotalBalancesCommand.cpp

        commands/history/HistoryPaymentsCommand.h
        commands/history/HistoryPaymentsCommand.cpp
        commands/history/HistoryPaymentsAllEquivalentsCommand.h
        commands/history/HistoryPaymentsAllEquivalentsCommand.cpp
        commands/history/HistoryTrustLinesCommand.h
        commands/history/HistoryTrustLinesCommand.cpp
        commands/history/HistoryWithContractorCommand.h
        commands/history/HistoryWithContractorCommand.cpp
        commands/history/HistoryAdditionalPaymentsCommand.h
        commands/history/HistoryAdditionalPaymentsCommand.cpp

        commands/trust_lines_list/GetFirstLevelContractorsCommand.h
        commands/trust_lines_list/GetFirstLevelContractorsCommand.cpp
        commands/trust_lines_list/GetTrustLinesCommand.h
        commands/trust_lines_list/GetTrustLinesCommand.cpp
        commands/trust_lines_list/GetTrustLineByAddressCommand.h
        commands/trust_lines_list/GetTrustLineByAddressCommand.cpp
        commands/trust_lines_list/GetTrustLineByIDCommand.h
        commands/trust_lines_list/GetTrustLineByIDCommand.cpp
        commands/trust_lines_list/EquivalentListCommand.h
        commands/trust_lines_list/EquivalentListCommand.cpp

        commands/subsystems_controller/SubsystemsInfluenceCommand.h
        commands/subsystems_controller/SubsystemsInfluenceCommand.cpp
        commands/subsystems_controller/TrustLinesInfluenceCommand.h
        commands/subsystems_controller/TrustLinesInfluenceCommand.cpp

        commands/transactions/PaymentTransactionByCommandUUIDCommand.h
        commands/transactions/PaymentTransactionByCommandUUIDCommand.cpp

        commands/general/RemoveOutdatedCryptoDataCommand.h
        commands/general/RemoveOutdatedCryptoDataCommand.cpp)

add_library(interface__commands ${SOURCE_FILES})
target_link_libraries(interface__commands
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        common)
