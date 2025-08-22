cmake_minimum_required(VERSION 3.14)

# Add all transaction subdirectories
add_subdirectory(transactions/trust_lines)
add_subdirectory(transactions/regular/payments)
add_subdirectory(transactions/result)
add_subdirectory(transactions/base)
add_subdirectory(transactions/cycles)
add_subdirectory(transactions/max_flow_calculation)
add_subdirectory(transactions/total_balances)
add_subdirectory(transactions/history)
add_subdirectory(transactions/find_path)
add_subdirectory(transactions/trustlines_list)
add_subdirectory(transactions/transaction)
add_subdirectory(transactions/gateway_notification)
add_subdirectory(transactions/general)
add_subdirectory(transactions/trust_line_channel)
add_subdirectory(transactions/rates)

add_library(transactions
    manager/TransactionsManager.h
    manager/TransactionsManager.cpp
    scheduler/TransactionsScheduler.h
    scheduler/TransactionsScheduler.cpp
    transactions/base/TransactionUUID.h)

target_include_directories(transactions
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions
    PUBLIC
        transactions__trustlines
        transactions__max_flow_calculation
        transactions__payments
        transactions__cycles
        transactions__result
        transactions__total_balances
        transactions__history
        transactions__trustlines_list
        transactions__find_path
        transactions__transaction
        transactions__gateway_notification
        transactions__general
        transactions__channels
        transactions__rates
        core_libs
        common
        exceptions
        equivalents)
