#include "util.hpp"
#include "config.hpp"
#include "fmt/color.h"
#include "taur.hpp"
#include <alpm.h>

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(string const& fullString, string const& ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

bool hasStart(string const& fullString, string const& start) {
    if (start.length() > fullString.length())
        return false;
    return (0 == fullString.compare(0, start.length(), start));
}

bool isInvalid(char c) {
    return !isprint(c);
}

void sanitizeStr(string& str){
    str.erase(std::remove_if(str.begin(), str.end(), isInvalid), str.end());
}

void interruptHandler(int) {
    log_printf(LOG_WARN, "Caught CTRL-C, Exiting!\n");

    std::exit(-1);
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
bool commitTransactionAndRelease(Config &cfg, bool soft) {
    alpm_handle_t *handle = cfg.handle;

    alpm_list_t *addPkgs    = alpm_trans_get_add(handle);
    alpm_list_t *removePkgs = alpm_trans_get_remove(handle);
    alpm_list_t *combined   = alpm_list_join(addPkgs, removePkgs);
    if (soft && !combined)
        return true;

    log_printf(LOG_INFO, "Changes to be made:\n");
    for (alpm_list_t *addPkgsClone = addPkgs; addPkgsClone; addPkgsClone = addPkgsClone->next) {
        fmt::print(fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("green", "#00aa00")), "    ++ ");
        fmt::print(fmt::emphasis::bold, "{}\n", alpm_pkg_get_name((alpm_pkg_t *)(addPkgsClone->data)));
    }
        

    for (alpm_list_t *removePkgsClone = removePkgs; removePkgsClone; removePkgsClone = removePkgsClone->next) {
        fmt::print(fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("red", "#aa0000")), "    -- ");
        fmt::print(fmt::emphasis::bold, "{}\n", alpm_pkg_get_name((alpm_pkg_t *)(removePkgsClone->data)));
    }

    fmt::print("Would you like to proceed with this transaction? [Y/n] ");
    
    string response;
    std::cin >> response;

    if(!std::cin) // CTRL-D
        return false;

    for (char &c : response) { 
        c = tolower(c); 
    }
    
    if (!response.empty() && response != "y") {
        bool releaseStatus = alpm_trans_release(handle) == 0;
        if (!releaseStatus)
            log_printf(LOG_ERROR, "Failed to release transaction ({}).\n", alpm_strerror(alpm_errno(handle)));

        log_printf(LOG_INFO, "Cancelled transaction.\n");
        return soft;
    }

    bool prepareStatus = alpm_trans_prepare(handle, &combined) == 0;
    if (!prepareStatus)
        log_printf(LOG_ERROR, "Failed to prepare transaction ({}).\n", alpm_strerror(alpm_errno(handle)));

    bool commitStatus = alpm_trans_commit(handle, &combined) == 0;
    if (!commitStatus)
        log_printf(LOG_ERROR, "Failed to commit transaction ({}).\n", alpm_strerror(alpm_errno(handle)));

    bool releaseStatus = alpm_trans_release(handle) == 0;
    if (!releaseStatus)
        log_printf(LOG_ERROR, "Failed to release transaction ({}).\n", alpm_strerror(alpm_errno(handle)));


    if (prepareStatus && commitStatus && releaseStatus) {
        log_printf(LOG_INFO, "Successfully finished transaction.\n");
        return true;
    }

    return false;
}

string expandVar(string& str) {
    const char* env;
    if (str[0] == '~') {
        env = getenv("HOME");   // it's likely impossible for not having the $HOME env var setup
        str.replace(0, 1, env); // replace ~ with the $HOME value
    } else if (str[0] == '$') {
        str.erase(0, 1); // erase from str[0] to str[1]
        env = getenv(str.c_str());
        if (env == nullptr) {
            log_printf(LOG_ERROR, "No such enviroment variable: {}\n", str);
            exit(-1);
        }
        str = string(env);
    }

    return str;
}

fmt::rgb hexStringToColor(string hexstr) {
    hexstr = hexstr.substr(1);
    // convert the hexadecimal string to individual components
    std::stringstream ss;
    ss << std::hex << hexstr;

    int intValue;
    ss >> intValue;

    int red = (intValue >> 16) & 0xFF;
    int green = (intValue >> 8) & 0xFF;
    int blue = intValue & 0xFF;
    
    return fmt::rgb(red, green, blue);
}

// http://stackoverflow.com/questions/478898/ddg#478960
string shell_exec(string cmd) {
    std::array<char, 128> buffer;
    string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // why there is a \n ??
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}

