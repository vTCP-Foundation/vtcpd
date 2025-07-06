#ifndef VTCPD_INTERFACES_COMMUNICATORIOTRANSACTION_H
#define VTCPD_INTERFACES_COMMUNICATORIOTRANSACTION_H

#include "../../../common/Types.h"
#include "CommunicatorMessagesQueueHandler.h"

#include <memory>

using namespace std;

class CommunicatorIOTransaction
{
public:
    typedef shared_ptr<CommunicatorIOTransaction> Shared;
    typedef unique_ptr<CommunicatorIOTransaction> Unique;

    virtual ~CommunicatorIOTransaction() = default;

    virtual CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler() = 0;
    virtual void rollback() = 0;
};

#endif //VTCPD_INTERFACES_COMMUNICATORIOTRANSACTION_H 