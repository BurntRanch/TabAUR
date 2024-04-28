#ifndef UTIL_HPP
#define UTIL_HPP

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config.hpp"
#include "fmt/base.h"
#include "fmt/color.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include <type_traits>

using std::string;
using std::string_view;
using std::vector;
using std::unique_ptr;

// taken from pacman
#define _(str) (char *)str
struct TaurPkg_t;
class TaurBackend;

#define BOLD                             fmt::emphasis::bold
#define BOLD_TEXT(x)                     (fmt::emphasis::bold | fmt::fg(x))
#define alpm_list_smart_pointer          unique_ptr<alpm_list_t, decltype(&alpm_list_free)>
#define make_list_smart_pointer(pointer) (unique_ptr<alpm_list_t, decltype(&alpm_list_free)>(pointer, alpm_list_free))

#define alpm_list_smart_deleter          unique_ptr<alpm_list_t, decltype(&free_list_and_internals)>
#define make_list_smart_deleter(pointer) (unique_ptr<alpm_list_t, decltype(&free_list_and_internals)>(pointer, free_list_and_internals))

enum prompt_yn {
    PROMPT_YN_DIFF,
    PROMPT_YN_CONTINUE_WITHOUT_DIFF,
    PROMPT_YN_EDIT_PKGBUILD,
    PROMPT_YN_PROCEED_INSTALL,
    PROMPT_YN_PROCEED_TRANSACTION,
};
enum prompt_list {
    PROMPT_LIST_CLEANBUILDS,
    PROMPT_LIST_REVIEWS,
};

enum {
    YES = 1,
    NO = 0,
};

enum log_level {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_NONE // display no prefix for this.
};

bool                             hasEnding(string_view fullString, string_view ending);
bool                             hasStart(string_view fullString, string_view start);
string                           expandVar(string& str);
bool                             is_numerical(string_view s, bool allowSpace = false);
bool                             taur_read_exec(vector<const char *> cmd, string& output, bool exitOnFailure = true);
void                             interruptHandler(int);
bool                             taur_exec(vector<const char *> cmd, bool exitOnFailure = true);
void                             sanitizeStr(string& str);
bool                             is_package_from_syncdb(alpm_pkg_t *pkg, alpm_list_t *syncdbs);
bool                             commitTransactionAndRelease(bool soft = false);
void                             printPkgInfo(TaurPkg_t& pkg, string_view db_name, int index = -1);
string                           makepkg_list(string const& pkg_name, string const& path);
void                             free_list_and_internals(alpm_list_t *list);
fmt::text_style                  getColorFromDBName(string_view db_name);
vector<alpm_pkg_t *>             filterAURPkgs(vector<alpm_pkg_t *> pkgs, alpm_list_t *syncdbs, bool inverse);
string                           shell_exec(string_view cmd);
vector<string>                   split(string_view text, char delim);
fmt::rgb                         hexStringToColor(string_view hexstr);
void                             ctrl_d_handler();
string                           getTitleFromVotes(float votes);
string                           getConfigDir();
string                           getHomeCacheDir();
string                           getHomeConfigDir();
bool                             makepkg_exec(string_view cmd, bool exitOnFailure = true);
bool                             pacman_exec(string_view op, vector<string> const& args, bool exitOnFailure = true, bool root = true);
std::optional<vector<TaurPkg_t>> askUserForPkg(vector<TaurPkg_t> pkgs, TaurBackend& backend, bool useGit);

template <typename T>
struct is_fmt_convertible {
private:
    template <typename U>
    static auto test(int) -> decltype(fmt::to_string(std::declval<U>()), std::true_type{});

