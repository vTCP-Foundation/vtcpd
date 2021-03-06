cmake_minimum_required(VERSION 3.6)

set(SOURCE_FILES

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
    ResetTrustLineDestinationTransaction.cpp )

add_library(transactions__trustlines ${SOURCE_FILES})
target_link_libraries(transactions__trustlines
    transactions__base
    common
    exceptions)
