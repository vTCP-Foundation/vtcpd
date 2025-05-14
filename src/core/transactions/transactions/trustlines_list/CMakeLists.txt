cmake_minimum_required(VERSION 3.14)

add_library(transactions__trustlines_list
    GetFirstLevelContractorsTransaction.h
    GetFirstLevelContractorsTransaction.cpp
    GetTrustLinesListTransaction.h
    GetTrustLinesListTransaction.cpp
    GetTrustLineByIDTransaction.h
    GetTrustLineByIDTransaction.cpp
    GetTrustLineByAddressTransaction.h
    GetTrustLineByAddressTransaction.cpp
    GetEquivalentListTransaction.h
    GetEquivalentListTransaction.cpp
	GetAllTrustLineListTransaction.h
	GetAllTrustLineListTransaction.cpp)

target_include_directories(transactions__trustlines_list
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__trustlines_list
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__trustlines_list
    PUBLIC
        transactions__base)