#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdarg>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#define _(str) (char *)str

enum log_level {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO
};

template<typename T>
struct is_string {
    static constexpr bool value = std::is_same_v<T, std::string> || std::is_convertible_v<T, std::string>;
};

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(std::string const& fullString, std::string const& ending);
bool hasStart(std::string const& fullString, std::string const& start);
void log_printf(int log, std::string fmt, ...);
std::string expandHome(std::string& str);

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

std::vector<std::string> split(std::string text, char delim);

#endif
