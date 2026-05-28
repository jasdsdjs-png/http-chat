#pragma once

#include <map>
#include <string>

// INI 文件中的一个配置段。
// 例如：
// [SelfServer]
// Port=10087
// 会被保存成 SectionInfo，其中 _section_datas["Port"] == "10087"。
struct SectionInfo {
    std::map<std::string, std::string> _section_datas;

    std::string operator[](const std::string& key) const {
        auto iter = _section_datas.find(key);
        if (iter == _section_datas.end()) {
            return "";
        }
        return iter->second;
    }
};

// 简单配置管理器，默认读取程序当前工作目录下的 config.ini。
// 注意：Visual Studio 调试时，当前工作目录通常是输出目录；
// ChatServer.vcxproj 已配置把 config.ini 复制到输出目录。
class ConfigMgr {
public:
    ~ConfigMgr() = default;
    static ConfigMgr& Inst();

    // 通过 cfg["SelfServer"]["Port"] 这种形式读取配置。
    SectionInfo operator[](const std::string& section) const;

    ConfigMgr(const ConfigMgr&) = delete;
    ConfigMgr& operator=(const ConfigMgr&) = delete;

private:
    ConfigMgr();

    std::map<std::string, SectionInfo> _config_map;
};
