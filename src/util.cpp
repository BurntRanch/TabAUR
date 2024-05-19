#pragma GCC diagnostic ignored "-Wignored-attributes"

#include "util.hpp"
#include "config.hpp"
#include "pacman.hpp"
#include "taur.hpp"

/** Build the `titles` array of localized titles and pad them with spaces so
 * that they align with the longest title. Storage for strings is stack
 * allocated and naively truncated to TITLE_MAXLEN characters.
 */
// function from pacman code for aligning colonums
static void make_aligned_titles(void) {
    unsigned int         i;
    size_t               maxlen                                                          = 0;
    int                  maxcol                                                          = 0;
    static const wchar_t title_suffix[]                                                  = L" :";
    wchar_t              wbuf[ARRAYSIZE(titles)][TITLE_MAXLEN + ARRAYSIZE(title_suffix)] = {{0}};
    size_t               wlen[ARRAYSIZE(wbuf)];
    int                  wcol[ARRAYSIZE(wbuf)];
    char                *buf[ARRAYSIZE(wbuf)];
    buf[T_ARCHITECTURE]    = _("Architecture");
    buf[T_BACKUP_FILES]    = _("Backup Files");
    buf[T_BUILD_DATE]      = _("Build Date");
    buf[T_COMPRESSED_SIZE] = _("Compressed Size");
    buf[T_CONFLICTS_WITH]  = _("Conflicts With");
    buf[T_DEPENDS_ON]      = _("Depends On");
    buf[T_DESCRIPTION]     = _("Description");
    buf[T_DOWNLOAD_SIZE]   = _("Download Size");
    buf[T_GROUPS]          = _("Groups");
    buf[T_INSTALL_DATE]    = _("Install Date");
    buf[T_INSTALL_REASON]  = _("Install Reason");
    buf[T_INSTALL_SCRIPT]  = _("Install Script");
    buf[T_INSTALLED_SIZE]  = _("Installed Size");
    buf[T_LICENSES]        = _("Licenses");
    buf[T_MD5_SUM]         = _("MD5 Sum");
    buf[T_NAME]            = _("Name");
    buf[T_OPTIONAL_DEPS]   = _("Optional Deps");
    buf[T_OPTIONAL_FOR]    = _("Optional For");
    buf[T_PACKAGER]        = _("Packager");
    buf[T_PROVIDES]        = _("Provides");
    buf[T_REPLACES]        = _("Replaces");
    buf[T_REPOSITORY]      = _("Repository");
    buf[T_REQUIRED_BY]     = _("Required By");
    buf[T_SHA_256_SUM]     = _("SHA-256 Sum");
    buf[T_SIGNATURES]      = _("Signatures");
    buf[T_URL]             = _("URL");
    buf[T_VALIDATED_BY]    = _("Validated By");
    buf[T_VERSION]         = _("Version");

    for (i = 0; i < ARRAYSIZE(wbuf); i++) {
        wlen[i] = mbstowcs(wbuf[i], buf[i], strlen(buf[i]) + 1);
        wcol[i] = wcswidth(wbuf[i], wlen[i]);
        if (wcol[i] > maxcol) {
            maxcol = wcol[i];
        }
        if (wlen[i] > maxlen) {
            maxlen = wlen[i];
        }
    }

    for (i = 0; i < ARRAYSIZE(wbuf); i++) {
        size_t padlen = maxcol - wcol[i];
        wmemset(wbuf[i] + wlen[i], L' ', padlen);
        wmemcpy(wbuf[i] + wlen[i] + padlen, title_suffix, ARRAYSIZE(title_suffix));
        wcstombs(titles[i], wbuf[i], sizeof(wbuf[i]));
    }
}

/** Turn a optdepends list into a text list.
 * @param optdeps a list with items of type alpm_depend_t
 */
