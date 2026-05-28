#pragma once
#include <string>
#include <map>

// Represents a section in an INI config file
struct SectionInfo {
    SectionInfo() {}
    ~SectionInfo() {
        _section_datas.clear();
    }
    SectionInfo(const SectionInfo& src) {
        _section_datas = src._section_datas;
    }
    SectionInfo& operator=(const SectionInfo& src) {
        if (&src == this) {
            return *this;
        }
        this->_section_datas = src._section_datas;
        return *this;
    }
    std::map<std::string, std::string> _section_datas;
    std::string operator[](const std::string& key) {
        if (_section_datas.find(key) == _section_datas.end()) {
            return "";
        }
        return _section_datas[key];
    }
};

// Config manager (Meyers singleton)
// Usage: std::string port = ConfigMgr::Inst()["GateServer"]["Port"];
class ConfigMgr
{
public:
    ~ConfigMgr() {
        _config_map.clear();
    }

    static ConfigMgr& Inst() {
        static ConfigMgr cfg_mgr;
        return cfg_mgr;
    }

    SectionInfo operator[](const std::string& section) {
        if (_config_map.find(section) == _config_map.end()) {
            return SectionInfo();
        }
        return _config_map[section];
    }

    ConfigMgr(const ConfigMgr& src) = delete;
    ConfigMgr& operator=(const ConfigMgr& src) = delete;

private:
    ConfigMgr();

    std::map<std::string, SectionInfo> _config_map;
};
