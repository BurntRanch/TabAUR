#define TOML_IMPLEMENTATION
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "config.hpp"

using std::string;
using std::getenv;
using std::ofstream;
using std::ifstream;

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

Config::~Config() {
    alpm_trans_release(this->handle);
    alpm_release(this->handle);
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

    alpm_errno_t err;
    this->handle    = alpm_initialize("/", this->getConfigValue<string>("pacman.libFolder", "/var/lib/pacman").c_str(), &err);

    if (!(this->handle))
        throw std::invalid_argument("Failed to get an alpm handle! Error: " + string(alpm_strerror(err)));

    this->loadPacmanConfigFile("/etc/pacman.conf");
  
    this->secretRecipe = this->getConfigValue<bool>("secret.IwantChocolateChipMuffins", false);
}

bool addServers(alpm_db_t *db, string includeFilename, string repoName) {
    ifstream includeFile(includeFilename);

    if (!includeFile.is_open())
        return false;

    string line;

    while (std::getline(includeFile, line)) {
        if (hasStart(line, "#") || line.length() < 20)
            continue;
        
        size_t repo_pos = line.find("$repo");
        if (repo_pos != string::npos)
            line.replace(repo_pos, 5, repoName);
        size_t arch_pos = line.find("$arch");
        if (arch_pos != string::npos)
            line.replace(arch_pos, 5, "x86_64");

        line = line.substr(9);

        alpm_db_add_server(db, line.c_str());
    }

    return true;
}

void Config::loadPacmanConfigFile(string filename) {
    mINI::INIFile file(filename);
    mINI::INIStructure ini;

    file.read(ini);
    
    for (auto const& it : ini)
    {
        string section = it.first;
        if (section == "options")
            continue;

        alpm_db_t *db = alpm_register_syncdb(this->handle, section.c_str(), ALPM_SIG_USE_DEFAULT);
        if (db == NULL)
            continue;

        bool serversStatus = addServers(db, ini[section]["Include"], section);
        if (!serversStatus)
            log_printf(LOG_ERROR, _("Failed to open mirrors file! (%s)\n"), ini[section]["Include"].c_str());

        alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);

        this->repos = alpm_list_add(this->repos, db);
    }
}
