#pragma once

#include <boost/asio.hpp>

namespace progdn
{
    struct IP_Host
    {
        uint32_t host;

        IP_Host() = default;

        IP_Host(const char* serialized) {
            struct in_addr addr;
            if (!::inet_pton(AF_INET, serialized, &addr))
                throw std::invalid_argument(std::string("\"") + serialized + "\" is not a valid IP");
            host = boost::asio::detail::socket_ops::network_to_host_long(addr.s_addr);
        }
    };
}
