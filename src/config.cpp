#define TOML_IMPLEMENTATION
#include <cstdlib>
#include <iostream>
#include <config.hpp>
#include <strutil.hpp>
#include <filesystem>

Config::Config() {
    if (!std::filesystem::exists(this->getCacheDir())) {
        std::cout << "TabAUR config folder was not found, Creating folder at " << this->getCacheDir() << "!" << std::endl;
        std::filesystem::create_directory(this->getCacheDir());
    }
    string filename = getConfigDir() + "/config.toml";

    if (!std::filesystem::exists(this->getConfigDir())) {
        std::cout << "TabAUR config folder was not found, Creating folder at " << this->getConfigDir() << "!" << std::endl;
        std::filesystem::create_directory(this->getConfigDir());
    }
    loadConfigFile(filename);
}

string Config::getSystemCacheDir() {
    char* dir = std::getenv("XDG_CACHE_HOME");
    if (dir != NULL && std::filesystem::exists(std::string(dir))) {
        std::string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else
        return std::string(std::getenv("HOME")) + "/.cache";
}

string Config::getCacheDir() {
    return this->getSystemCacheDir() + "/TabAUR";
}

string Config::getSystemConfigDir() {
    char* dir = std::getenv("XDG_CONFIG_HOME");
    if (dir != NULL && std::filesystem::exists(std::string(dir))) {
        std::string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else
        return std::string(std::getenv("HOME")) + "/.config";
}

string Config::getConfigDir() {
    return this->getSystemConfigDir() + "/TabAUR";
}

void Config::loadConfigFile(string filename) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(filename);
    } catch (const toml::parse_error& err) {
        std::cerr << "Parsing failed:\n" << err << "\n";
        exit(-1);
    }
}
