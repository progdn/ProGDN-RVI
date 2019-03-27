#include "command_line_interface.h"

#include <boost/format.hpp>

#include <iostream>

namespace progdn
{
    CommandLineInterface::CommandLineInterface(int argc, char* argv[]) :
        m_description(
            "ProGDN Real Visitor Info Server\n\n"
            "Allowed options")
    {
        auto conf_help = (boost::format("Path to configuration file. Default: %1%") % conf).str();
        m_description.add_options()
            (kOption_Help,
             "Produce help message and exit")
            (kOption_Version,
             "Print version and exit")
            (kOption_Verbose,
             "Enable logging to syslog")
            (kOption_Conf,
             boost::program_options::value(&conf),
             conf_help.c_str())
            (kOption_Background,
             "Run in background")
        ;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, m_description), m_vars_map);
        boost::program_options::notify(m_vars_map);

        if (is_option_specified(kOption_Help))
        {
            std::cout << m_description << std::endl;
            std::exit(0);
        }

        if (is_option_specified(kOption_Version)) {
            std::cout << PROGDN_RVI_VERSION << std::endl;
            std::exit(0);
        }
    }

    bool CommandLineInterface::is_option_specified(const char* name) const
    {
        return (m_vars_map.count(name) > 0);
    }
}
