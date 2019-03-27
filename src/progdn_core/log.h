#pragma once

#include <progdn_core/singleton.h>

#include <boost/format.hpp>

#include <memory>
#include <string>
#include <vector>

namespace progdn
{
    class Log : public Singleton<Log>
    {
    public:
        enum Level { Error, Warning, Info, Debug };

    public:
        class Writer
        {
        public:
            using Level = Log::Level;

        public:
            virtual ~Writer() = default;

        public:
            virtual void write(Level level, const std::string& text) = 0;
            static const char* to_string(Level level) noexcept;
        };

    private:
        std::vector<std::unique_ptr<Writer>> m_writers;

    public:
        static void debug(const std::string& messageText) noexcept;
        static void info(const std::string& messageText) noexcept;
        static void warning(const std::string& messageText) noexcept;
        static void error(const std::string& messageText) noexcept;

        static bool is_enabled() noexcept {
            return is_instance_created();
        }

    public:
        template<typename WriterT, typename... Arguments>
        void emplace_writer(Arguments... arguments) {
            m_writers.emplace_back(new WriterT(std::forward<Arguments>(arguments)...));
        }

        template<typename T>
        static void debug(const boost::basic_format<T>& messageText) noexcept {
            try { debug(messageText.str()); } catch(...) {}
        }

        template<typename T>
        static void info(const boost::basic_format<T>& messageText) noexcept {
            try { info(messageText.str()); } catch(...) {}
        }

        template<typename T>
        static void warning(const boost::basic_format<T>& messageText) noexcept {
            try { warning(messageText.str()); } catch(...) {}
        }

        template<typename T>
        static void error(const boost::basic_format<T>& messageText) noexcept {
            try { error(messageText.str()); } catch(...) {}
        }

    private:
        static void try_write(Level level, const std::string& text) noexcept;
    };
}
