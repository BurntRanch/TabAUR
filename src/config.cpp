#define TOML_IMPLEMENTATION
#include "config.hpp"
#include "util.hpp"
#include "ini.h"

using std::getenv;
using std::ofstream;
using std::ifstream;

Config::Config() {
    string configDir = this->getConfigDir();
    string filename  = configDir + "/config.toml";
    if (!fs::exists(configDir)) {
        log_printf(LOG_WARN, "TabAUR config folder was not found, Creating folders at %s!\n", configDir.c_str());
        fs::create_directories(configDir);
    }
    if (!fs::exists(filename)) {
        log_printf(LOG_WARN, "config.toml not found, generating new one\n");
        // https://github.com/hyprwm/Hyprland/blob/main/src/config/ConfigManager.cpp#L681
        ofstream configFile(filename, std::ios::trunc);
        configFile << defConfig;
        configFile.close();
    }
    loadConfigFile(filename);
    loadColors();

    string cacheDir = this->getCacheDir();
    if (!fs::exists(cacheDir)) {
        log_printf(LOG_WARN, "TabAUR cache folder was not found, Creating folders at %s!\n", cacheDir.c_str());
        fs::create_directories(cacheDir);
    }
}

Config::~Config() {
    alpm_trans_release(this->handle);
    alpm_release(this->handle);
    alpm_list_free(this->repos);
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
    return this->getConfigValue<string>("general.cacheDir", this->getHomeCacheDir() + "/TabAUR");
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

void Config::initializeVars() {
    this->cacheDir     = this->getCacheDir();
    this->makepkgBin   = this->getConfigValue<string>("bins.makepkg", "makepkg");
    this->git          = this->getConfigValue<string>("bins.git", "git");
    this->sudo         = this->getConfigValue<string>("general.sudo", "sudo");
    this->useGit       = this->getConfigValue<bool>("general.useGit", true);
    this->aurOnly      = this->getConfigValue<bool>("general.aurOnly", false);
    this->debug        = this->getConfigValue<bool>("general.debug", true);
    this->colors       = this->getConfigValue<bool>("general.colors", true);
    this->secretRecipe = this->getConfigValue<bool>("secret.IwantChocolateChipMuffins", false);

    sanitizeStr(this->sudo);
    sanitizeStr(this->makepkgBin);
    sanitizeStr(this->cacheDir);
    sanitizeStr(this->git);
}

void Config::loadConfigFile(string filename) {
    try {
        this->tbl = toml::parse_file(filename);
    } catch (const toml::parse_error& err) {
        log_printf(LOG_ERROR, "Parsing config.toml failed:\n");
        std::cerr << err << std::endl;
        exit(-1);
    }
    this->initializeVars();

    alpm_errno_t err;
    this->handle    = alpm_initialize(this->getConfigValue<string>("pacman.RootDir", "/").c_str(), this->getConfigValue<string>("pacman.DBPath", "/var/lib/pacman").c_str(), &err);

    if (!(this->handle))
        throw std::invalid_argument("Failed to get an alpm handle! Error: " + string(alpm_strerror(err)));

    this->loadPacmanConfigFile(this->getConfigValue("pacman.ConfigFile", "/etc/pacman.conf"));
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
            log_printf(LOG_ERROR, "Failed to open mirrors file! (%s)\n", ini[section]["Include"].c_str());

        alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);

        this->repos = alpm_list_add(this->repos, db);
    }
}