// https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c#4654718
bool is_number(const string& s) {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

bool taur_read_exec(vector<const char*> cmd, string *output) {
    cmd.push_back(nullptr);
    int pipeout[2];

    if (pipe(pipeout) < 0) {
        log_printf(LOG_ERROR, "pipe() failed: {}\n", strerror(errno));
        exit(127);
    }

    int pid = fork();
    
    if (pid > 0) { // we wait for the command to finish then start executing the rest
        close(pipeout[1]);

        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // read stdout
            if (output) {
                char c;
                while (read(pipeout[0], &c, 1) == 1) {
                    (*output) += c;
                }
            }

            close(pipeout[0]);

            return true;
        }
    } else if (pid == 0) {
        if (dup2(pipeout[1], STDOUT_FILENO) == -1)
            exit(127);
        
        close(pipeout[0]);
        close(pipeout[1]);

        execvp(cmd[0], const_cast<char* const*>(cmd.data()));

        log_printf(LOG_ERROR, "An error has occurred: {}\n", strerror(errno));
        exit(127);
    } else {
        log_printf(LOG_ERROR, "fork() failed: {}\n", strerror(errno));

        close(pipeout[0]);
        close(pipeout[1]);

        exit(127);
    }

    close(pipeout[0]);
    close(pipeout[1]);

    return false;
}

bool taur_exec(vector<const char*> cmd) {
    cmd.push_back(nullptr);

    int pid = fork();

    if (pid < 0) {
        log_printf(LOG_ERROR, "fork() failed: {}\n", strerror(errno));
        exit(127);
    }

    if (pid == 0) {
        execvp(cmd[0], const_cast<char* const*>(cmd.data()));
        log_printf(LOG_ERROR, "An error as occured: {}\n", strerror(errno));
        exit(127);
    } else if (pid > 0) { // we wait for the command to finish then start executing the rest
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
        else {
            log_printf(LOG_ERROR, "Failed to execute the command: ");
            print_vec(cmd); // fmt::join() doesn't work with vector<const char*>
            exit(127);
        }
    }

    return false;
}

fmt::text_style getColorFromDBName(string db_name, Config &cfg) {
    if (db_name == "aur")
        return fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("blue", "#0000aa"));
    else if (db_name == "extra")
        return fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("green", "#00aa00"));
    else if (db_name == "multilib")
        return fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("cyan", "#004499"));
    else
        return fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("yellow", "#aa8a00"));
}

// Takes a pkg, and index, to show. index is for show and can be set to -1 to hide.
void printPkgInfo(TaurPkg_t pkg, Config &cfg, int index) {
    if (index > -1)
        fmt::print(fmt::fg(cfg.getThemeValue("magenta", "#aa00aa")), "[{}] ", index);
    fmt::print(getColorFromDBName(pkg.db_name, cfg), "{}/", pkg.db_name);
    fmt::print(fmt::emphasis::bold, "{} ", pkg.name);
    fmt::print(fmt::emphasis::bold | fmt::fg(cfg.getThemeValue("green", "#00aa00")), "{}\n", pkg.version);
    fmt::println("    {}", pkg.desc);
}

optional<TaurPkg_t> askUserForPkg(vector<TaurPkg_t> pkgs, TaurBackend& backend, Config &cfg, bool useGit) {
    if (pkgs.size() == 1) {
        return pkgs[0].url.empty() ? pkgs[0] : backend.fetch_pkg(pkgs[0].name, useGit).value_or(pkgs[0]);
    } else if (pkgs.size() > 1) {
        log_printf(LOG_INFO, "TabAUR has found multiple packages relating to your search query, Please pick one.\n");
        string input;
        do {
            // CTRL-D
            if (!std::cin) {
                log_printf(LOG_WARN, "Exiting due to CTRL-D!\n");
                return {};
            }

            if (!input.empty())
                log_printf(LOG_WARN, "Invalid input!\n");

            for (size_t i = 0; i < pkgs.size(); i++)
                printPkgInfo(pkgs[i], cfg, i);

            fmt::print("Choose a package to download: ");
            std::cin >> input;
        } while (!is_number(input) || (size_t)std::stoi(input) >= pkgs.size());

        size_t selected = std::stoi(input);

        return pkgs[selected].url.empty() ? pkgs[selected] : backend.fetch_pkg(pkgs[selected].name, useGit).value_or(pkgs[selected]);
    }

    return {};
}

std::vector<string> split(string text, char delim) {
    string              line;
    std::vector<string> vec;
    std::stringstream   ss(text);
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}
