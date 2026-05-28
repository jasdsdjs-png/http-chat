#include "ConfigMgr.h"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <iostream>

ConfigMgr& ConfigMgr::Inst() {
    static ConfigMgr cfg_mgr;
    return cfg_mgr;
}

SectionInfo ConfigMgr::operator[](const std::string& section) const {
    auto iter = _config_map.find(section);
    if (iter == _config_map.end()) {
        return SectionInfo();
    }
    return iter->second;
}

ConfigMgr::ConfigMgr() {
    auto config_path = std::filesystem::current_path() / "config.ini";
    std::cout << "Config path: " << config_path << std::endl;

    boost::property_tree::ptree pt;
    boost::property_tree::read_ini(config_path.string(), pt);

    for (const auto& section_pair : pt) {
        SectionInfo section_info;
        for (const auto& key_value_pair : section_pair.second) {
            section_info._section_datas[key_value_pair.first] =
                key_value_pair.second.get_value<std::string>();
        }
        _config_map[section_pair.first] = section_info;
    }
}
