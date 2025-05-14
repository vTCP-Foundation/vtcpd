cmake_minimum_required(VERSION 3.14)

add_library(transactions__channels
    InitChannelTransaction.h
    InitChannelTransaction.cpp
    ConfirmChannelTransaction.h
    ConfirmChannelTransaction.cpp
    GetContractorListTransaction.h
    GetContractorListTransaction.cpp
    GetChannelInfoTransaction.h
    GetChannelInfoTransaction.cpp
    GetChannelInfoByAddressesTransaction.h
    GetChannelInfoByAddressesTransaction.cpp
    UpdateChannelAddressesInitiatorTransaction.h
    UpdateChannelAddressesInitiatorTransaction.cpp
    UpdateChannelAddressesTargetTransaction.h
    UpdateChannelAddressesTargetTransaction.cpp
    SetChannelContractorAddressesTransaction.h
    SetChannelContractorAddressesTransaction.cpp
    SetChannelContractorCryptoKeyTransaction.h
    SetChannelContractorCryptoKeyTransaction.cpp
    RegenerateChannelCryptoKeyTransaction.h
    RegenerateChannelCryptoKeyTransaction.cpp

    RemoveChannelTransaction.h
    RemoveChannelTransaction.cpp)

target_include_directories(transactions__channels
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__channels
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__channels
    PUBLIC
        transactions__base
        common
        exceptions)
