#define TOML_HEADER_ONLY 0
#include "config.hpp"

#include <filesystem>
#include <iostream>

#include "ini.h"
#include "util.hpp"

Config::~Config()
{
    if (this->handle)
    {
        alpm_trans_release(this->handle);
        alpm_release(this->handle);
    }

    if (this->repos)
        alpm_list_free(this->repos);
}

// initialize Config, can only be ran once for each Config instance.
Config::Config(const std::string_view configFile, const std::string_view themeFile, const std::string_view configDir)
{
    bool newUser = false;

    if (!std::filesystem::exists(configDir))
    {
        log_println(WARN, _("TabAUR config folder was not found, Creating folders at {}!"), configDir);
        std::filesystem::create_directories(configDir);

        newUser = true;
    }
    if (!std::filesystem::exists(configFile))
    {
        log_println(WARN, _("{} not found, generating new one"), configFile);
        // https://github.com/hyprwm/Hyprland/blob/main/src/config/ConfigManager.cpp#L681
        std::ofstream f(configFile.data(), std::ios::trunc);
        f << AUTOCONFIG;
        f.close();
    }
    if (!std::filesystem::exists(themeFile))
    {
        log_println(WARN, _("{} not found, generating new one"), themeFile);
        std::ofstream f(themeFile.data(), std::ios::trunc);
        f << AUTOTHEME;
        f.close();
    }

    this->loadConfigFile(configFile);
    this->loadThemeFile(themeFile);

    if (!std::filesystem::exists(this->cacheDir))
    {
        log_println(WARN, _("TabAUR cache folder was not found, Creating folders at {}!"), this->cacheDir.string());
        std::filesystem::create_directories(this->cacheDir);
    }

    if (newUser)
        // ye i'm sorry if it's too wide
        log_println(
            NONE, fg(color.blue),
            _("I see you're a new user, Welcome!\n"
              "Even though the AUR is very convenient, it could contain packages that are unmoderated and could be "
              "unsafe.\n"
              "You should always read the sources, popularity, and votes to judge by yourself whether the package is "
              "trustable.\n"
              "This project is in no way liable for any damage done to your system as a result of AUR packages.\n"
              "Thank you!\n"));
}

/*
 * initialize all the "config.toml" variables
 * and sanitize them (never trust user's input)
 */
void Config::initVars()
{
    this->cacheDir      = path(this->getConfigValue<std::string>("general.cacheDir", std::string(getHomeCacheDir()) + "/TabAUR"));
    this->pmConfig      = this->getConfigValue<std::string>("pacman.ConfigFile", "/etc/pacman.conf");
    this->makepkgConf   = this->getConfigValue<std::string>("pacman.MakepkgConf", "/etc/makepkg.conf");
    this->makepkgBin    = this->getConfigValue<std::string>("bins.makepkg", "makepkg");
    this->git           = this->getConfigValue<std::string>("bins.git", "git");
    this->sudo          = this->getConfigValue<std::string>("general.sudo", "sudo");
    this->useGit        = this->getConfigValue<bool>("general.useGit", true);
    this->aurOnly       = this->getConfigValue<bool>("general.aurOnly", false);
    this->debug         = this->getConfigValue<bool>("general.debug", true);
    this->colors        = this->getConfigValue<bool>("general.colors", true);
    this->secretRecipe  = this->getConfigValue<bool>("secret.recipe", false);
    fmt::disable_colors = (!this->colors);

    sanitizeStr(this->sudo);
    sanitizeStr(this->makepkgBin);
    sanitizeStr(this->makepkgConf);
    sanitizeStr(this->git);

    for (auto& str : split(this->getConfigValue<std::string>("general.editor", "nano"), ' '))
    {
        sanitizeStr(str);
        this->editor.push_back(str);
    }

    char* no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0')
    {
        fmt::disable_colors = true;
        this->colors        = false;
    }
}

/** parse the config file (aka "config.toml")
 *  and initialize libalpm
 *  using the variables under the [pacman] table in "config.toml"
 *  @param the directory of the config file
 */
void Config::loadConfigFile(std::string_view filename)
{
    try
    {
        this->tbl = toml::parse_file(filename);
    }
    catch (const toml::parse_error& err)
    {
        log_println(ERROR, _("Parsing config file {} failed:"), filename);
        std::cerr << err << std::endl;
        exit(-1);
    }
    this->initVars();

    alpm_errno_t err;
    this->handle = alpm_initialize(this->getConfigValue<std::string>("pacman.RootDir", "/").c_str(),
                                   this->getConfigValue<std::string>("pacman.DBPath", "/var/lib/pacman").c_str(), &err);

    if (!(this->handle))
        die(_("Failed to get an alpm handle! Error: {}"), alpm_strerror(err));

    this->loadPacmanConfigFile(this->getConfigValue<std::string>("pacman.ConfigFile", "/etc/pacman.conf"));
}

