#include <cstdio>
#include <util.hpp>

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(string const& fullString, std::string const& ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

bool hasStart(string const& fullString, std::string const& start) {
    if (start.length() > fullString.length())
        return false;
    return (0 == fullString.compare(0, start.length(), start));
}

void log_printf(int log, string fmt, ...){
    va_list args;
    va_start(args, fmt);
    switch(log){
        case LOG_ERROR:
            std::cerr << BOLDRED << "====[ ERROR ]==== " << std::endl << NOCOLOR << BOLD; break;
        case LOG_WARN:
            std::cout << BOLDYELLOW << "Warning: " << NOCOLOR << BOLD; break;
        case LOG_INFO:
            std::cout << BOLDBLUE << "Info: " << NOCOLOR << BOLD; break;
    }
    vprintf(fmt.c_str(), args);
    va_end(args);
    std::cout << NOCOLOR;
}

void sanitizeStr(string& str){
    str.erase(sanitize(str.begin(), str.end()), str.end());
}

string expandHome(std::string& str) {
    string ret = str;
    size_t found = ret.find("~");
    if (found != string::npos) {
        const char* homeDir = getenv("HOME");
        if (homeDir != nullptr)
            ret.replace(found, 1, homeDir);
        else {
            log_printf(LOG_ERROR, _("HOME environment variable is not set.\n"));
            exit(-1);
        }
    }
    return ret;
}

std::vector<string> split(std::string text, char delim) {
    string line;
    std::vector<string> vec;
    std::stringstream ss(text);
    while(std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}

