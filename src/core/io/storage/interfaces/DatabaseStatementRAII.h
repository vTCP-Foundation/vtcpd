#ifndef VTCPD_INTERFACES_DATABASESTATEMENTRAII_H
#define VTCPD_INTERFACES_DATABASESTATEMENTRAII_H

#include <memory>

using namespace std;

class DatabaseStatementRAII
{
public:
    virtual ~DatabaseStatementRAII() = default;

    virtual void* get() const noexcept = 0;
    virtual bool is_valid() const noexcept = 0;
    virtual void* release() noexcept = 0;
    virtual int reset() noexcept = 0;
};

#endif //VTCPD_INTERFACES_DATABASESTATEMENTRAII_H 