static void optdeplist_display(alpm_pkg_t *pkg, unsigned short cols = getcols()) {
    alpm_list_t *i, *text = NULL;
    alpm_db_t   *localdb = alpm_get_localdb(config->handle);
    for (i = alpm_pkg_get_optdepends(pkg); i; i = alpm_list_next(i)) {
        alpm_depend_t *optdep    = (alpm_depend_t *)i->data;
        char          *depstring = alpm_dep_compute_string(optdep);
        if (alpm_pkg_get_origin(pkg) == ALPM_PKG_FROM_LOCALDB) {
            if (alpm_find_satisfier(alpm_db_get_pkgcache(localdb), depstring)) {
                const char *installed = _(" [installed]");
                depstring             = (char *)realloc(depstring, strlen(depstring) + strlen(installed) + 1);
                strcpy(depstring + strlen(depstring), installed);
            }
        }
        text = alpm_list_add(text, depstring);
    }
    list_display_linebreak(titles[T_OPTIONAL_DEPS], text, cols);
    FREELIST(text);
}

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(string_view fullString, string_view ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

bool hasStart(string_view fullString, string_view start) {
    if (start.length() > fullString.length())
        return false;
    return (0 == fullString.compare(0, start.length(), start));
}

bool isInvalid(char c) {
    return !isprint(c);
}

void sanitizeStr(string& str) {
    str.erase(std::remove_if(str.begin(), str.end(), isInvalid), str.end());
}

/** Print some text after hitting CTRL-C. 
 * Used only in main() for signal()
 */
void interruptHandler(int) {
    die(_("Caught CTRL-C, Exiting!"));
}

/** Function to check if a package is from a synchronization database
 * Basically if it's in pacman repos like core, extra, multilib, etc.
 * @param name The package name to check
 * @param syncdbs
 * @return true if the pkg exists, else false
 */
bool is_package_from_syncdb(const char *name, alpm_list_t *syncdbs) {
    for (; syncdbs; syncdbs = alpm_list_next(syncdbs))
        if (alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), name))
            return true;

    return false;
}

// soft means it won't return false (or even try) if the list is empty
bool commitTransactionAndRelease(bool soft) {
    alpm_handle_t *handle = config->handle;

    alpm_list_t   *addPkgs    = alpm_trans_get_add(handle);
    alpm_list_t   *removePkgs = alpm_trans_get_remove(handle);
    alpm_list_t   *combined   = alpm_list_join(addPkgs, removePkgs);

    if (soft && !combined)
        return true;

    log_println(INFO, _("Changes to be made:"));
    for (alpm_list_t *addPkgsClone = addPkgs; addPkgsClone; addPkgsClone = addPkgsClone->next) {
        fmt::print(BOLD_TEXT(color.green), "    ++ ");
        fmt::println(fmt::emphasis::bold, "{}", alpm_pkg_get_name((alpm_pkg_t *)(addPkgsClone->data)));
    }

    for (alpm_list_t *removePkgsClone = removePkgs; removePkgsClone; removePkgsClone = removePkgsClone->next) {
        fmt::print(BOLD_TEXT(color.red), "    -- ");
        fmt::println(fmt::emphasis::bold, "{}", alpm_pkg_get_name((alpm_pkg_t *)(removePkgsClone->data)));
    }

    if (!askUserYorN(true, PROMPT_YN_PROCEED_TRANSACTION)) {
        bool releaseStatus = alpm_trans_release(handle) == 0;
        if (!releaseStatus)
            log_println(ERROR, _("Failed to release transaction ({})."), alpm_strerror(alpm_errno(handle)));

        log_println(INFO, _("Cancelled transaction."));
        return soft;
    }

    bool prepareStatus = alpm_trans_prepare(handle, &combined) == 0;
    if (!prepareStatus)
        log_println(ERROR, _("Failed to prepare transaction ({})."), alpm_strerror(alpm_errno(handle)));

    bool commitStatus = alpm_trans_commit(handle, &combined) == 0;
    if (!commitStatus)
        log_println(ERROR, _("Failed to commit transaction ({})."), alpm_strerror(alpm_errno(handle)));

    bool releaseStatus = alpm_trans_release(handle) == 0;
    if (!releaseStatus)
        log_println(ERROR, _("Failed to release transaction ({})."), alpm_strerror(alpm_errno(handle)));

    if (prepareStatus && commitStatus && releaseStatus) {
        log_println(INFO, _("Successfully finished transaction."));
        return true;
    }

    return false;
}

