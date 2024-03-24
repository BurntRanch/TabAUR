#define TOML_IMPLEMENTATION
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "config.hpp"
#include "util.hpp"

using std::getenv;
using std::ofstream;

Config::Config() {
    string configDir = this->getConfigDir();
    string filename  = configDir + "/config.toml";
    if (!fs::exists(configDir)) {
        log_printf(LOG_WARN, _("TabAUR config folder was not found, Creating folders at %s!\n"), configDir.c_str());
        fs::create_directories(configDir);
    }
    if (!fs::exists(filename)) {
        log_printf(LOG_WARN, _("config.toml not found, generating new one\n"));
        // https://github.com/hyprwm/Hyprland/blob/main/src/config/ConfigManager.cpp#L681
        ofstream configFile(filename, std::ios::trunc);
        configFile << defConfig;
        configFile.close();
    }
    loadConfigFile(filename);
    loadColors();

    string cacheDir = this->getCacheDir();
    if (!fs::exists(cacheDir)) {
        log_printf(LOG_WARN, _("TabAUR cache folder was not found, Creating folders at %s!\n"), cacheDir.c_str());
        fs::create_directories(cacheDir);
    }
}

string Config::getHomeCacheDir() {
    char* dir = getenv("XDG_CACHE_HOME");
    if (dir != NULL && fs::exists(string(dir))) {
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
    if (dir != NULL && fs::exists(string(dir))) {
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

    this->useGit       = this->getConfigValue<bool>("general.useGit", true);
    this->aurOnly      = this->getConfigValue<bool>("general.aurOnly", false);
    this->colors       = this->getConfigValue<bool>("general.colors", true);
    this->debug        = this->getConfigValue<bool>("general.debug", false);
    this->secretRecipe = this->getConfigValue<bool>("secret.IwantChocolateChipMuffins", false);
    this->makepkgBin   = this->getConfigValue<string>("bins.makepkgBin", "makepkg");
    this->sudo         = this->getConfigValue<string>("general.sudo", "sudo");
    this->cacheDir     = this->getCacheDir();
}

void Config::loadColors() {
    if (this->colors) {
        NOCOLOR = "\033[0m";
        BOLD    = "\033[0;1m";
        BLACK   = "\033[0;30m";
        RED     = "\033[0;31m";
        GREEN   = "\033[0;32m";
        YELLOW  = "\033[0;33m";
        BLUE    = "\033[0;34m";
        MAGENTA = "\033[0;35m";
        CYAN    = "\033[0;36m";
        WHITE   = "\033[0;37m";

        BOLDBLACK   = "\033[1;30m";
        BOLDRED     = "\033[1;31m";
        BOLDGREEN   = "\033[1;32m";
        BOLDYELLOW  = "\033[1;33m";
        BOLDBLUE    = "\033[1;34m";
        BOLDMAGENTA = "\033[1;35m";
        BOLDCYAN    = "\033[1;36m";
        BOLDWHITE   = "\033[1;37m";
    } else {
        NOCOLOR = "";
        BOLD    = "";
        BLACK   = "";
        RED     = "";
        GREEN   = "";
        YELLOW  = "";
        BLUE    = "";
        MAGENTA = "";
        CYAN    = "";
        WHITE   = "";

        BOLDBLACK   = "";
        BOLDRED     = "";
        BOLDGREEN   = "";
        BOLDYELLOW  = "";
        BOLDBLUE    = "";
        BOLDMAGENTA = "";
        BOLDCYAN    = "";
        BOLDWHITE   = "";
    }
}
