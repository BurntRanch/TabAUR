#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <alpm.h>
#define TOML_HEADER_ONLY 0
#include "toml.hpp"

using std::string;

// so we don't need to include util.hpp for getConfigValue()
string  expandVar(string& str);

class Config {
public:
    alpm_handle_t *handle = nullptr;
    alpm_list_t *repos = nullptr;
    string makepkgBin;
    string cacheDir;
    string sudo;
    string git;
    bool aurOnly;
    bool useGit;
    bool colors;
    bool secretRecipe;
    bool debug;

    Config();
    ~Config();

    string getHomeCacheDir();
    string getCacheDir();
    string getHomeConfigDir();
    string getConfigDir();
    void   initializeVars();
    void   loadConfigFile(string filename); 
    void   loadColors();
    void   loadPacmanConfigFile(string filename);
    
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

extern std::unique_ptr<Config> config;

// we comment the default config values, just like /etc/pacman.conf
inline const string defConfig = R"#([general]
# All options are commented out with their default values listed.
# If you wish to use different options values, uncomment and update those.
# It's safe to remove any options you want, just remember their default value

# If true(default), then it'll uses git for downloading/updating the aur repo.
# Else if false, then it'll use tarballs (.tar.gz) of the aur repo.
#useGit = true

# If you use sudo or doas.
#sudo = "sudo"

# Optional pretty prints, because some terminals don't suport them.
#colors = true

# If false (default), it'll allow you to operate on system packages as well as AUR.
# This option can be overrided by the --aur-only long option or running "-Ra" instead of "-R".
#aurOnly = false

# Useful for knowing more about TabAUR operations.
# Please activate this before reporting bugs on our Github repo, thank you :)
#debug = true

# Where we are gonna download the AUR packages (default $XDG_CACHE_HOME, else ~/.cache/TabAUR)
#cacheDir = "$XDG_CACHE_HOME"

[bins]
#makepkg = "makepkg"
#git = "git"

[pacman]
#RootDir = "/"
#DBPath = "/var/lib/pacman"
#ConfigFile = "/etc/pacman.conf"
)#";

inline string NOCOLOR;
inline string BOLD;

inline string BLACK;
inline string GREEN;
inline string YELLOW;
inline string BLUE;
inline string RED;
inline string MAGENTA;
inline string CYAN;
inline string WHITE;

inline string BOLDBLACK;
inline string BOLDGREEN;
inline string BOLDYELLOW;
inline string BOLDBLUE;
inline string BOLDRED;
inline string BOLDMAGENTA;
inline string BOLDCYAN;
inline string BOLDWHITE;
#endif
