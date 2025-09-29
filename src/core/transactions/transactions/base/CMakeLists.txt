cmake_minimum_required(VERSION 3.14)

add_library(transactions__base
    TransactionUUID.h
    BaseTransaction.h
    BaseTransaction.cpp
    BaseCollectTopologyTransaction.h
    BaseCollectTopologyTransaction.cpp
    BaseCollectTopologyForExchangeTransaction.h
    BaseCollectTopologyForExchangeTransaction.cpp)

target_include_directories(transactions__base
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__base
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__base
    PUBLIC
        common
        exceptions
        network__messages
        transactions__result)
