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

string sanitizeStr(string& str){
    str.erase(sanitize(str.begin(), str.end()), str.end());
    return str;
}

std::string expandVar(std::string& str){
    const char* env;
    if(str[0] == '~'){
        env = getenv("HOME"); // it's likely impossible for not having the $HOME env var setup 
        str.replace(0, 1, env); // replace ~ with the $HOME value
    } else if (str[0] == '$') {
        str.erase(0, 1); // erase from str[0] to str[1]
        env = getenv(str.c_str());
        if (env == nullptr) {
            log_printf(LOG_ERROR, _("No such enviroment variable: %s\n"), str.c_str());
            exit(-1);
        } 
        str = std::string(env);
    }
    
    return str;
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