/** parse the theme file (aka "theme.toml")
 *  @param filename The directory of the theme file
 */
void Config::loadThemeFile(std::string_view filename)
{
    try
    {
        this->theme_tbl = toml::parse_file(filename);
    }
    catch (const toml::parse_error& err)
    {
        log_println(ERROR, _("Parsing theme file {} failed:"), filename);
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
fmt::rgb Config::getThemeValue(const std::string& value, const std::string& fallback)
{
    return hexStringToColor(this->theme_tbl["theme"][value].value<std::string>().value_or(fallback));
}

/** Get the theme color variable and return its hexcode
 * @param value The value we want
 * @param fallback The default value if it doesn't exists
 * @return color hexcode value
 */
std::string Config::getThemeHexValue(const std::string& value, const std::string& fallback)
{
    return this->theme_tbl["theme"][value].value<std::string>().value_or(fallback);
}

// clang-format off
void Config::initColors()
{
    color.red           = this->getThemeValue("red",           "#ff2000");
    color.green         = this->getThemeValue("green",         "#00ff00");
    color.blue          = this->getThemeValue("blue",          "#00aaff");
    color.cyan          = this->getThemeValue("cyan",          "#00ffff");
    color.yellow        = this->getThemeValue("yellow",        "#ffff00");
    color.magenta       = this->getThemeValue("magenta",       "#ff11cc");
    color.gray          = this->getThemeValue("gray",          "#5a5a5a");
    color.aur           = this->getThemeValue("aur",           this->getThemeHexValue("blue",    "#00aaff"));
    color.extra         = this->getThemeValue("extra",         this->getThemeHexValue("green",   "#00ff00"));
    color.core          = this->getThemeValue("core",          this->getThemeHexValue("yellow",  "#ffff00"));
    color.multilib      = this->getThemeValue("multilib",      this->getThemeHexValue("cyan",    "#00ffff"));
    color.others        = this->getThemeValue("others",        this->getThemeHexValue("magenta", "#ff11cc"));
    color.version       = this->getThemeValue("version",       this->getThemeHexValue("green",   "#00ff00"));
    color.last_modified = this->getThemeValue("last_modified", this->getThemeHexValue("magenta", "#ff11cc"));
    color.outofdate     = this->getThemeValue("outofdate",     this->getThemeHexValue("red",     "#ff2000"));
    color.orphan        = this->getThemeValue("orphan",        this->getThemeHexValue("red",     "#ff2000"));
    color.popularity    = this->getThemeValue("popularity",    this->getThemeHexValue("cyan",    "#00ffff"));
    color.votes         = this->getThemeValue("votes",         this->getThemeHexValue("cyan",    "#00ffff"));
    color.installed     = this->getThemeValue("installed",     this->getThemeHexValue("gray",    "#5a5a5a"));
    color.index         = this->getThemeValue("index",         this->getThemeHexValue("magenta", "#ff11cc"));
}

// clang-format on
bool addServers(alpm_db_t* db, const std::string_view includeFilename, std::string_view repoName)
{
    std::ifstream includeFile(includeFilename.data());

    if (!includeFile.is_open())
        return false;

    std::string line;

    while (std::getline(includeFile, line))
    {
        if (hasStart(line, "#") || line.length() < 20)
            continue;

        size_t repo_pos = line.find("$repo");
        if (repo_pos != std::string::npos)
            line.replace(repo_pos, "$repo"_len, repoName);

        size_t arch_pos = line.find("$arch");
        if (arch_pos != std::string::npos)
            line.replace(arch_pos, "$arch"_len, "x86_64");

        // Each line that has a server starts with a 9 letter header
        line = line.substr(9);

        alpm_db_add_server(db, line.c_str());
    }

    return true;
}

void Config::loadPacmanConfigFile(const std::string_view filename)
{
    mINI::INIFile      file(filename.data());
    mINI::INIStructure ini;

    file.read(ini);

    for (auto const& it : ini)
    {
        std::string_view section = it.first;
        if (section == "options")
            continue;

        alpm_db_t* db = alpm_register_syncdb(this->handle, section.data(), ALPM_SIG_USE_DEFAULT);
        if (db == NULL)
            continue;

        bool serversStatus = addServers(db, ini[section.data()]["Include"], section);
        if (!serversStatus)
            log_println(ERROR, _("Failed to open mirrors file! ({})"), ini[section.data()]["Include"]);

        alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);

        this->repos = alpm_list_add(this->repos, db);
    }
}
