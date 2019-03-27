#pragma once

#include <boost/program_options.hpp>

namespace progdn
{
    class CommandLineInterface
    {
    public:
        static constexpr auto kOption_Help    = "help";
        static constexpr auto kOption_Version = "version";
        static constexpr auto kOption_Verbose = "verbose";
        static constexpr auto kOption_Conf    = "conf";
        static constexpr auto kOption_Background = "background";

    public:
        std::string conf = "progdn-rvi.conf";

    private:
        boost::program_options::options_description m_description;
        boost::program_options::variables_map m_vars_map;

    public:
        CommandLineInterface(int argc, char* argv[]);

    public:
        bool is_option_specified(const char* name) const;
    };
}
