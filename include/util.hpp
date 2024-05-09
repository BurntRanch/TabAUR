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

#ifdef ENABLE_NLS
/* here so it doesn't need to be included elsewhere */
#include <libintl.h> 
#include <locale.h>
/* define _() as shortcut for gettext() */
#define _(str) gettext(str)
#else
#define _(s) (char *)s
#endif

struct TaurPkg_t;
class TaurBackend;

#define BOLD           fmt::emphasis::bold
#define BOLD_TEXT(x)   (fmt::emphasis::bold | fmt::fg(x))
#define NOCOLOR        "\033[0m"
#define AUR_URL        "https://aur.archlinux.org"
#define AUR_URL_GIT(x) fmt::format("https://aur.archlinux.org/{}.git", x)
#define AUR_URL_TAR(x) fmt::format("https://aur.archlinux.org/cgit/aur.git/snapshot/{}.tar.gz", x)

#define alpm_list_smart_pointer          unique_ptr<alpm_list_t, decltype(&alpm_list_free)>
#define make_list_smart_pointer(pointer) (unique_ptr<alpm_list_t, decltype(&alpm_list_free)>(pointer, alpm_list_free))

#define alpm_list_smart_deleter          unique_ptr<alpm_list_t, decltype(&free_list_and_internals)>
#define make_list_smart_deleter(pointer) (unique_ptr<alpm_list_t, decltype(&free_list_and_internals)>(pointer, free_list_and_internals))

#define NOCONFIRMREQ(x)                                                                                                                                                            \
    if (config->noconfirm && !required) {                                                                                                                                          \
        fmt::print("\n");                                                                                                                                                          \
        return x;                                                                                                                                                                  \
    }
#define NOCONFIRM(x)                                                                                                                                                               \
    if (config->noconfirm) {                                                                                                                                                       \
        fmt::print("\n");                                                                                                                                                          \
        return x;                                                                                                                                                                  \
    }

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
    PROMPT_LIST_REMOVE_PKGS,
};

enum {
    YES = 1,
    NO  = 0,
};