/** Replace special symbols such as ~ and $ in std::strings
 * @param str The string
 * @return The modified string
 */
string expandVar(string& str) {
    const char *env;
    if (str[0] == '~') {
        env = getenv("HOME");
        if (env == nullptr)
            die(_("FATAL: $HOME enviroment variable is not set (how?)"));
        
        str.replace(0, 1, env); // replace ~ with the $HOME value
    } else if (str[0] == '$') {
        str.erase(0, 1); // erase from str[0] to str[1]
        env = getenv(str.c_str());
        if (env == nullptr)
            die(_("No such enviroment variable: {}"), str);
        
        str = env;
    }

    return str;
}

fmt::rgb hexStringToColor(string_view hexstr) {
    hexstr = hexstr.substr(1);
    // convert the hexadecimal string to individual components
    std::stringstream ss;
    ss << std::hex << hexstr;

    int intValue;
    ss >> intValue;

    int red   = (intValue >> 16) & 0xFF;
    int green = (intValue >> 8) & 0xFF;
    int blue  = intValue & 0xFF;

    return fmt::rgb(red, green, blue);
}

// http://stackoverflow.com/questions/478898/ddg#478960
string shell_exec(string_view cmd) {
    std::array<char, 128> buffer;
    string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.data(), "r"), pclose);

    if (!pipe)
        die(_("popen() failed!"));

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();

    // why there is a '\n' at the end??
    if (!result.empty() && result[result.length() - 1] == '\n')
        result.erase(result.length() - 1);
    return result;
}

// https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c#4654718
bool is_numerical(string_view s, bool allowSpace) {
    if (allowSpace)
        return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return (!std::isdigit(c) && (c != ' ')); }) == s.end();
    else
        return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return (!std::isdigit(c)); }) == s.end();
}

bool taur_read_exec(vector<const char *> cmd, string& output, bool exitOnFailure) {
    int pipeout[2];

    if (pipe(pipeout) < 0)
        die(_("pipe() failed: {}"), strerror(errno));

    int pid = fork();

    if (pid > 0) { // we wait for the command to finish then start executing the rest
        close(pipeout[1]);

        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // read stdout
            char c;
            while (read(pipeout[0], &c, 1) == 1) {
                output += c;
            }

            close(pipeout[0]);

            return true;
        } else {
            log_println(ERROR, _("Failed to execute the command: {}"), fmt::join(cmd, " "));
            if (exitOnFailure)
                exit(-1);
        }
    } else if (pid == 0) {
        if (dup2(pipeout[1], STDOUT_FILENO) == -1)
            exit(1);

        close(pipeout[0]);
        close(pipeout[1]);
        cmd.push_back(nullptr);
        execvp(cmd[0], const_cast<char *const *>(cmd.data()));

        die(_("An error has occurred: {}"), strerror(errno));
    } else {
        close(pipeout[0]);
        close(pipeout[1]);
        die(_("fork() failed: {}"), strerror(errno));
    }

    close(pipeout[0]);
    close(pipeout[1]);

    return false;
}

/** Executes commands with execvp() and keep the program running without existing
 * @param cmd The command to execute 
 * @param exitOnFailure Whether to call exit(1) on command failure.
 * @return true if the command successed, else false 
 */
