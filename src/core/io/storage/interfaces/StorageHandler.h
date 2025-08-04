#ifndef VTCPD_INTERFACES_STORAGEHANDLER_H
#define VTCPD_INTERFACES_STORAGEHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "IOTransaction.h"

#include <memory>
#include <string>

using namespace std;

class StorageHandler
{
public:
    virtual ~StorageHandler() = default;

    virtual IOTransaction::Shared beginTransaction() = 0;
    virtual void vacuum() = 0;
};

#endif //VTCPD_INTERFACES_STORAGEHANDLER_H 