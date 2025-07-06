#ifndef VTCPD_INTERFACES_COMMUNICATORSTORAGEHANDLER_H
#define VTCPD_INTERFACES_COMMUNICATORSTORAGEHANDLER_H

#include "../../../logger/Logger.h"
#include "CommunicatorIOTransaction.h"
#include "../../../common/exceptions/IOError.h"

#include <memory>
#include <string>

using namespace std;

class CommunicatorStorageHandler
{
public:
    virtual ~CommunicatorStorageHandler() = default;

    virtual CommunicatorIOTransaction::Shared beginTransaction() = 0;
    virtual CommunicatorIOTransaction::Unique beginTransactionUnique() = 0;
};

#endif //VTCPD_INTERFACES_COMMUNICATORSTORAGEHANDLER_H 