bool taur_exec(vector<const char *> cmd, bool exitOnFailure) {

    int pid = fork();

    if (pid < 0) {
        die(_("fork() failed: {}"), strerror(errno));
    }

    if (pid == 0) {
        log_println(DEBUG, _("running {}"), fmt::join(cmd, " "));
        cmd.push_back(nullptr);
        execvp(cmd[0], const_cast<char *const *>(cmd.data()));

        // execvp() returns instead of exiting when failed
        die(_("An error has occurred: {}: {}"), cmd[0], strerror(errno));
    } else if (pid > 0) { // we wait for the command to finish then start executing the rest
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
        else {
            log_println(ERROR, _("Failed to execute the command: {}"), fmt::join(cmd, " "));
            if (exitOnFailure)
                exit(1);
        }
    }

    return false;
}

/** Convinient way to executes makepkg commands with taur_exec() and keep the program running without existing
 * @param cmd The command to execute 
 * @param exitOnFailure Whether to call exit(1) on command failure.
 * @return true if the command successed, else false 
 */
bool makepkg_exec(string_view cmd, bool exitOnFailure) {
    vector<const char *> ccmd = {config->makepkgBin.c_str()};

    if (config->noconfirm)
        ccmd.push_back("--noconfirm");

    if (!config->colors)
        ccmd.push_back("--nocolor");

    ccmd.push_back("--config");
    ccmd.push_back(config->makepkgConf.c_str());

    for (auto& str : split(cmd, ' '))
        ccmd.push_back(str.c_str());

    return taur_exec(ccmd, exitOnFailure);
}

/** Convinient way to executes pacman commands with taur_exec() and keep the program running without existing
 * Note: execPacman() in main.cpp and this are different functions
 * @param op The pacman operation (can and must be like -Syu)
 * @param args The packages to be installed
 * @param exitOnFailure Whether to call exit(1) on command failure. (Default true)
 * @param root If pacman should be executed as root (Default true)
 * @return true if the command successed, else false 
 */
