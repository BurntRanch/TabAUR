#ifndef UTIL_HPP
#define UTIL_HPP
#define FMT_HEADER_ONLY 1

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <optional>

#include "fmt/base.h"
#include "fmt/color.h"
#include "config.hpp"

using std::string;
using std::vector;
using std::unique_ptr;

// taken from pacman
#define _(str) (char*)str
struct TaurPkg_t;
class TaurBackend;

#define BOLD_TEXT(x) (fmt::emphasis::bold | fmt::fg(x))
#define alpm_list_smart_pointer unique_ptr<alpm_list_t, decltype(&alpm_list_free)>
#define make_list_smart_pointer(pointer) (unique_ptr<alpm_list_t, decltype(&alpm_list_free)>(pointer, alpm_list_free))

#define alpm_list_smart_deleter unique_ptr<alpm_list_t, decltype(&free_list_and_internals)>
#define make_list_smart_deleter(pointer) (unique_ptr<alpm_list_t, decltype(&free_list_and_internals)>(pointer, free_list_and_internals))

enum log_level {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_NONE    // display no prefix for this.
};

bool hasEnding(string const& fullString, string const& ending);
bool hasStart(string const& fullString, string const& start);
string expandVar(string& str);
bool is_number(const string& s, bool allowSpace = false);
bool taur_read_exec(std::vector<const char*> cmd, string *output);
void interruptHandler(int);
bool taur_exec(std::vector<const char*> cmd, bool exitOnFailure = true);
void sanitizeStr(string& str);
bool is_package_from_syncdb(alpm_pkg_t *pkg, alpm_list_t *syncdbs);
bool commitTransactionAndRelease(bool soft = false);
void printPkgInfo(TaurPkg_t &pkg, string db_name, int index = -1);
void free_list_and_internals(alpm_list_t *list);
fmt::text_style getColorFromDBName(string db_name);
std::optional<std::vector<TaurPkg_t>> askUserForPkg(vector<TaurPkg_t> pkgs, TaurBackend& backend, bool useGit);
vector<alpm_pkg_t *> filterAURPkgs(vector<alpm_pkg_t *> pkgs, alpm_list_t *syncdbs, bool inverse);
string shell_exec(string cmd);
vector<string> split(string text, char delim);
fmt::rgb hexStringToColor(string hexstr);
string getTitleForPopularity(float popularity);
string getConfigDir();
string getHomeCacheDir();
string getHomeConfigDir();

template <typename... Args>
void log_printf(int log, const fmt::text_style &ts, string fmt, Args&&... args) {
    switch(log) {
        case LOG_ERROR:
            fmt::print(ts, "ERROR: "); break;
        case LOG_WARN:
            fmt::print(ts, "Warning: "); break;
        case LOG_INFO:
            fmt::print(ts, "Info: "); break;
        case LOG_DEBUG:
            if (!config->debug)
                return;
            fmt::print(ts, "[DEBUG]: ");
            break;
    }
    fmt::print(ts, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(int log, string fmt, Args&&... args) {
    switch(log) {
        case LOG_ERROR:
            fmt::print(BOLD_TEXT(config->getThemeValue("red", red)), "ERROR: "); break;
        case LOG_WARN:
            fmt::print(BOLD_TEXT(config->getThemeValue("yellow", yellow)), "Warning: "); break;
        case LOG_INFO:
            fmt::print(BOLD_TEXT(config->getThemeValue("cyan", cyan)), "Info: "); break;
        case LOG_DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(config->getThemeValue("magenta", magenta)), "[DEBUG]: ");
            break;
    }
    fmt::print(fmt, std::forward<Args>(args)...);
}

// could use fmt::join, but doesn't work with vector<const char*>
template <typename T>
void print_vec(std::vector<T> vec, bool display) {
    if(!display)
        return;
    
    for(auto& i : vec)
        std::cout << i << " ";
    std::cout << std::endl;
}

template <typename T>
T sanitize(T beg, T end) {
    T dest = beg;
    for (T itr = beg; itr != end; ++itr)
        // if its:
        // 1. a digit
        // 2. an uppercase letter
        // 3. a lowercase letter
        if (!(((*itr) >= 48 && (*itr) <= 57) && ((*itr) >= 65 && (*itr) <= 90) && ((*itr) >= 97 && (*itr) <= 122)))
            *(dest++) = *itr;
    return dest;
}

static inline std::vector<string> secret = {
    {"Ingredients:"},
    {R"#(
    3/4 cup milk
    1/2 cup vegetable oil
    1 large egg
    2 cups all-purpose flour
    1/2 cup white sugar
    2 teaspoons baking powder
    1/2 teaspoon salt
    3/4 cup mini semi-sweet chocolate chips
    1 and 1/2 tablespoons white sugar
    1 tablespoon brown sugar
    )#"},
    {R"#(Step 1:
    Preheat the oven to 400 degrees F (200 degrees C).
    Grease a 12-cup muffin tin or line cups with paper liners.
    )#"},
    {R"#(Step 2:
    Combine milk, oil, and egg in a small bowl until well blended. 
    Combine flour, 1/2 cup sugar, baking powder, and salt together in a large bowl, making a well in the center. 
    Pour milk mixture into well and stir until batter is just combined; fold in chocolate chips.
    )#"},
    {R"#(Step 3:
    Spoon batter into the prepared muffin cups, filling each 2/3 full. 
    Combine 1 and 1/2 tablespoons white sugar and 1 tablespoon brown sugar in a small bowl; sprinkle on tops of muffins.
    )#"},
    {R"#(Step 4:
    Bake in the preheated oven until tops spring back when lightly pressed, about 18 to 20 minutes. 
    Cool in the tin briefly, then transfer to a wire rack.
            
            Serve warm or cool completely.
    )#"},
    {"Credits to: https://www.allrecipes.com/recipe/7906/chocolate-chip-muffins/"}
};

#endif
