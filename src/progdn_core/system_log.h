#pragma once

#include <progdn_core/log.h>

namespace progdn
{
    class SystemLog : public Log::Writer
    {
    public:
        SystemLog(const char* identifier);
        virtual ~SystemLog();

    private:
        virtual void write(Level messageLevel, const std::string& messageText) override;
        static int to_priority(Level level) noexcept;
    };
}