bool pacman_exec(string_view op, vector<string> const& args, bool exitOnFailure, bool root) {
    vector<const char *> ccmd;

    if (root)
        ccmd = {config->sudo.c_str(), "pacman", op.data()};
    else
        ccmd = {"pacman", op.data()};

    if (config->noconfirm)
        ccmd.push_back("--noconfirm");

    if (config->colors)
        ccmd.push_back("--color=auto");
    else
        ccmd.push_back("--color=never");

    ccmd.push_back("--config");
    ccmd.push_back(config->pmConfig.c_str());
    ccmd.push_back("--");

    for (auto& str : args)
        ccmd.push_back(str.c_str());

    return taur_exec(ccmd, exitOnFailure);
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
fmt::text_style getColorFromDBName(string_view db_name) {
    if (db_name == "aur")
        return BOLD_TEXT(color.aur);
    else if (db_name == "extra")
        return BOLD_TEXT(color.extra);
    else if (db_name == "core")
        return BOLD_TEXT(color.core);
    else if (db_name == "multilib")
        return BOLD_TEXT(color.multilib);
    else
        return BOLD_TEXT(color.others);
}

// Takes a pkg to show on search.
void printPkgInfo(TaurPkg_t& pkg, string_view db_name) {
    fmt::print(getColorFromDBName(db_name), "{}/", db_name);
    fmt::print(BOLD, "{} ", pkg.name);
    fmt::print(BOLD_TEXT(color.version), "{} ", pkg.version);
    // Don't print popularity and votes on system packages
    if (pkg.votes > -1) {
        fmt::print(fg(color.popularity), " Popularity: {:.2f} ", pkg.popularity);
        fmt::print(fg(color.votes), "Votes: {} ({}) ", pkg.votes, getTitleFromVotes(pkg.votes));
    }

    if (pkg.maintainer == "\1")
        fmt::print(BOLD_TEXT(color.orphan), "(un-maintained) ");

    if (pkg.outofdate) {
        char       *timestr      = std::ctime(&pkg.outofdate);
        string_view timestr_view = timestr;
        if (!timestr_view.empty()) {
            timestr[timestr_view.length() - 1] = '\0'; // delete the last newline.
            fmt::print(BOLD_TEXT(color.outofdate), "(Outdated: {}) ", timestr);
        }
    }

    if (pkg.installed)
        fmt::println(BOLD_TEXT(color.installed), "[Installed]");
    else
        fmt::print("\n");
    fmt::println("    {}", pkg.desc);
}

void printLocalFullPkgInfo(alpm_pkg_t *pkg) {
    /* make aligned titles once only */
    static int need_alignment = 1;
    if (need_alignment) {
        need_alignment = 0;
        make_aligned_titles();
    }

    string_display(titles[T_NAME], alpm_pkg_get_name(pkg));
    string_display(titles[T_VERSION], alpm_pkg_get_version(pkg));
    string_display(titles[T_DESCRIPTION], alpm_pkg_get_desc(pkg));
    string_display(titles[T_ARCHITECTURE], alpm_pkg_get_arch(pkg));
    string_display(titles[T_URL], alpm_pkg_get_url(pkg));
    list_display(titles[T_LICENSES], alpm_pkg_get_licenses(pkg));
    list_display(titles[T_GROUPS], alpm_pkg_get_groups(pkg));
    deplist_display(titles[T_PROVIDES], alpm_pkg_get_provides(pkg));
    deplist_display(titles[T_DEPENDS_ON], alpm_pkg_get_depends(pkg));
    optdeplist_display(pkg);

    fmt::print("\n");
}

// faster than makepkg --packagelist
string makepkg_list(string_view pkg_name, string path) {
    string ret;

    string versionInfo = shell_exec("grep 'pkgver=' " + path + "/PKGBUILD | cut -d= -f2");
    string pkgrel      = shell_exec("grep 'pkgrel=' " + path + "/PKGBUILD | cut -d= -f2");
    string epoch       = shell_exec("grep 'epoch=' "  + path + "/PKGBUILD | cut -d= -f2");

    if (!pkgrel.empty() && pkgrel[0] != '\0')
        versionInfo += '-' + pkgrel;

    if (!epoch.empty() && epoch[0] != '\0')
        versionInfo = epoch + ':' + versionInfo;

    string arch       = shell_exec("grep 'CARCH=' " + config->makepkgConf + " | cut -d= -f2 | sed -e \"s/'//g\" -e 's/\"//g'");
    string arch_field = shell_exec("awk -F '[()]' '/^arch=/ {gsub(/\"/,\"\",$2); print $2}' " + path + "/PKGBUILD | sed -e \"s/'//g\" -e 's/\"//g'");

    if (arch_field == "any")
        arch = "any";

    string pkgext = shell_exec("grep 'PKGEXT=' " + config->makepkgConf + " | cut -d= -f2 | sed -e \"s/'//g\" -e 's/\"//g'");

    ret = fmt::format("{}/{}-{}-{}{}", path, pkg_name, versionInfo, arch, pkgext);
    return ret;
}

/** Search a DB using libalpm
 * Why? well, alpm_db_search searches for results that match ALL regex patterns, this one searches for results that match ANY regex pattern.
*/
bool util_db_search(alpm_db_t *db, alpm_list_t *needles, alpm_list_t **ret) {
    if (!db || !needles || *ret != nullptr)
        return false;
    
    for (; needles; needles = needles->next) {
        alpm_list_t *ret_needle   = nullptr;
        alpm_list_t *needles_next = needles->next; // save the next value, we will overwrite it.

        needles->next = nullptr;

        if (alpm_db_search(db, needles, &ret_needle) != 0) 
            return false;
        *ret = alpm_list_join(*ret, ret_needle);

        needles->next = needles_next; // put it back
    }

    return true;
}

// Function to perform binary search on a vector of strings
string_view binarySearch(const vector<string>& arr, string_view target) {
    int left = 0, right = arr.size() - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] == target)
            return arr[mid]; // returns the word if target is found
        else if (arr[mid] < target)
            left = mid + 1;
        else
            right = mid - 1;
    }

    return "";
}

