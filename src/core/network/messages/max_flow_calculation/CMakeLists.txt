cmake_minimum_required(VERSION 3.14)

add_library(messages__max_flow_calculation
    InitiateMaxFlowCalculationMessage.h
    InitiateMaxFlowCalculationMessage.cpp
    MaxFlowCalculationSourceFstLevelMessage.h
    MaxFlowCalculationSourceFstLevelMessage.cpp
    MaxFlowCalculationSourceSndLevelMessage.h
    MaxFlowCalculationSourceSndLevelMessage.cpp
    MaxFlowCalculationTargetFstLevelMessage.h
    MaxFlowCalculationTargetFstLevelMessage.cpp
    MaxFlowCalculationTargetSndLevelMessage.h
    MaxFlowCalculationTargetSndLevelMessage.cpp
    ResultMaxFlowCalculationMessage.h
    ResultMaxFlowCalculationMessage.cpp
    ResultMaxFlowCalculationGatewayMessage.h
    ResultMaxFlowCalculationGatewayMessage.cpp
    ExchangeRatesMessage.h
    ExchangeRatesMessage.cpp
    InitiateMaxFlowForExchangeCalculationMessage.h
    InitiateMaxFlowForExchangeCalculationMessage.cpp)

target_include_directories(messages__max_flow_calculation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(messages__max_flow_calculation
    PRIVATE
        cxx_std_20
)

target_link_libraries(messages__max_flow_calculation
    PUBLIC
        messages__base__max_flow_calculation)