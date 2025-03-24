#pragma once

#include "lyf.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <unordered_map>

using std::string;
using std::unordered_map;
namespace fs = std::filesystem;

class SectionInfo {
public:
    SectionInfo() {}

    ~SectionInfo() {
        _datas.clear();
    }

    SectionInfo(const SectionInfo& src)
        : _datas(src._datas) {}

    SectionInfo&
    operator=(const SectionInfo& src) {
        if (&src == this) {
            return *this;
        }
        this->_datas = src._datas;
        return *this;
    }

    string
    operator[](const string& key) {
        return GetValue(key);
    }

    string
    GetValue(const string& key) {
        if (_datas.find(key) == _datas.end()) {
            return string{};
        }
        return _datas[key];
    }

    void
    SetValue(const string& key, const string& value) {
        _datas[key] = value;
    }

    const unordered_map<string, string>&
    Datas() const {
        return _datas;
    }

private:
    unordered_map<string, string> _datas;
};

class ConfigManager : public lyf::Singleton<ConfigManager> {
    friend class lyf::Singleton<ConfigManager>;

public:
    ~ConfigManager() {
        _configMap.clear();
    }

    SectionInfo&
    operator[](const string& sectionName) {
        return _configMap[sectionName];
    }

public:
    void
    PrintConfig() const {
        string str;
        for (const auto& [sectionName, sectionInfo] : _configMap) {
            str += "[" + sectionName + "]\n";
            for (const auto& [key, value] : sectionInfo.Datas()) {
                str += key + " = " + value + "\n";
            }
        }
        std::cout << "Config content:\n" << str << std::endl;
    }

    string
    GetValue(const string& sectionName, const string& key) {
        if (_configMap.find(sectionName) == _configMap.end()) {
            return string{};
        }
        return _configMap[sectionName][key];
    }

    fs::path
    GetStaticPath() const {
        return _staticPath;
    }

    fs::path
    GetBinPath() const {
        return _binPath;
    }

private:
    void
    InitPath() {
        // 获取当前工作目录
        fs::path currentPath = fs::current_path();
        string binDir        = _configMap["Output"]["Path"];
        string staticDir     = _configMap["Static"]["Path"];
        // 构建资源文件存放的文件夹路径
        _binPath    = currentPath / binDir;
        _staticPath = currentPath / binDir / staticDir;
        // 创建资源文件存放的文件夹
        if (!fs::exists(_binPath)) {
            fs::create_directory(_binPath);
        }
        if (!fs::exists(_staticPath)) {
            fs::create_directory(_staticPath);
        }
    }

    ConfigManager() {
        fs::path currentPath = fs::current_path();
        fs::path configPath  = currentPath / "config.ini";
        std::cout << "Config path: " << configPath.string() << std::endl;
        // 使用Boost.PropertyTree来读取INI文件
        boost::property_tree::ptree pt;
        boost::property_tree::read_ini(configPath.string(), pt);

        // 遍历INI文件中的所有section
        for (const auto& [sectionName, sectionInfo] : pt) {
            SectionInfo info;
            // 对于每个section，遍历其所有的key-value对
            for (const auto& [key, value] : sectionInfo) {
                info.SetValue(key, value.data());
            }
            // 将section的key-value对保存到config_map中
            _configMap[sectionName] = std::move(info);
        }

        PrintConfig();

        InitPath();
    }

private:
    unordered_map<string, SectionInfo> _configMap; // 存储配置信息的map
    fs::path _staticPath;                          // 静态文件路径
    fs::path _binPath;                             // 二进制文件路径
};
