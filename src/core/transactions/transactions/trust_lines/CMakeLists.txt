cmake_minimum_required(VERSION 3.14)

add_library(transactions__trustlines
    base/BaseTrustLineTransaction.h
    base/BaseTrustLineTransaction.cpp
    SetOutgoingTrustLineTransaction.h
    SetOutgoingTrustLineTransaction.cpp
    CloseIncomingTrustLineTransaction.h
    CloseIncomingTrustLineTransaction.cpp
    OpenTrustLineTransaction.h
    OpenTrustLineTransaction.cpp
    AcceptTrustLineTransaction.h
    AcceptTrustLineTransaction.cpp
    PublicKeysSharingSourceTransaction.h
    PublicKeysSharingSourceTransaction.cpp
    PublicKeysSharingTargetTransaction.h
    PublicKeysSharingTargetTransaction.cpp
    AuditSourceTransaction.h
    AuditSourceTransaction.cpp
    AuditTargetTransaction.h
    AuditTargetTransaction.cpp
    ConflictResolverInitiatorTransaction.h
    ConflictResolverInitiatorTransaction.cpp
    ConflictResolverContractorTransaction.h
    ConflictResolverContractorTransaction.cpp
    CheckTrustLineTransaction.h
    CheckTrustLineTransaction.cpp
    RemoveTrustLineTransaction.h
    RemoveTrustLineTransaction.cpp
    ResetTrustLineSourceTransaction.h
    ResetTrustLineSourceTransaction.cpp
    ResetTrustLineDestinationTransaction.h
    ResetTrustLineDestinationTransaction.cpp)

target_include_directories(transactions__trustlines
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__trustlines
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__trustlines
    PUBLIC
        transactions__base
        common
        exceptions
        features)