vector<string> load_aur_list() {
    path file_path = config->cacheDir / "packages.aur";
    std::ifstream infile(file_path);
    if (!infile.good())
        die(_("Failed to open {}"), file_path.c_str());

    vector<string>    aur_list;
    string            pkg;
    
    while (infile >> pkg)
        aur_list.push_back(pkg);

    return aur_list;
}

bool download_aur_cache(path file_path) {
    cpr::Response r = cpr::Get(cpr::Url{AUR_URL"/packages.gz"});
    
    if (r.status_code == 200) {
        std::ofstream outfile(file_path, std::ios::trunc);
        if (!outfile.is_open()) {
            log_println(ERROR, _("Failed to open/write {}"), file_path.c_str());
            return false;
        }
        outfile << r.text;
    } else {
        log_println(ERROR, _("Failed to download {} with status code: {}"), r.url.str(), r.status_code); 
        return false;
    }

    return true;
}

// recursiveCall indicates whether the function is calling itself.
// This function will automatically try again after downloading the file, if not already present.
// Do not call this with true unless you do not want this behavior.
// It is, by default, false.
bool update_aur_cache(bool recursiveCall) {
    path file_path = config->cacheDir / "packages.aur";
    
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0) {
        if (errno == ENOENT && !recursiveCall) { // file not found, download THEN try again once more.
            log_println(INFO, _("File {} not found, attempting download."), file_path.string());
            return download_aur_cache(file_path) && update_aur_cache(true);
        }

        log_println(ERROR, _("Unable to get {} metadata: {}"), file_path.string(), errno);

        return false;
    }
    
    auto _current_time = std::chrono::system_clock::now();
    auto _timeoutDuration = std::chrono::hours(24 * 5);
    
    auto timeout = std::chrono::duration_cast<std::chrono::seconds>(_timeoutDuration).count();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(_current_time);

    if (file_stat.st_mtim.tv_sec < now_time_t - timeout) {
        log_println(INFO, _("Refreshing {}"), file_path.string());
        download_aur_cache(file_path);
    }
     
    return true;
}

/** Ask the user to select a package out of a list.
 * @param pkgs The list of packages, in a vector
 * @param backend A reference to the TaurBackend responsible for fetching any AUR packages.
 * @param useGit Whether the fetched pkg should use a .git url
 * @return Optional TaurPkg_t, will not return if interrupted.
*/
optional<vector<TaurPkg_t>> askUserForPkg(vector<TaurPkg_t> pkgs, TaurBackend& backend, bool useGit) {
    if (pkgs.size() == 1) {
        return pkgs[0].aur_url.empty() ? pkgs : vector<TaurPkg_t>({backend.fetch_pkg(pkgs[0].name, useGit).value_or(pkgs[0])});
    } else if (pkgs.size() > 1) {
        log_println(INFO, _("TabAUR has found multiple packages relating to your search query, Please pick one."));
        string input;
        do {
            // CTRL-D
            ctrl_d_handler();

            if (!input.empty())
                log_println(WARN, _("Invalid input!"));

            for (size_t i = 0; i < pkgs.size(); i++) {
                fmt::print(fg(color.index), "[{}] ", i);
                printPkgInfo(pkgs[i], pkgs[i].db_name);
            }

            log_printf(NONE, _("Choose a package to download: "));
            std::getline(std::cin, input);
        } while (!is_numerical(input, true));

        vector<string>    indices = split(input, ' ');

        vector<TaurPkg_t> output;
        output.reserve(indices.size());

        for (size_t i = 0; i < indices.size(); i++) {
            size_t selected = std::stoi(indices[i]);

            if (selected >= pkgs.size())
                continue;

            output.push_back(pkgs[selected].aur_url.empty() ? pkgs[selected] : backend.fetch_pkg(pkgs[selected].name, useGit).value_or(pkgs[selected]));
        }

        return output;
    }

    return {};
}

void ctrl_d_handler() {
    if (std::cin.eof()) {
        die(_("Exiting due to CTRL-D or EOF"));
    }
}

