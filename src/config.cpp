#define TOML_IMPLEMENTATION
#include "config.hpp"
#include "ini.h"
#include "util.hpp"
#include <filesystem>
#include <iostream>

using std::ofstream;
using std::ifstream;

Config::Config() {}

Config::~Config() {
    if (this->handle) {
        alpm_trans_release(this->handle);
        alpm_release(this->handle);
    }

    if (this->repos)
        alpm_list_free(this->repos);
}

// initialize Config, can only be ran once for each Config instance.
void Config::init(string configFile, string themeFile) {
    if (this->initialized)
        return;
    string configDir = getConfigDir();
    bool   newUser   = false;

    if (!std::filesystem::exists(configDir)) {
        log_println(LOG_NONE, "Warning: TabAUR config folder was not found, Creating folders at {}!", configDir);
        std::filesystem::create_directories(configDir);

        newUser = true;
    }
    if (!std::filesystem::exists(configFile)) {
        log_println(LOG_NONE, "Warning: {} not found, generating new one", configFile);
        // https://github.com/hyprwm/Hyprland/blob/main/src/config/ConfigManager.cpp#L681
        ofstream f(configFile, std::ios::trunc);
        f << AUTOCONFIG;
        f.close();
    }
    if (!std::filesystem::exists(themeFile)) {
        log_println(LOG_NONE, "Warning: {} not found, generating new one", themeFile);
        ofstream f(themeFile, std::ios::trunc);
        f << AUTOTHEME;
        f.close();
    }

    this->loadConfigFile(configFile);
    this->loadThemeFile(themeFile);

    this->initialized = true;
    if (!std::filesystem::exists(config->cacheDir)) {
        log_println(LOG_WARN, "TabAUR cache folder was not found, Creating folders at {}!", config->cacheDir);
        std::filesystem::create_directories(config->cacheDir);
    }

    if (newUser)
        // ye i'm sorry for if it's too wide
        fmt::println(fg(color.blue),
                     "I see you're a new user, Welcome!\n"
                     "Even though the AUR is very convenient, it could contain packages that are unmoderated and could be unsafe.\n"
                     "You should always read the sources, popularity, and votes to judge by yourself whether the package is trustable.\n"
                     "This project is in no way liable for any damage done to your system as a result of AUR packages.\n"
                     "Thank you!\n"sv);
}

// get initialized variable
bool Config::isInitialized() {
    return this->initialized;
}

/*
* initialize all the "config.toml" variables
* and sanitize them (never trust user's input)
*/
void Config::initVars() {
    this->cacheDir      = this->getConfigValue<string>("general.cacheDir", getHomeCacheDir() + "/TabAUR");
    this->pmConfig      = this->getConfigValue<string>("pacman.ConfigFile", "/etc/pacman.conf");
    this->makepkgConf   = this->getConfigValue<string>("pacman.MakepkgConf", "/etc/makepkg.conf");
    this->makepkgBin    = this->getConfigValue<string>("bins.makepkg", "makepkg");
    this->git           = this->getConfigValue<string>("bins.git", "git");
    this->sudo          = this->getConfigValue<string>("general.sudo", "sudo");
    this->editorBin     = this->getConfigValue<string>("general.editor", "nano");
    this->useGit        = this->getConfigValue<bool>("general.useGit", true);
    this->aurOnly       = this->getConfigValue<bool>("general.aurOnly", false);
    this->debug         = this->getConfigValue<bool>("general.debug", true);
    this->colors        = this->getConfigValue<bool>("general.colors", true);
    this->secretRecipe  = this->getConfigValue<bool>("secret.recipe", false);
    fmt::disable_colors = this->colors == 0;

    sanitizeStr(this->sudo);
    sanitizeStr(this->editorBin);
    sanitizeStr(this->makepkgBin);
    sanitizeStr(this->makepkgConf);
    sanitizeStr(this->cacheDir);
    sanitizeStr(this->git);

    char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') {
        fmt::disable_colors = true;
        this->colors        = false;
    }
}

