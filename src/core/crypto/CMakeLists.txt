cmake_minimum_required(VERSION 3.6)

set(SOURCE_FILES

        memory.h
        memory.cpp

        keychain.h
        keychain.cpp

        lamportkeys.cpp
        lamportkeys.h

        lamportscheme.cpp
        lamportscheme.h

        ByteEncryptor.cpp
        ByteEncryptor.h

        MsgEncryptor.cpp
        MsgEncryptor.h

        ProviderMsgEncryptor.h
        ProviderMsgEncryptor.cpp)

add_library(crypto ${SOURCE_FILES})
target_link_libraries(
        crypto
        -lsodium)



