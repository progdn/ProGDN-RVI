#include <progdn_core/system_log.h>

#include <syslog.h>

#include <sstream>

namespace progdn
{
    SystemLog::SystemLog(const char* identifier)
    {
        ::openlog(identifier, LOG_CONS | LOG_PID | LOG_NDELAY | LOG_PERROR, 0);
    }

    SystemLog::~SystemLog()
    {
        ::closelog();
    }

    void SystemLog::write(Level messageLevel, const std::string& messageText)
    {
        auto priority = to_priority(messageLevel);
        ::syslog(priority, "%s", messageText.c_str());
    }

    int SystemLog::to_priority(Level level) noexcept
    {
        switch (level)
        {
        case Level::Info:
            return LOG_INFO;
        case Level::Warning:
            return LOG_WARNING;
        case Level::Error:
            return LOG_ERR;
        case Level::Debug:
            return LOG_DEBUG;
        };
        return 0;
    }
}
