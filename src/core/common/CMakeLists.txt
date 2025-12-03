cmake_minimum_required(VERSION 3.14)

add_library(common
    Types.h 
    Constraints.h
    NodeUUID.cpp
    NodeUUID.h
    serialization/BytesDeserializer.cpp 
    serialization/BytesDeserializer.h 
    serialization/BytesSerializer.cpp 
    serialization/BytesSerializer.h 
    time/TimeUtils.h
    multiprecision/MultiprecisionUtils.h
    multiprecision/ExchangeRateMath.cpp
    multiprecision/ExchangeRateMath.h
    memory/MemoryUtils.h)

target_include_directories(common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(common
    PRIVATE
        cxx_std_20
)
