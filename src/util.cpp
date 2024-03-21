#include <cstdio>
#include "util.hpp"

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

// Function to check if a package is from a synchronization database
bool is_package_from_syncdb(alpm_pkg_t *pkg, alpm_list_t *syncdbs) {
    const char *name = alpm_pkg_get_name(pkg);

    for (; syncdbs; syncdbs = alpm_list_next(syncdbs))
        for (alpm_list_t *p = alpm_db_get_pkgcache((alpm_db_t *)(syncdbs->data)); p; p = alpm_list_next(p))
            if (strcmp(name, alpm_pkg_get_name((alpm_pkg_t *)(p->data))) == 0)
                return true;
    return false;
}

// soft means it won't return false (or even try) if the list is empty
bool commitTransactionAndRelease(alpm_handle_t *handle, bool soft) {
    alpm_list_t *addPkgs    = alpm_trans_get_add(handle);
    alpm_list_t *removePkgs = alpm_trans_get_remove(handle);
    alpm_list_t *combined   = alpm_list_join(addPkgs, removePkgs);
    if (soft && !combined)
        return true;
    bool prepareStatus = alpm_trans_prepare(handle, &combined) == 0;
    bool commitStatus = alpm_trans_commit(handle, &combined) == 0;
    bool releaseStatus = alpm_trans_release(handle) == 0;
    return prepareStatus && commitStatus && releaseStatus;
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

