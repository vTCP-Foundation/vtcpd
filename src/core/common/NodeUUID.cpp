#include "NodeUUID.h"
#include <algorithm>  // for std::copy

NodeUUID::NodeUUID()
    : uuid(boost::uuids::random_generator()())
{
}

NodeUUID::NodeUUID(uuid const &u)
    : boost::uuids::uuid(u)
{
}

NodeUUID::NodeUUID(const string &hex)
    : uuid(boost::lexical_cast<uuid>(hex))
{
}

NodeUUID::NodeUUID(const uint8_t* bytes)
{
    std::copy(bytes, bytes + kBytesSize, begin());
}

const string NodeUUID::stringUUID() const
{
    return boost::lexical_cast<string>(*this);
}

const NodeUUID& NodeUUID::empty()
{
    static const NodeUUID kEmpty("00000000-0000-0000-0000-000000000000");
    return kEmpty;
}