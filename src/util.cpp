#include "util.hpp"
#include "config.hpp"
#include "taur.hpp"
#include <alpm.h>
#include <alpm_list.h>
#include <iostream>
#include <memory>
#include <optional>
#include <memory>

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

/** Print some text after hitting CTRL-C. 
 * Used only in main() for signal()
 */
void interruptHandler(int) {
    log_printf(LOG_WARN, "Caught CTRL-C, Exiting!\n");

    std::exit(-1);
}

/** Function to check if a package is from a synchronization database
 * Basically if it's in pacman repos like core, extra, multilib, etc.
 * @param pkg The package to check
 * @param syncdbs
 * @return true if the pkg exists, else false
 */ 
bool is_package_from_syncdb(alpm_pkg_t *pkg, alpm_list_t *syncdbs) {
    const char *name = alpm_pkg_get_name(pkg);

    for (; syncdbs; syncdbs = alpm_list_next(syncdbs))
        if (alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), name))
            return true;
            
    return false;
}

// soft means it won't return false (or even try) if the list is empty
bool commitTransactionAndRelease(bool soft) {
    alpm_handle_t *handle = config->handle;

    alpm_list_t *addPkgs    = alpm_trans_get_add(handle);
    alpm_list_t *removePkgs = alpm_trans_get_remove(handle);
    alpm_list_t *combined   = alpm_list_join(addPkgs, removePkgs);
    if (soft && !combined)
        return true;

    log_printf(LOG_INFO, "Changes to be made:\n");
    for (alpm_list_t *addPkgsClone = addPkgs; addPkgsClone; addPkgsClone = addPkgsClone->next) {
        log_printf(LOG_NONE, BOLD_TEXT(config->getThemeValue("green", green)), "    ++ ");
        log_printf(LOG_NONE, fmt::emphasis::bold, "{}\n", alpm_pkg_get_name((alpm_pkg_t *)(addPkgsClone->data)));
    }
        

    for (alpm_list_t *removePkgsClone = removePkgs; removePkgsClone; removePkgsClone = removePkgsClone->next) {
        log_printf(LOG_NONE, BOLD_TEXT(config->getThemeValue("red", red)), "    -- ");
        log_printf(LOG_NONE, fmt::emphasis::bold, "{}\n", alpm_pkg_get_name((alpm_pkg_t *)(removePkgsClone->data)));
    }

    log_printf(LOG_NONE, "Would you like to proceed with this transaction? [Y/n] ");
    
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

/** Replace special symbols such as ~ and $ in std::strings
 * @param str The string
 * @return The modified string
 */ 
string expandVar(string& str) {
    const char* env;
    if (str[0] == '~') {
        env = getenv("HOME");
        if (env == nullptr) {
            // it's so bad we need to write the text all red
            log_printf(LOG_ERROR, "{}", fmt::format(fmt::fg(config->getThemeValue("red", red)), "$HOME enviroment variable is not set (how?)"));
            exit(-1);
        }
        str.replace(0, 1, string(env)); // replace ~ with the $HOME value
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
    // why there is a '\n' at the end??
    result.erase(result.end()-1, result.end());
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

/** Executes commands with execvp() and keep the program running without existing
 * @param cmd The command to execute 
 * @return true if the command successed, else false 
 * P.S. it's likely impossible to return false because if the command failed, the program will just exists
 */ 
bool taur_exec(vector<const char*> cmd) {
    cmd.push_back(nullptr);

    int pid = fork();

    if (pid < 0) {
        log_printf(LOG_ERROR, "fork() failed: {}\n", strerror(errno));
        exit(-1);
    }

    if (pid == 0) {
        execvp(cmd[0], const_cast<char* const*>(cmd.data()));
        log_printf(LOG_ERROR, "An error as occured: {}\n", strerror(errno));
        exit(-1);
    } else if (pid > 0) { // we wait for the command to finish then start executing the rest
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
        else {
            log_printf(LOG_ERROR, "Failed to execute the command: ");
            print_vec(cmd); // fmt::join() doesn't work with vector<const char*>
            exit(-1);
        }
    }

    return false;
}

/** Free a list and every single item in it.
 * This will attempt to free everything, should be used for strings that contain C strings only.
 * @param list the list to free
*/
void free_list_and_internals(alpm_list_t *list) {
    for (alpm_list_t *listClone = list; listClone; listClone = listClone->next)
        free(listClone->data);

    alpm_list_free(list);
}

/** Get the database color
 * @param db_name The database name
 * @return database's color in bold
 */
fmt::text_style getColorFromDBName(string db_name) {
    if (db_name == "aur")
        return BOLD_TEXT(config->getThemeValue("blue", blue));
    else if (db_name == "extra")
        return BOLD_TEXT(config->getThemeValue("green", green));
    else if (db_name == "multilib")
        return BOLD_TEXT(config->getThemeValue("cyan", cyan));
    else
        return BOLD_TEXT(config->getThemeValue("yellow", yellow));
}

// Takes a pkg, and index, to show. index is for show and can be set to -1 to hide.
void printPkgInfo(TaurPkg_t &pkg, int index) {
    if (index > -1)
        log_printf(LOG_NONE, fmt::fg(config->getThemeValue("magenta", magenta)), "[{}] ", index);
    
    log_printf(LOG_NONE, getColorFromDBName(pkg.db_name), "{}/", pkg.db_name);
    log_printf(LOG_NONE, fmt::emphasis::bold, "{} ", pkg.name);
    log_printf(LOG_NONE, BOLD_TEXT(config->getThemeValue("green", green)), "{} ", pkg.version);
    log_printf(LOG_NONE, fmt::fg(config->getThemeValue("cyan", cyan)), " Popularity: {} ({})\n", pkg.popularity, getTitleForPopularity(pkg.popularity));
    log_printf(LOG_NONE, "    {}\n", pkg.desc);
}

/** Ask the user to select a package out of a list.
 * @param pkgs The list of packages, in a vector
 * @param backend A reference to the TaurBackend responsible for fetching any AUR packages.
 * @param useGit Whether the fetched pkg should use a .git url
 * @return Optional TaurPkg_t, will not return if interrupted.
*/
optional<TaurPkg_t> askUserForPkg(vector<TaurPkg_t> pkgs, TaurBackend& backend, bool useGit) {
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
                printPkgInfo(pkgs[i], i);

            log_printf(LOG_NONE, "Choose a package to download: ");
            std::cin >> input;
        } while (!is_number(input) || (size_t)std::stoi(input) >= pkgs.size());

        size_t selected = std::stoi(input);

        return pkgs[selected].url.empty() ? pkgs[selected] : backend.fetch_pkg(pkgs[selected].name, useGit).value_or(pkgs[selected]);
    }

    return {};
}

/** Filters out/only AUR packages.
 * Default behavior is filtering out.
 * @param pkgs a unique_ptr to a list of packages to filter.
 * @param inverse a bool that, if true, will return only AUR packages instead of the other way around.
 * @return an optional unique_ptr to a result.
*/
optional<alpm_list_smart_pointer> filterAURPkgs(alpm_list_t *pkgs, alpm_list_t *syncdbs, bool inverse) {
    size_t pkgsSize = alpm_list_count(pkgs);

    alpm_list_smart_pointer sync_pkgs(nullptr, alpm_list_free);

    for (; syncdbs; syncdbs = syncdbs->next) {
        for (size_t i = 0; i < pkgsSize; i++) {
            alpm_pkg_t *pkg = alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), alpm_pkg_get_name((alpm_pkg_t *)(alpm_list_nth(pkgs, i)->data)));
            if (pkg) {
                alpm_list_t *sync_pkgs_get = sync_pkgs.get();
                (void)sync_pkgs.release();
                sync_pkgs = make_list_smart_pointer(alpm_list_add(sync_pkgs_get, (void *)(pkg)));
            }
        }
    }

    alpm_list_smart_pointer out(nullptr, alpm_list_free);

    if (inverse) {
        for (alpm_list_t *pkgsClone = pkgs; pkgsClone; pkgsClone = sync_pkgs->next) {
            // it couldn't find the sync pkg in the pkgs list, AUR package.
            if (alpm_list_find_ptr(pkgs, (alpm_pkg_t *)(sync_pkgs->data)) == nullptr) {
                alpm_list_t *out_get = out.get();
                (void)out.release();
                out = make_list_smart_pointer(alpm_list_add(out_get, (alpm_pkg_t *)(sync_pkgs->data)));
            }
        }
    } else {
        // can't move ownership for some reason, just copy the data, both'll get deleted anyway.
        out.swap(sync_pkgs);
    }

    return out;
}

string getTitleForPopularity(float popularity) {
    if (popularity < 0.2)
        return "Untrustable";
    if (popularity < 0.8)
        return "Caution";
    if (popularity < 10)
        return "Normal";
    if (popularity < 20)
        return "Good";
    if (popularity >= 20)
        return "Trustable";
    return "Weird, report this bug.";
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