enum log_level {
    ERROR,
    WARN,
    INFO,
    DEBUG,
    NONE // display no prefix for this.
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
void                             printPkgInfo(TaurPkg_t& pkg, string_view db_name);
void                             printLocalFullPkgInfo(alpm_pkg_t *pkg);
string                           makepkg_list(string_view pkg_name, string path);
void                             free_list_and_internals(alpm_list_t *list);
fmt::text_style                  getColorFromDBName(string_view db_name);
vector<alpm_pkg_t *>             filterAURPkgs(vector<alpm_pkg_t *> pkgs, alpm_list_t *syncdbs, bool inverse);
vector<string_view>              filterAURPkgsNames(vector<string_view> pkgs, alpm_list_t *syncdbs, bool inverse);
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
void log_println(log_level log, const fmt::text_style ts, fmt::runtime_format_string<> fmt, Args&&... args) {
    switch (log) {
        case ERROR:
            fmt::print(BOLD_TEXT(color.red), "ERROR: ");
            break;
        case WARN:
            fmt::print(BOLD_TEXT(color.yellow), "Warning: ");
            break;
        case INFO:
            fmt::print(BOLD_TEXT(color.cyan), "Info: ");
            break;
        case DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(color.magenta), "[DEBUG]: ");
            break;
        default:
            break;
    }
    fmt::println(ts, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_println(log_level log, fmt::runtime_format_string<> fmt, Args&&... args) {
    log_println(log, fmt::text_style(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_println(log_level log, string_view fmt, Args&&... args) {
    log_println(log, fmt::text_style(), fmt::runtime(fmt), std::forward<Args>(args)...);
}

template <typename... Args>
void log_println(log_level log, const fmt::text_style ts, string_view fmt, Args&&... args) {
    log_println(log, ts, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(log_level log, const fmt::text_style ts, fmt::runtime_format_string<> fmt, Args&&... args) {
    switch (log) {
        case ERROR:
            fmt::print(BOLD_TEXT(color.red), "ERROR: ");
            break;
        case WARN:
            fmt::print(BOLD_TEXT(color.yellow), "Warning: ");
            break;
        case INFO:
            fmt::print(BOLD_TEXT(color.cyan), "Info: ");
            break;
        case DEBUG:
            if (!config->debug)
                return;
            fmt::print(BOLD_TEXT(color.magenta), "[DEBUG]: ");
            break;
        default:
            break;
    }
    fmt::print(ts, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(log_level log, fmt::runtime_format_string<> fmt, Args&&... args) {
    log_printf(log, fmt::text_style(), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(log_level log, string_view fmt, Args&&... args) {
    log_printf(log, fmt::text_style(), fmt::runtime(fmt), std::forward<Args>(args)...);
}

template <typename... Args>
void log_printf(log_level log, const fmt::text_style ts, string_view fmt, Args&&... args) {
    log_printf(log, ts, fmt, std::forward<Args>(args)...);
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
            log_printf(INFO, BOLD, "View the diffs for {}? {}", std::forward<Args>(args)..., inputs_str);
            NOCONFIRM(NO);
            break;
        case PROMPT_YN_CONTINUE_WITHOUT_DIFF:
            log_printf(WARN, BOLD, "With your current settings, viewing PKGBUILD diffs is unsupported (maybe useGit is false?), continue with the installation anyway? {}",
                       inputs_str);
            NOCONFIRM(YES);
            break;
        case PROMPT_YN_EDIT_PKGBUILD:
            log_printf(INFO, BOLD, "Review PKGBUILD for {}? {}", std::forward<Args>(args)..., inputs_str);
            NOCONFIRM(NO);
            break;
        case PROMPT_YN_PROCEED_INSTALL:
            log_printf(INFO, BOLD, "Proceed with the installation? {}", inputs_str);
            NOCONFIRM(YES);
            break;
        case PROMPT_YN_PROCEED_TRANSACTION:
            log_printf(INFO, BOLD, "Would you like to proceed with this transaction? {}", inputs_str);
            NOCONFIRM(YES);
            break;
        default:
            return def;
    }

    //std::cin.sync();
    // while the getline function works, and the result is not 1 character long, keep reminding the user.
    while (std::getline(std::cin, result) && (result.length() > 1))
        log_printf(WARN, "Please provide a valid response {}", inputs_str);

    ctrl_d_handler();

    if (result.empty())
        return def;

    if (def ? tolower(result[0]) != 'n' : tolower(result[0]) != 'y')
        return def;

    return !def;
}

/** Ask the user to select items from a list, input seperated by <separator>.
 * @param source the source list for the user to choose from.
 * @param pr  the prompt enum
 * @param required if this prompt can be skipped manually or by noconfirm.
 * @returns the resulting list, empty if anything bad happens.
*/
template <typename T, typename = std::enable_if_t<is_fmt_convertible_v<T>>>
vector<T> askUserForList(vector<T>& list, prompt_list pr, bool required = false) {

    string sep_str = "Type the index of each package (eg: \"0 1 2\", \"0-2\", \"a\" for all, \"n\" or enter for none)";
    string result_str;

    for (size_t i = 0; i < list.size(); i++)
        fmt::println(fmt::fg(color.index), "[{}] {}", i, fmt::format(BOLD_TEXT(fmt::color::white), "{}", list[i]));

    log_println(INFO, "{}", sep_str);

    switch (pr) {
        case PROMPT_LIST_CLEANBUILDS:
            log_printf(INFO, BOLD, "Packages to completely rebuild: ");
            NOCONFIRMREQ({});
            break;
        case PROMPT_LIST_REVIEWS:
            log_printf(INFO, BOLD, "Packages you'd like to review: ");
            NOCONFIRMREQ({});
            break;
        case PROMPT_LIST_REMOVE_PKGS:
            log_printf(INFO, BOLD, "Packages you'd like to remove: ");
            NOCONFIRMREQ({});
            break;
        default:
            return {};
    }

    //std::cin.sync();
    // while the getline function works, and the result is not 1 character long, keep reminding the user.
    vector<T> result;

    while (std::getline(std::cin, result_str)) {

        if ((result_str.empty() && !required) || result_str == "n") {
            std::cout << std::endl;
            return {};
        }

        if (result_str == "a")
            return list;

        vector<string> input_indices = split(result_str, ' ');

        int added_elements = 0;
        bool breakandcontinue = false;
        for (size_t i = 0; i < input_indices.size() && !breakandcontinue; i++) {
            // 1-5 means 1 through 5
            if (input_indices[i].find('-') != string::npos) {
                vector<string> loop_bounds = split(input_indices[i], '-');
                if (loop_bounds.size() != 2 || !is_numerical(loop_bounds[0]) || !is_numerical(loop_bounds[1])) {
                    log_printf(WARN, "Invalid loop range! (loop ranges look like \"0-5\"): ");
                    breakandcontinue = true;
                    break;
                }

                int lowerbound  = std::stoi(loop_bounds[0]);
                int higherbound = std::stoi(loop_bounds[1]);

                if ((0 > lowerbound || lowerbound > list.size()) || (lowerbound > higherbound || higherbound >= list.size())) {
                    log_printf(WARN, "Invalid loop range! (loop ranges must stay in bounds and in order): ");
                    breakandcontinue = true;
                    break;
                }

                for (int i = lowerbound; i <= higherbound; i++) {
                    result.push_back(list[i]);
                    added_elements++;
                }

                continue;
            }

            if (!is_numerical(input_indices[i])) {
                log_printf(WARN, "{}: ", sep_str);
                continue;
            }

            int index = std::stoi(input_indices[i]);
            if (0 > index || index >= list.size()) {
                log_println(WARN, "Invalid index! Ignoring index #{}.", input_indices[i]);
                continue;
            }
            result.push_back(list[index]);
            added_elements++;
        }

        if (breakandcontinue)
            continue;

        if (added_elements == 0) {
            log_printf(WARN, "{}: ", sep_str);
            continue;
        }

        break;
    }
    ctrl_d_handler();

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
