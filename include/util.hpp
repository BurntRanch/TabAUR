#ifndef UTIL_HPP
#define UTIL_HPP

#include <cstdarg>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstring>
#include <alpm.h>

using std::string;
// taken from pacman
#define _(str) (char *)str

// original on https://gitlab.archlinux.org/pacman/pacman/-/blob/master/src/pacman/conf.c#L45 
#define NOCOLOR       "\033[0m"

#define BOLD          "\033[0;1m"

#define BLACK         "\033[0;30m"
#define RED           "\033[0;31m"
#define GREEN         "\033[0;32m"
#define YELLOW        "\033[0;33m"
#define BLUE          "\033[0;34m"
#define MAGENTA       "\033[0;35m"
#define CYAN          "\033[0;36m"
#define WHITE         "\033[0;37m"

#define BOLDBLACK     "\033[1;30m"
#define BOLDRED       "\033[1;31m"
#define BOLDGREEN     "\033[1;32m"
#define BOLDYELLOW    "\033[1;33m"
#define BOLDBLUE      "\033[1;34m"
#define BOLDMAGENTA   "\033[1;35m"
#define BOLDCYAN      "\033[1;36m"
#define BOLDWHITE     "\033[1;37m"
#define GREY46        "\033[38;5;243m"

enum log_level {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO
};

template<typename T>
struct is_string {
    static constexpr bool value = std::is_same_v<T, string> || std::is_convertible_v<T, string>;
};

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(string const& fullString, string const& ending);
bool hasStart(string const& fullString, string const& start);
void log_printf(int log, string fmt, ...);
string expandHome(string& str);
void sanitizeStr(string& str);
bool is_package_from_syncdb(alpm_pkg_t *pkg, alpm_list_t *syncdbs);
std::vector<string> split(string text, char delim);

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

#endif
