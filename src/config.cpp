#include <stdexcept>
#define TOML_IMPLEMENTATION
#include "config.hpp"
#include "util.hpp"
#include "ini.h"

#include <filesystem>

using std::getenv;
using std::ofstream;
using std::ifstream;

namespace fs = std::filesystem;

Config::Config() {
    string configDir = this->getConfigDir();
    string confFilename  = configDir + "/config.toml";
    string themeFilename  = configDir + "/theme.toml";

    bool newUser = false;

    if (!fs::exists(configDir)) {
        log_printf(LOG_WARN, "TabAUR config folder was not found, Creating folders at {}!\n", configDir);
        fs::create_directories(configDir);

        newUser = true;
    }
    if (!fs::exists(confFilename)) {
        log_printf(LOG_WARN, "config.toml not found, generating new one\n");
        // https://github.com/hyprwm/Hyprland/blob/main/src/config/ConfigManager.cpp#L681
        ofstream f(confFilename, std::ios::trunc);
        f << defConfig;
        f.close();
    }
    if (!fs::exists(themeFilename)) {
        log_printf(LOG_WARN, "theme.toml not found, generating new one\n");
        ofstream f(themeFilename, std::ios::trunc);
        f << defTheme;
        f.close();
    }

    loadConfigFile(confFilename);
    loadThemeFile(themeFilename);

    if (newUser) 
        // ye i'm sorry for if it's too wide
        fmt::println(fmt::fg(getThemeValue("blue", blue)), "I see you're a new user, Welcome!\nEven though the AUR is very convenient, it could contain packages that are unmoderated and could be unsafe.\nYou should always read the sources, popularity, and votes to judge by yourself whether the package is trustable.\nThis project is in no way liable for any damage done to your system as a result of AUR packages.\nThank you!\n");

    string cacheDir = this->getCacheDir();
    if (!fs::exists(cacheDir)) {
        log_printf(LOG_WARN, "TabAUR cache folder was not found, Creating folders at {}!\n", cacheDir);
        fs::create_directories(cacheDir);
    }
    fmt::disable_colors = this->colors == 0;
}

Config::~Config() {
    alpm_trans_release(this->handle);
    alpm_release(this->handle);
    alpm_list_free(this->repos);
}

/*
* Get the user cache directory
* either from $XDG_CACHE_HOME or from $HOME/.cache/
* @return user's cache directory  
*/
string Config::getHomeCacheDir() {
    char* dir = getenv("XDG_CACHE_HOME");
    if (dir != NULL && fs::exists(string(dir))) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else {
        char *home = getenv("HOME");
        if (home == nullptr)
            throw std::invalid_argument("Failed to find $HOME, set it to your home directory!"); // nevermind..
        return string(home) + "/.cache";
    }
}

/*
* Get the user cache directory
* either from Config::getHomeCacheDir() or the cacheDir variable of config.toml
* @return TabAUR's cache directory 
*/
string Config::getCacheDir() {
    return this->getConfigValue<string>("general.cacheDir", this->getHomeCacheDir() + "/TabAUR");
}

/*
* Get the user config directory
* either from $XDG_CONFIG_HOME or from $HOME/.config/
* @return user's config directory  
*/
string Config::getHomeConfigDir() {
    char* dir = getenv("XDG_CONFIG_HOME");
    if (dir != NULL && fs::exists(string(dir))) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else {
        char *home = getenv("HOME");
        if (home == nullptr)
            throw std::invalid_argument("Failed to find $HOME, set it to your home directory!"); // nevermind..
        return string(home) + "/.config";
    }
}

/*
 * Get the TabAUR config directory 
 * where we'll have both "config.toml" and "theme.toml"
 * from Config::getHomeConfigDir()
 * @return TabAUR's config directory
 */
string Config::getConfigDir() {
    return this->getHomeConfigDir() + "/TabAUR";
}

/*
* initialize all the "config.toml" variables
* and sanitize them (never trust user's input)
*/
void Config::initializeVars() {
    this->cacheDir     = this->getCacheDir();
    this->makepkgConf  = this->getConfigValue<string>("pacman.MakepkgConf", "/etc/makepkg.conf");
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
    sanitizeStr(this->makepkgConf);
    sanitizeStr(this->cacheDir);
    sanitizeStr(this->git);
}

/** parse the config file (aka "config.toml")
 *  and initialize libalpm
 *  using the variables under the [pacman] table in "config.toml"
 *  @param the directory of the config file
*/
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

/** parse the theme file (aka "theme.toml")
 *  @param filename The directory of the theme file
 */
void Config::loadThemeFile(string filename) {
    try {
        this->theme_tbl = toml::parse_file(filename);
    } catch (const toml::parse_error& err) {
        log_printf(LOG_ERROR, "Parsing theme.toml failed:\n");
        std::cerr << err << std::endl;
        exit(-1);
    }
}

/** Get the theme color variable and return a fmt::rgb type variable
 * Which can be used for colorize the text (useful for functions like log_printf())
 * @param value The value we want
 * @param fallback The default value if it doesn't exists
 * @return fmt::rgb type variable 
 */
fmt::rgb Config::getThemeValue(string value, string fallback) {
    return hexStringToColor(this->theme_tbl["theme"][value].value<string>().value_or(fallback));
}

/** Get the theme color variable and return its hexcode
 * @param value The value we want
 * @param fallback The default value if it doesn't exists
 * @return color hexcode value
 */
string Config::getThemeHexValue(string value, string fallback) {
    return this->theme_tbl["theme"][value].value<string>().value_or(fallback);
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
            log_printf(LOG_ERROR, "Failed to open mirrors file! ({})\n", ini[section]["Include"]);

        alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);

        this->repos = alpm_list_add(this->repos, db);
    }
}
