#include <progdn_core/log.h>

namespace progdn
{
    const char* Log::Writer::to_string(Level level) noexcept
    {
        switch (level)
        {
        case Level::Debug:
            return "DBG";
        case Level::Info:
            return "INF";
        case Level::Warning:
            return "WRN";
        case Level::Error:
            return "ERR";
        default:
            return "???";
        };
    }

    void Log::debug(const std::string& messageText) noexcept
    {
        try_write(Level::Debug, messageText);
    }

    void Log::info(const std::string& messageText) noexcept
    {
        try_write(Level::Info, messageText);
    }

    void Log::warning(const std::string& messageText) noexcept
    {
        try_write(Level::Warning, messageText);
    }

    void Log::error(const std::string& messageText) noexcept
    {
        try_write(Level::Error, messageText);
    }

    void Log::try_write(Level level, const std::string& text) noexcept
    {
        try
        {
            auto& this_object = Log::get_instance_or_null();
            if (this_object) {
                for (auto& writer : this_object->m_writers)
                    writer->write(level, text);
            }
        }
        catch (...) { }
    }
}
