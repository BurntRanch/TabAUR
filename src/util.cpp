#include "util.hpp"
#include "config.hpp"
#include "taur.hpp"

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

void log_printf(int log, string fmt, ...) {
    va_list args;
    va_start(args, fmt);
    switch(log) {
        case LOG_ERROR:
            std::cerr << BOLDRED << "====[ ERROR ]====" << std::endl << BOLD; break;
        case LOG_WARN:
            std::cout << BOLDYELLOW << "Warning: " << BOLD; break;
        case LOG_INFO:
            std::cout << BOLDBLUE << "Info: " << BOLD; break;
        case LOG_DEBUG:
            if (config->debug)
                std::cout << BOLDMAGENTA << "[DEBUG]: " << BOLD;
            else
                return;
    }
    vprintf(fmt.c_str(), args);
    va_end(args);
    // just automatically new line because it's useless to \n each time
    std::cout << NOCOLOR << std::endl;
}

bool isInvalid(char c) {
    return !isprint(c);
}

void sanitizeStr(string& str){
    str.erase(std::remove_if(str.begin(), str.end(), isInvalid), str.end());
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

    log_printf(LOG_INFO, "Changes to be made:");
    for (alpm_list_t *addPkgsClone = addPkgs; addPkgsClone; addPkgsClone = addPkgsClone->next)
        std::cout << "    " << BOLDGREEN << "++ " << NOCOLOR << alpm_pkg_get_name((alpm_pkg_t *)(addPkgsClone->data)) << std::endl;

    for (alpm_list_t *removePkgsClone = removePkgs; removePkgsClone; removePkgsClone = removePkgsClone->next)
        std::cout << "    " << BOLDRED << "-- " << NOCOLOR << alpm_pkg_get_name((alpm_pkg_t *)(removePkgsClone->data)) << std::endl;

    std::cout << "Would you like to proceed with this transaction? [Y/n] ";
    
    string response;
    std::cin >> response;

    for (char &c : response) { 
        c = tolower(c); 
    }
    
    if (!response.empty() && response != "y") {
        bool releaseStatus = alpm_trans_release(handle) == 0;
        if (!releaseStatus)
            log_printf(LOG_ERROR, "Failed to release transaction (%s).", alpm_strerror(alpm_errno(handle)));

        log_printf(LOG_INFO, "Cancelled transaction.");
        return soft;
    }

    bool prepareStatus = alpm_trans_prepare(handle, &combined) == 0;
    if (!prepareStatus)
        log_printf(LOG_ERROR, "Failed to prepare transaction (%s).", alpm_strerror(alpm_errno(handle)));

    bool commitStatus = alpm_trans_commit(handle, &combined) == 0;
    if (!commitStatus)
        log_printf(LOG_ERROR, "Failed to commit transaction (%s).", alpm_strerror(alpm_errno(handle)));

    bool releaseStatus = alpm_trans_release(handle) == 0;
    if (!releaseStatus)
        log_printf(LOG_ERROR, "Failed to release transaction (%s).", alpm_strerror(alpm_errno(handle)));


    if (prepareStatus && commitStatus && releaseStatus) {
        log_printf(LOG_INFO, "Successfully finished transaction.");
        return true;
    }

    return false;
}
     
std::string expandVar(std::string& str) {
    const char* env;
    if (str[0] == '~') {
        env = getenv("HOME");   // it's likely impossible for not having the $HOME env var setup
        str.replace(0, 1, env); // replace ~ with the $HOME value
    } else if (str[0] == '$') {
        str.erase(0, 1); // erase from str[0] to str[1]
        env = getenv(str.c_str());
        if (env == nullptr) {
            log_printf(LOG_ERROR, "No such enviroment variable: %s", str.c_str());
            exit(-1);
        }
        str = std::string(env);
    }

    return str;
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
        log_printf(LOG_ERROR, "pipe() failed: %s", strerror(errno));
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

        log_printf(LOG_ERROR, "An error has occurred: %s", strerror(errno));
        exit(127);
    } else {
        log_printf(LOG_ERROR, "fork() failed: %s", strerror(errno));

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
        log_printf(LOG_ERROR, "fork() failed: %s", strerror(errno));
        exit(127);
    }

    if (pid == 0) {
        execvp(cmd[0], const_cast<char* const*>(cmd.data()));

        log_printf(LOG_ERROR, "An error has occurred: %s", strerror(errno));
        exit(127);
    } else if (pid > 0) { // we wait for the command to finish then start executing the rest
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
        else {
            std::cerr << BOLDRED << "====[ ERROR ]====" << std::endl << BOLD << "Failed to execute the command: " << NOCOLOR;
            for(auto& i : cmd)
                std::cout << i << " ";
            std::cout << NOCOLOR << std::endl;
            exit(127);
        }
    }

    return false;
}

std::vector<string> split(std::string text, char delim) {
    string              line;
    std::vector<string> vec;
    std::stringstream   ss(text);
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}
