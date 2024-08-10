#ifndef CONFIG_HPP
#define CONFIG_HPP

#define TOML_HEADER_ONLY 0
#define TOML_ENABLE_FORMATTERS 0

#include <alpm.h>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>

#include "fmt/color.h"
#include "toml++/toml.hpp"

using std::filesystem::path;

// so we don't need to include util.hpp for getConfigValue()
std::string expandVar(std::string& str);

struct _color_t
{
    fmt::rgb red;
    fmt::rgb green;
    fmt::rgb blue;
    fmt::rgb cyan;
    fmt::rgb yellow;
    fmt::rgb magenta;
    fmt::rgb gray;
    fmt::rgb aur;
    fmt::rgb extra;
    fmt::rgb core;
    fmt::rgb multilib;
    fmt::rgb others;
    fmt::rgb version;
    fmt::rgb last_modified;
    fmt::rgb outofdate;
    fmt::rgb orphan;
    fmt::rgb popularity;
    fmt::rgb votes;
    fmt::rgb installed;
    fmt::rgb index;
};

class Config
{
public:
    alpm_handle_t*           handle = nullptr;
    alpm_list_t*             repos  = nullptr;
    std::string              makepkgBin;
    std::vector<std::string> editor;
    path                     cacheDir;
    std::string              pmConfig;
    std::string              sudo;
    std::string              git;
    std::string              makepkgConf;
    bool                     aurOnly;
    bool                     useGit;
    bool                     colors;
    bool                     secretRecipe;
    bool                     debug;
    bool                     quiet;
    bool                     noconfirm;
    // alpm transaction flags
    int flags;

    Config(std::string_view configFile, std::string_view themeFile, std::string_view configDir);
    ~Config();

    void initVars();
    void initColors();

    void loadConfigFile(std::string_view filename);
    void loadPacmanConfigFile(std::string filename);
    void loadThemeFile(std::string_view filename);

    // stupid c++ that wants template functions in header
    template <typename T>
    T getConfigValue(const std::string& value, T fallback)
    {
        std::optional<T> ret = this->tbl.at_path(value).value<T>();
        if constexpr (toml::is_string<T>)  // if we want to get a value that's a std::string
            return ret ? expandVar(ret.value()) : expandVar(fallback);
        else
            return ret.value_or(fallback);
    }

    fmt::rgb    getThemeValue(const std::string& value, const std::string& fallback);
    std::string getThemeHexValue(const std::string& value, const std::string& fallback);

private:
    toml::table tbl, theme_tbl;
};

extern std::unique_ptr<Config> config;
inline struct _color_t         color;

// we comment the default config values, just like /etc/pacman.conf
inline const constexpr std::string_view AUTOCONFIG = R"#([general]
# All options are commented out with their default values listed.
# If you wish to use different options values, uncomment and update those.
# It's safe to remove any options you want, just remember their default value

# If true(default), then it'll uses git for downloading/updating the aur repo.
# Else if false, then it'll use tarballs (.tar.gz) of the aur repo.
#useGit = true

# Which privilege elevation program we gonna use.
#sudo = "sudo"

# Default editor for opening PKGBUILDs
# It can include flags, e.g nano --modernbindings
#editor = "nano"

# Optional pretty prints, because some terminals don't suport them.
#colors = true

# If false (default), it'll allow you to operate on system packages as well as AUR.
# This option can be overrided with "-a" or "--aur-only".
#aurOnly = false

# Useful for knowing more about TabAUR operations.
# Please activate this before reporting bugs on our Github repo, thank you :)
#debug = true

# Search AUR packages based on what 
# Available options: "name", "name-desc", "depends", "makedepends", "optdepends", "checkdepends"
#searchBy = "name-desc"

# Where we are gonna download the AUR packages (default $XDG_CACHE_HOME/TabAUR, else ~/.cache/TabAUR)
#cacheDir = "$XDG_CACHE_HOME/TabAUR"

[bins]
#makepkg = "makepkg"
#git = "git"

[pacman]
#RootDir = "/"
#DBPath = "/var/lib/pacman"
#ConfigFile = "/etc/pacman.conf"
#MakepkgConf = "/etc/makepkg.conf"
)#";

inline const constexpr std::string_view AUTOTHEME = R"#([theme]
red = "#ff2000"
green = "#00ff00"
blue = "#00aaff"
cyan = "#00ffff"
yellow = "#ffff00"
magenta = "#ff11cc"
gray = "#5a5a5a"

# custom DB colors such as extra, aur, etc.
# Unfortunately toml doesn't support things like "aur = cyan"
# If you would like to let TabAUR use one of the colors above instead, you can just leave it empty.
#aur = "#00aaff"
#extra = "#00ff00"
#core = "#ffff00"
#multilib = "#00ffff"
# Other DBs such as extra-testing, core-testing, custom ones in custom repos, etc.
#others = "#ff11cc"

#version = "#00ff00"
#last_modified = "#ff11cc"
#outofdate = "#ff2000"
#orphan = "#ff2000"
#popularity = "#00ffff"
#votes = "#00ffff"
#installed = "#5a5a5a"
#index = "#ff11cc"
)#";

#endif
