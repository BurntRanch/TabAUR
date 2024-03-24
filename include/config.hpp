#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#define TOML_HEADER_ONLY 1
#include "toml.hpp"
#include "util.hpp"

using std::string;
namespace fs = std::filesystem;

class Config {
public:
    bool   useGit;
    bool   aurOnly;
    string makepkgBin;
    string cacheDir;
    string sudo;
    bool   colors;
    bool   secretRecipe;
    bool   debug;

    Config();
    string getHomeCacheDir();
    string getCacheDir();
    string getHomeConfigDir();
    string getConfigDir();
    void   loadConfigFile(string filename);
    void   loadColors();

    // stupid c++ that wants template functions in header
    template <typename T>
    T getConfigValue(string value, T fallback) {
        toml::optional<T> ret = this->tbl.at_path(value).value<T>();
        if constexpr (toml::is_string<T>) // if we want to get a value that's a string
            return ret ? expandVar(ret.value()) : expandVar(fallback);
        else
            return ret ? ret.value() : fallback;
    }

private:
    toml::table tbl;
};

extern Config config;

// we comment the default config values, just like /etc/pacman.conf
inline const string defConfig = R"#([general]
# if true(default), then it'll uses git for downloading/updating the aur repo
# else if false, then it'll use tarballs (.tar.gz) of the aur repo
#useGit = true

# If you use sudo or doas
#sudo = "sudo"

# Optional pretty prints, because some terminals don't suport them
#colors = true

# If false (default), it'll allow you to operate on system packages as well as AUR.
# this option can be overrided by the --aur-only long option or running "-Ra" instead of "-R".
#aurOnly = false

# Useful for knowing more about TabAUR operations
# please activate this before reporting bugs on our Github repo, thank you :)
#debug = false

[bins]
#makepkgBin = "makepkg"

[storage]
# Where we are gonna download the AUR packages (default $XDG_CACHE_HOME, else ~/.cache/TabAUR)
#cacheDir = "$XDG_CACHE_HOME"
)#";

static string NOCOLOR;
static string BOLD;

static string BLACK;
static string GREEN;
static string YELLOW;
static string BLUE;
static string RED;
static string MAGENTA;
static string CYAN;
static string WHITE;

static string BOLDBLACK;
static string BOLDGREEN;
static string BOLDYELLOW;
static string BOLDBLUE;
static string BOLDRED;
static string BOLDMAGENTA;
static string BOLDCYAN;
static string BOLDWHITE;
#endif
