cmake_minimum_required(VERSION 3.14)

add_library(crypto
    memory.h
    memory.cpp
    keychain.h
    keychain.cpp
    sphincskeys.cpp
    sphincskeys.h
    sphincsscheme.cpp
    sphincsscheme.h
    ByteEncryptor.cpp
    ByteEncryptor.h
    MsgEncryptor.cpp
    MsgEncryptor.h
    ProviderMsgEncryptor.h
    ProviderMsgEncryptor.cpp)

target_include_directories(crypto
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(crypto
    PRIVATE
        cxx_std_20
)

# Find OpenSSL package (required for SPHINCS+ support)
# SPHINCS+ SLH-DSA-SHA2-256s is available in OpenSSL 3.5+
set(CMAKE_PREFIX_PATH "/usr/local/openssl" ${CMAKE_PREFIX_PATH})
find_package(OpenSSL 3.5 REQUIRED)

# Verify SPHINCS+ availability at configure time
if(NOT OPENSSL_FOUND OR OPENSSL_VERSION VERSION_LESS "3.5")
    message(FATAL_ERROR "OpenSSL 3.5+ is required for SPHINCS+ SLH-DSA-SHA2-256s support. Found version: ${OPENSSL_VERSION}")
endif()

# Find sodium package if available, otherwise fall back to direct linking
find_package(sodium QUIET)
if(sodium_FOUND)
    target_link_libraries(crypto
        PRIVATE
            sodium::sodium
            OpenSSL::Crypto
    )
else()
    target_link_libraries(crypto
        PRIVATE
            sodium
            OpenSSL::Crypto
    )
endif()



