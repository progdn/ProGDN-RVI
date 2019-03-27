#include <progdn_core/system_limits.h>

#include <sys/resource.h>

#include <cstring>
#include <stdexcept>
#include <string>

namespace progdn
{
    void SystemLimits::unlimit_open_files_number()
    {
        rlimit limit;
        if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) {
            auto error = errno;
            throw std::runtime_error("Cannot get RLIMIT_NOFILE, error " + std::to_string(error) + " (" + strerror(error) + ')');
        }
        limit.rlim_cur = limit.rlim_max;
        if (::setrlimit(RLIMIT_NOFILE, &limit) != 0) {
            auto error = errno;
            throw std::runtime_error("Cannot set RLIMIT_NOFILE, error " + std::to_string(error) + " (" + strerror(error) + ')');
        }
    }
}
