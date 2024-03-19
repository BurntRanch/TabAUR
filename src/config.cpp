#define TOML_IMPLEMENTATION
#include <cstdlib>
#include <iostream>
#include <config.hpp>
#include <filesystem>
#include <fstream>

using std::string;
using std::getenv;
using std::ofstream;

Config::Config() {
    string configDir = this->getConfigDir();
    string filename = configDir + "/config.toml";
    if (!std::filesystem::exists(configDir)) {
        log_printf(LOG_WARN, _("TabAUR config folder was not found, Creating folders at %s!\n"), configDir.c_str());
        std::filesystem::create_directories(configDir);
    }
    if(!std::filesystem::exists(filename)) {
        log_printf(LOG_WARN, _("config.toml not found, generating new one\n"));
        // https://github.com/hyprwm/Hyprland/blob/main/src/config/ConfigManager.cpp#L681
        ofstream configFile(filename, std::ios::trunc);
        configFile << defConfig;
        configFile.close();
    }
    loadConfigFile(filename);
    
    string cacheDir = this->getCacheDir();
    if (!std::filesystem::exists(cacheDir)) {
        log_printf(LOG_WARN, _("TabAUR cache folder was not found, Creating folders at %s!\n"), cacheDir.c_str());
        std::filesystem::create_directories(cacheDir);
    }
}

string Config::getHomeCacheDir() {
    char* dir = getenv("XDG_CACHE_HOME");
    if (dir != NULL && std::filesystem::exists(string(dir))) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else
        return string(getenv("HOME")) + "/.cache";
}

string Config::getCacheDir() {
    return this->getConfigValue<string>("storage.cacheDir", this->getHomeCacheDir() + "/TabAUR");
}

string Config::getHomeConfigDir() {
    char* dir = getenv("XDG_CONFIG_HOME");
    if (dir != NULL && std::filesystem::exists(string(dir))) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else
        return string(getenv("HOME")) + "/.config";
}

string Config::getConfigDir() {
    return this->getHomeConfigDir() + "/TabAUR";
}

void Config::loadConfigFile(string filename) {
    try {
        this->tbl = toml::parse_file(filename);
    } catch (const toml::parse_error& err) {
        log_printf(LOG_ERROR, _("Parsing config.toml failed:\n"));
        std::cerr << err << std::endl;
        exit(-1);
    }
    
    this->sudo      = this->getConfigValue<string>("general.sudo", "sudo"); sanitizeStr(this->sudo);
    this->useGit    = this->getConfigValue<bool>("general.useGit", true);
    this->aurOnly   = this->getConfigValue<bool>("general.aurOnly", false);
    this->makepkgBin = this->getConfigValue<string>("bins.makepkgBin", "makepkg"); sanitizeStr(this->makepkgBin);
    this->cacheDir  = this->getCacheDir(); sanitizeStr(this->cacheDir);
    this->colors    = this->getConfigValue<bool>("general.colors", true);
    this->secretRecipe = this->getConfigValue<bool>("secret.IwantChocolateChipMuffins", false);
}
