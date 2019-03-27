#pragma once

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace progdn
{
    class IniFile 
    {
    public:
        static boost::property_tree::ptree parse(const boost::filesystem::path& filepath)
        {
            try {
                boost::property_tree::ptree pt;
                boost::iostreams::file_descriptor_source config_fds(filepath);
                boost::iostreams::stream<boost::iostreams::file_descriptor_source> config_fs(config_fds);
                boost::property_tree::ini_parser::read_ini(config_fs, pt);
                return pt;
            } catch (const std::exception& e) {
                throw std::runtime_error("Cannot parse file " + filepath.string() + ": " + e.what());
            }
        }
    };
}
