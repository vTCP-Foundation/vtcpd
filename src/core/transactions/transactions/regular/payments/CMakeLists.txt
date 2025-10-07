cmake_minimum_required(VERSION 3.14)

add_library(transactions__payments
    base/BasePaymentTransaction.h
    base/BasePaymentTransaction.cpp
    base/PathStats.h
    base/PathStats.cpp
    base/BaseExchangePaymentTransaction.h
    base/BaseExchangePaymentTransaction.cpp
    CoordinatorPaymentTransaction.h
    CoordinatorPaymentTransaction.cpp
    ReceiverPaymentTransaction.h
    ReceiverPaymentTransaction.cpp
    IntermediateNodePaymentTransaction.h
    IntermediateNodePaymentTransaction.cpp
    CoordinatorExchangePaymentTransaction.h
    CoordinatorExchangePaymentTransaction.cpp
    ReceiverExchangePaymentTransaction.h
    ReceiverExchangePaymentTransaction.cpp
    IntermediateNodeExchangePaymentTransaction.h
    IntermediateNodeExchangePaymentTransaction.cpp
    VotesStatusResponsePaymentTransaction.h
    VotesStatusResponsePaymentTransaction.cpp
    CycleCloserInitiatorTransaction.h
    CycleCloserInitiatorTransaction.cpp
    CycleCloserIntermediateNodeTransaction.h
    CycleCloserIntermediateNodeTransaction.cpp)

target_include_directories(transactions__payments
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__payments
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__payments
    PUBLIC
        core_libs
        transactions__base
        network__messages
        paths)