/** parse the config file (aka "config.toml")
 *  and initialize libalpm
 *  using the variables under the [pacman] table in "config.toml"
 *  @param the directory of the config file
*/
void Config::loadConfigFile(string_view filename) {
    try {
        this->tbl = toml::parse_file(filename);
    } catch (const toml::parse_error& err) {
        log_println(LOG_NONE, "ERROR: Parsing config file {} failed:", filename);
        std::cerr << err << std::endl;
        exit(-1);
    }
    this->initVars();

    alpm_errno_t err;
    this->handle = alpm_initialize(this->getConfigValue<string>("pacman.RootDir", "/").c_str(), this->getConfigValue<string>("pacman.DBPath", "/var/lib/pacman").c_str(), &err);

    if (!(this->handle))
        throw std::invalid_argument("Failed to get an alpm handle! Error: " + string(alpm_strerror(err)));

    this->loadPacmanConfigFile(this->getConfigValue("pacman.ConfigFile", "/etc/pacman.conf"));
}

/** parse the theme file (aka "theme.toml")
 *  @param filename The directory of the theme file
 */
void Config::loadThemeFile(string_view filename) {
    try {
        this->theme_tbl = toml::parse_file(filename);
    } catch (const toml::parse_error& err) {
        log_println(LOG_ERROR, "Parsing theme file {} failed:", filename);
        std::cerr << err << std::endl;
        exit(-1);
    }

    this->initColors();
}

/** Get the theme color variable and return a fmt::rgb type variable
 * Which can be used for colorize the text (useful for functions like log_println())
 * @param value The value we want
 * @param fallback The default value if it doesn't exists
 * @return fmt::rgb type variable 
 */
fmt::rgb Config::getThemeValue(const string& value, const string& fallback) {
    return hexStringToColor(this->theme_tbl["theme"][value].value<string>().value_or(fallback));
}

/** Get the theme color variable and return its hexcode
 * @param value The value we want
 * @param fallback The default value if it doesn't exists
 * @return color hexcode value
 */
string Config::getThemeHexValue(const string& value, const string& fallback) {
    return this->theme_tbl["theme"][value].value<string>().value_or(fallback);
}

void Config::initColors() {
    color.red           = this->getThemeValue("red",           "#ff2000");
    color.green         = this->getThemeValue("green",         "#00ff00");
    color.blue          = this->getThemeValue("blue",          "#00aaff");
    color.cyan          = this->getThemeValue("cyan",          "#00ffff");
    color.yellow        = this->getThemeValue("yellow",        "#ffff00");
    color.magenta       = this->getThemeValue("magenta",       "#ff11cc");
    color.gray          = this->getThemeValue("gray",          "#5a5a5a");
    color.aur           = this->getThemeValue("aur",           this->getThemeHexValue("blue", "#00aaff"));
    color.extra         = this->getThemeValue("extra",         this->getThemeHexValue("green", "#00ff00"));
    color.core          = this->getThemeValue("core",          this->getThemeHexValue("yellow", "#ffff00"));
    color.multilib      = this->getThemeValue("multilib",      this->getThemeHexValue("cyan", "#00ffff"));
    color.others        = this->getThemeValue("others",        this->getThemeHexValue("magenta", "#ff11cc"));
    color.version       = this->getThemeValue("version",       this->getThemeHexValue("green", "#00ff00"));
    color.last_modified = this->getThemeValue("last_modified", this->getThemeHexValue("magenta", "#ff11cc"));
    color.popularity    = this->getThemeValue("popularity",    this->getThemeHexValue("cyan", "#00ffff"));
    color.votes         = this->getThemeValue("votes",         this->getThemeHexValue("cyan", "#00ffff"));
    color.installed     = this->getThemeValue("installed",     this->getThemeHexValue("gray", "#5a5a5a"));
    color.index         = this->getThemeValue("index",         this->getThemeHexValue("magenta", "#ff11cc"));
}

bool addServers(alpm_db_t *db, const string& includeFilename, string_view repoName) {
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
    mINI::INIFile      file(filename);
    mINI::INIStructure ini;

    file.read(ini);

    for (auto const& it : ini) {
        string_view section = it.first;
        if (section == "options")
            continue;

        alpm_db_t *db = alpm_register_syncdb(this->handle, section.data(), ALPM_SIG_USE_DEFAULT);
        if (db == NULL)
            continue;

        bool serversStatus = addServers(db, ini[section.data()]["Include"], section);
        if (!serversStatus)
            log_println(LOG_ERROR, "Failed to open mirrors file! ({})", ini[section.data()]["Include"]);

        alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);

        this->repos = alpm_list_add(this->repos, db);
    }
}
