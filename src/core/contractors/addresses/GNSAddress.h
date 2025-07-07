#ifndef VTCPD_GNSADDRESS_H
#define VTCPD_GNSADDRESS_H

#include "BaseAddress.h"
#include "../../common/Constraints.h"
#include "../../network/communicator/internal/common/Types.h"
#include "../../common/exceptions/ValueError.h"

#include <string>

class GNSAddress : public BaseAddress
{

public:
    typedef shared_ptr<GNSAddress> Shared;

public:
    GNSAddress(
        const string &fullAddress);

    GNSAddress(
        byte_t* buffer);

    // Overload accepting const pointer
    GNSAddress(
        const byte_t* buffer) : GNSAddress(const_cast<byte_t*>(buffer)) {}

    const string host() const override;

    const Port port() const override;

    const string fullAddress() const override;

    const AddressType typeID() const override;

    BytesShared serializeToBytes() const override;

    size_t serializedSize() const override;

    void setIPAndPort(
        const string &providerData);

    const string name() const;

    const string provider() const;

private:
    string mProvider;
    string mName;
    string mHost;
    Port mPort;
};

#endif // VTCPD_GNSADDRESS_H