    template <typename>
    static auto test(...) -> std::false_type;

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
constexpr bool is_fmt_convertible_v = is_fmt_convertible<T>::value;

template <typename... Args>
void log_println(int log, const fmt::text_style& ts, std::string_view fmt, Args&&... args) {
    switch (log) {
        case LOG_ERROR:
            fmt::print(BOLD_TEXT(color.red), "ERROR: ");
            break;
        case LOG_WARN:
            fmt::print(BOLD_TEXT(color.yellow), "Warning: ");
            break;
        case LOG_INFO:
            fmt::print(BOLD_TEXT(color.cyan), "Info: ");
            break;
        case LOG_DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(color.magenta), "[DEBUG]: ");
            break;
    }
    // I don't want to add a '\n' each time i'm writing a log_printf(), I just forget it all the time
    fmt::println(ts, fmt::runtime(fmt), std::forward<Args>(args)...);
}

template <typename... Args>
void log_println(int log, std::string_view fmt, Args&&... args) {
    switch (log) {
        case LOG_ERROR:
            fmt::print(BOLD_TEXT(color.red), "ERROR: ");
            break;
        case LOG_WARN:
            fmt::print(BOLD_TEXT(color.yellow), "Warning: ");
            break;
        case LOG_INFO:
            fmt::print(BOLD_TEXT(color.cyan), "Info: ");
            break;
        case LOG_DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(color.magenta), "[DEBUG]: ");
            break;
    }
    fmt::println(fmt::runtime(fmt), std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(int log, std::string_view fmt, Args&&... args) {
    switch (log) {
        case LOG_ERROR:
            fmt::print(BOLD_TEXT(color.red), "ERROR: ");
            break;
        case LOG_WARN:
            fmt::print(BOLD_TEXT(color.yellow), "Warning: ");
            break;
        case LOG_INFO:
            fmt::print(BOLD_TEXT(color.cyan), "Info: ");
            break;
        case LOG_DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(color.magenta), "[DEBUG]: ");
            break;
    }
    fmt::print(fmt::runtime(fmt), std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(int log, const fmt::text_style& ts, std::string_view fmt, Args&&... args) {
    switch (log) {
        case LOG_ERROR:
            fmt::print(BOLD_TEXT(color.red), "ERROR: ");
            break;
        case LOG_WARN:
            fmt::print(BOLD_TEXT(color.yellow), "Warning: ");
            break;
        case LOG_INFO:
            fmt::print(BOLD_TEXT(color.cyan), "Info: ");
            break;
        case LOG_DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(color.magenta), "[DEBUG]: ");
            break;
    }
    fmt::print(ts, fmt::runtime(fmt), std::forward<Args>(args)...);
}

/** Ask the user a yes or no question.
 * @param def the default result
 * @param pr  the prompt enum
 * @param args arguments, required with some prompts, like package names etc.
 * @returns the result, y = true, f = false, only returns def if the result is def
*/
template <typename... Args>
bool askUserYorN(bool def, prompt_yn pr, Args&&... args) {
    string inputs_str = "[" + (string)(def ? "Y" : "y") + "/" + (string)(!def ? "N" : "n") + "] ";
    string result;
   
    switch (pr) {
        case PROMPT_YN_DIFF:
            log_printf(LOG_INFO, BOLD, "View the diffs for {}? {}", std::forward<Args>(args)..., inputs_str);
            if (config->noconfirm) return NO;
            break;
        case PROMPT_YN_CONTINUE_WITHOUT_DIFF:
            log_printf(LOG_WARN, BOLD,
                       "With your current settings, viewing PKGBUILD diffs is unsupported (maybe useGit is false?), continue with the installation anyway? {}",
                           inputs_str);
            if (config->noconfirm) return YES;
            break;
        case PROMPT_YN_EDIT_PKGBUILD:
            log_printf(LOG_INFO, BOLD, "Review PKGBUILD for {}? {}", std::forward<Args>(args)..., inputs_str);
            if (config->noconfirm) return NO;
            break;
        case PROMPT_YN_PROCEED_INSTALL:
            log_printf(LOG_INFO, BOLD, "Proceed with the installation? {}", inputs_str);
            if (config->noconfirm) return YES;
            break;
        case PROMPT_YN_PROCEED_TRANSACTION:
            log_printf(LOG_INFO, BOLD, "Would you like to proceed with this transaction? {}", inputs_str);
            if (config->noconfirm) return NO;
            break;
        default:
            return def;
    }

    //std::cin.sync();
    // while the getline function works, and the result is not 1 character long, keep reminding the user.
    while (std::getline(std::cin, result) && (result.length() > 1))
        log_printf(LOG_WARN, "Please provide a valid response {}", inputs_str);

    ctrl_d_handler();
    
    if (result.empty())
        return def;

    if (def ? tolower(result[0]) != 'n' : tolower(result[0]) != 'y')
        return def;

    return !def;
}

/** Ask the user to select items from a list, input seperated by <separator>.
 * @param required if this prompt can be skipped manually or by noconfirm.
 * @param source the source list for the user to choose from.
 * @param pr  the prompt enum
 * @param separator the separator used when reading the input, default is space.
 * @returns the resulting list, empty if anything bad happens.
*/
template <typename T, typename = std::enable_if_t<is_fmt_convertible_v<T>>>
vector<T> askUserForList(bool required, vector<T> &list, prompt_list pr, char separator = ' ') {
    string sep_str = "Type the index of each package, Seperate with '";

    // I love C++ strings.
    sep_str += separator;
    sep_str += "'";

    string result_str;

    for (size_t i = 0; i < list.size(); i++)
        fmt::println(fmt::fg(color.index), "[{}] {}", i, fmt::format(BOLD | fmt::fg(fmt::color::white), "{}", list[i]));
   
    switch (pr) {
        case PROMPT_LIST_CLEANBUILDS:
            log_printf(LOG_INFO, BOLD, "Pick which queries you'd like to cleanbuild ({}): ", sep_str);
            if (config->noconfirm && !required) return {};
            break;
        case PROMPT_LIST_REVIEWS:
            log_printf(LOG_INFO, BOLD, "Pick which queries you'd like to review before installing ({}): ", sep_str);
            if (config->noconfirm && !required) return {};
            break;
        default:
            return {};
    }

    //std::cin.sync();
    // while the getline function works, and the result is not 1 character long, keep reminding the user.
    vector<T> result;

    while (std::getline(std::cin, result_str)) {
        ctrl_d_handler();

        if (result_str.empty() && !required)
            return {};

        if (result_str == "*")
            return list;
    
        if (!is_numerical(result_str)) {
            log_printf(LOG_WARN, "{}: ", sep_str);
            continue;
        }
        vector<string> input_indices = split(result_str, separator);

        int added_elements = 0;
        for (size_t i = 0; i < input_indices.size(); i++) {
            int index = std::stoi(input_indices[i]);
            if (0 > index || index >= list.size()) {
                log_println(LOG_WARN, "Invalid index! Ignoring index #{}.", input_indices[i]);
                continue;
            }
            result.push_back(list[index]);
            added_elements++;
        }

        if (added_elements == 0) {
            log_printf(LOG_WARN, "{}: ", sep_str);
            continue;
        }

        break;
    }
    
    return result;
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

static inline std::vector<std::string_view> secret = {
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
    1 tablespoon brown sugar)#"},
                                                      {R"#(
    Step 1:
        Preheat the oven to 400 degrees F (200 degrees C).
        Grease a 12-cup muffin tin or line cups with paper liners.)#"},
                                                      {R"#(
    Step 2:
        Combine milk, oil, and egg in a small bowl until well blended. 
        Combine flour, 1/2 cup sugar, baking powder, and salt together in a large bowl, making a well in the center. 
        Pour milk mixture into well and stir until batter is just combined; fold in chocolate chips.)#"},
                                                      {R"#(
    Step 3:
        Spoon batter into the prepared muffin cups, filling each 2/3 full. 
        Combine 1 and 1/2 tablespoons white sugar and 1 tablespoon brown sugar in a small bowl; sprinkle on tops of muffins.)#"},
                                                      {R"#(
    Step 4:
        Bake in the preheated oven until tops spring back when lightly pressed, about 18 to 20 minutes. 
        Cool in the tin briefly, then transfer to a wire rack.
            
            Serve warm or cool completely.
    )#"},
    {"Credits to: https://www.allrecipes.com/recipe/7906/chocolate-chip-muffins/"}};

#endif
