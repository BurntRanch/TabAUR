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
    LOG_WARN
};

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(std::string const& fullString, std::string const& ending);
bool hasStart(std::string const& fullString, std::string const& start);
void log_printf(int log, std::string fmt, ...);
std::vector<std::string> split(std::string text, char delim);

#endif