/** Filters out/only AUR packages.
 * Default behavior is filtering out.
 * @param pkgs a unique_ptr to a list of packages to filter.
 * @param inverse a bool that, if true, will return only AUR packages instead of the other way around.
 * @return an optional unique_ptr to a result.
*/
vector<alpm_pkg_t *> filterAURPkgs(vector<alpm_pkg_t *> pkgs, alpm_list_t *syncdbs, bool inverse) {
    vector<alpm_pkg_t *> out;
    out.reserve(pkgs.size());

    for (; syncdbs; syncdbs = syncdbs->next) {
        for (size_t i = 0; i < pkgs.size(); i++) {
            bool existsInSync = alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), alpm_pkg_get_name(pkgs[i])) != nullptr;

            if ((existsInSync && inverse) || (!existsInSync && !inverse))
                pkgs[i] = nullptr;
        }
    }

    for (size_t i = 0; i < pkgs.size(); i++)
        if (pkgs[i])
            out.push_back(pkgs[i]);

    return out;
}

/** Filters out/only AUR packages (names only).
 * Default behavior is filtering out.
 * @param pkgs a unique_ptr to a list of packages to filter.
 * @param inverse a bool that, if true, will return only AUR packages instead of the other way around.
 * @return an optional unique_ptr to a result.
*/
vector<string_view> filterAURPkgsNames(vector<string_view> pkgs, alpm_list_t *syncdbs, bool inverse) {
    vector<string_view> out;
    out.reserve(pkgs.size());

    for (; syncdbs; syncdbs = syncdbs->next) {
        for (size_t i = 0; i < pkgs.size(); i++) {
            bool existsInSync = alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), pkgs[i].data()) != nullptr;

            if ((existsInSync && inverse) || (!existsInSync && !inverse))
                pkgs[i] = "";
        }
    }

    for (size_t i = 0; i < pkgs.size(); i++)
        if (pkgs[i].length())
            out.push_back(pkgs[i]);

    return out;
}

string getTitleFromVotes(float votes) {
    if (votes < 2)
        return _("Untrustable");
    if (votes < 5)
        return _("Caution");
    if (votes < 10)
        return _("Normal");
    if (votes < 20)
        return _("Good");
    if (votes >= 20)
        return _("Trustable");
    return _("Weird, report this bug.");
}

/*
* Get the user cache directory
* either from $XDG_CACHE_HOME or from $HOME/.cache/
* @return user's cache directory  
*/
string getHomeCacheDir() {
    char *dir = getenv("XDG_CACHE_HOME");
    if (dir != NULL && dir[0] != '\0' && std::filesystem::exists(dir)) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else {
        char *home = getenv("HOME");
        if (home == nullptr)
            die(_("Failed to find $HOME, set it to your home directory!"));
        
        return string(home) + "/.cache";
    }
}

/*
* Get the user config directory
* either from $XDG_CONFIG_HOME or from $HOME/.config/
* @return user's config directory  
*/
string getHomeConfigDir() {
    char *dir = getenv("XDG_CONFIG_HOME");
    if (dir != NULL && dir[0] != '\0' && std::filesystem::exists(dir)) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else {
        char *home = getenv("HOME");
        if (home == nullptr)
            die(_("Failed to find $HOME, set it to your home directory!"));

        return string(home) + "/.config";
    }
}

/*
 * Get the TabAUR config directory 
 * where we'll have both "config.toml" and "theme.toml"
 * from Config::getHomeConfigDir()
 * @return TabAUR's config directory
 */
string getConfigDir() {
    return getHomeConfigDir() + "/TabAUR";
}

/*
 * Get the TabAUR cache directory 
 * where we'll have all AUR packages downloaded here
 * from Config::getHomeCacheDir()
 * @return TabAUR's cache directory
 */
string getCacheDir() {
    return getHomeCacheDir() + "/TabAUR";
}

vector<string> split(string_view text, char delim) {
    string            line;
    vector<string>    vec;
    std::stringstream ss(text.data());
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}
