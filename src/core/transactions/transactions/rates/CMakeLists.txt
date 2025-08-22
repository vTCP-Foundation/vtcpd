cmake_minimum_required(VERSION 3.14)

add_library(transactions__rates
    SetExchangeRateTransaction.h
    SetExchangeRateTransaction.cpp
    GetExchangeRateTransaction.h
    GetExchangeRateTransaction.cpp
    ListExchangeRatesTransaction.h
    ListExchangeRatesTransaction.cpp
    RemoveExchangeRateTransaction.h
    RemoveExchangeRateTransaction.cpp
    ClearExchangeRatesTransaction.h
    ClearExchangeRatesTransaction.cpp)

target_include_directories(transactions__rates
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__rates
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__rates
    PUBLIC
        transactions__base)