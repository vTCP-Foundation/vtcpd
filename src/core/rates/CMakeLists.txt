cmake_minimum_required(VERSION 3.14)

add_library(rates
    ExchangeRate.cpp
    ExchangeRate.h
    manager/ExchangeRatesManager.cpp
    manager/ExchangeRatesManager.h)

target_include_directories(rates
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(rates
    PRIVATE
        cxx_std_20
)

target_link_libraries(rates
    PUBLIC
        common
        logger
        ${Boost_SYSTEM_LIBRARIES}
        ${Boost_THREAD_LIBRARIES}
        ${Boost_DATE_TIME_LIBRARIES}
)