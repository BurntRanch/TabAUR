/*
 *
 *  Copyright (c) 2006-2022 Pacman Development Team <pacman-dev@lists.archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <alpm.h>

#include <cstddef>
#pragma GCC diagnostic ignored "-Wvla"

#include <limits.h>

#include "args.hpp"
#include "taur.hpp"
#include "util.hpp"

using namespace std::string_view_literals;

std::unique_ptr<Config>      config;
std::unique_ptr<TaurBackend> backend;

// this may be hard to read, but better than calling fmt::println multiple times
void usage(const int op)
{
    if (op == OP_MAIN)
    {
        fmt::println("TabAUR usage: taur <op> [...]"sv);
        fmt::print("options:\n{}", R"(
    taur {-h --help}
    taur {-V --version}
    taur {-t, --test-colors}
    taur {-D --database} <options> <package(s)>
    taur {-F --files}    [options] [file(s)]
    taur {-Q --query}    [options] [package(s)]
    taur {-R --remove}   [options] <package(s)>
    taur {-S --sync}     [options] [package(s)]
    taur {-T --deptest}  [options] [package(s)]
    taur {-U --upgrade}  [options] <file(s)>
    )"sv);
    }
    else
    {
        if (op == OP_SYNC)
        {
            fmt::println("usage:  taur {{-S --sync}} [options] [package(s)]");
            fmt::println("options:{}", R"(
    -s, --search <regex> search remote repositories for matching strings
    -u, --sysupgrade     upgrade installed packages (-uu enables downgrades)
    -y, --refresh        download fresh package databases from the server
            )"sv);
        }
        else if (op == OP_QUERY)
        {
            fmt::println("usage: taur {{-Q --query}} [options] [package(s)]");
            fmt::print("options:{}", R"(
    -q, --quiet          show less information for query and search
                         )"sv);
        }
    }

    fmt::println("{}", R"(
    -a, --aur-only       do only AUR operations
    -g, --use-git        use git instead of tarballs for AUR repos
    --config    <path>   set an alternate configuration file
    --theme     <path>   set an alternate theme file
    --cachedir  <dir>    set an alternate package cache location
    --colors    <1,0>    colorize the output
    --debug     <1,0>    show debug messages
    --sudo      <path>   choose which binary to use for privilege-escalation
    --noconfirm          do not ask for any confirmation (passed to both makepkg and pacman)
    )"sv);
}

void test_colors()
{
    const std::time_t& current_time = std::time(nullptr);
    std::string timestr      = std::ctime(&current_time);
    timestr.pop_back();

    const TaurPkg_t pkg = 
    {
        .name       = "TabAUR",
        .version    = VERSION,
        .desc       = "A customizable and lightweight AUR helper, designed to be simple but powerful.",
        .maintainer = "\1",
        .outofdate  = current_time,
        .popularity = 'R',  // if you know, you know :P
        .votes      = 34,
        .installed  = true,
        .db_name    = "aur",
    };

    if (fmt::disable_colors)
        fmt::println("Colors are disabled");
    
    log_println(DEBUG, _("Debug color: {}"), fmt::format(BOLD_COLOR(color.magenta), "(bold) magenta"));
    log_println(INFO, _("Info color: {}"), fmt::format(BOLD_COLOR(color.cyan), "(bold) cyan"));
    log_println(WARN, _("Warning color: {}"), fmt::format(BOLD_COLOR(color.yellow), "(bold) yellow"));
    log_println(ERROR, _("Error color: {}"), fmt::format(BOLD_COLOR(color.red), "(bold) red"));
    fmt::println(fg(color.red), "red");
    fmt::println(fg(color.blue), "blue");
    fmt::println(fg(color.yellow), "yellow");
    fmt::println(fg(color.green), "green");
    fmt::println(fg(color.cyan), "cyan");
    fmt::println(fg(color.magenta), "magenta");
    fmt::println(fg(color.gray), "gray");

    fmt::println("\ndb colors:");
    fmt::println(getColorFromDBName("aur"), "(bold) aur");
    fmt::println(getColorFromDBName("extra"), "(bold) extra");
    fmt::println(getColorFromDBName("core"), "(bold) core");
    fmt::println(getColorFromDBName("multilib"), "(bold) multilib");
    fmt::println(getColorFromDBName("others"), "(bold) others");

    fmt::println(BOLD_COLOR(color.version), "\n(bold) version " VERSION);
    fmt::println(fg(color.popularity), "Popularity: {}", pkg.popularity);
    fmt::println(fg(color.votes), "Votes: {} ({})", pkg.votes, getTitleFromVotes(pkg.votes));
    fmt::println(fg(color.index), "index [1]");
    fmt::println(BOLD_COLOR(color.installed), "(bold) indicator [Installed]");
    fmt::println(BOLD_COLOR(color.orphan), "(bold) (un-maintained)");
    fmt::println(BOLD_COLOR(color.outofdate), "(bold) (Outdated: {})", timestr);

    fmt::println("\nexamples package search preview:");
    printPkgInfo(pkg, pkg.db_name);
}

void execPacman(int argc, char* argv[])
{
    if (argc > (_POSIX_ARG_MAX - 1))
        die(_("argc is invalid! (argc > {})"), _POSIX_ARG_MAX - 1);

    std::vector<const char*> args;

    args.push_back("pacman");
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);
    args.push_back(nullptr);

    if (execvp(args[0], const_cast<char* const*>(args.data())) == -1)
        perror("execvp");

    exit(1);
}

int installPkg(alpm_list_t* pkgNames)
{
    if (!pkgNames && !op.op_s_upgrade)
        return false;

    const bool  useGit   = config->useGit;
    const path& cacheDir = config->cacheDir;

    bool returnStatus = true;
    bool stat;

    std::vector<std::string>      pacmanPkgs;  // list of pacman packages to install, to avoid spamming pacman.
    std::vector<std::string_view> pkgNamesVec;
    std::vector<std::string_view> pkgsToCleanBuild, pkgsToReview;

    // move them into a vector for askUserForList
    for (; pkgNames; pkgNames = pkgNames->next)
        pkgNamesVec.push_back(reinterpret_cast<const char *>(pkgNames->data));

    if (pkgNamesVec.empty())
    {
        if (op.op_s_search)
        {
            log_println(WARN, _("Please specify a target"));
            return false;
        }
    }

    if (op.op_s_search)
    {
        for (size_t i = 0; i < pkgNamesVec.size(); i++)
        {
            const std::vector<TaurPkg_t>& pkgs = backend->search(pkgNamesVec[i], useGit, config->aurOnly, false);

            if (pkgs.empty())
            {
                log_println(WARN, _("No results found for {}!"), pkgNamesVec[i]);
                returnStatus = false;
                continue;
            }

            for (size_t i = 0; i < pkgs.size(); i++)
                printPkgInfo(pkgs[i], pkgs[i].db_name);

            returnStatus = true;
        }
        return returnStatus;
    }

    if (!update_aur_cache())
        log_println(ERROR, _("Failed to get information about {}"), (config->cacheDir / "packages.aur").string());

    // I swear there was a comment here..
    const std::vector<std::string_view>& AURPkgs = filterAURPkgsNames(pkgNamesVec, alpm_get_syncdbs(config->handle), true);

    for (const std::string_view pkg : pkgNamesVec)
    {
        if (std::find(AURPkgs.begin(), AURPkgs.end(), pkg) == AURPkgs.end())
            pacmanPkgs.push_back(pkg.data());
    }

    if (!op.op_s_cleanbuild && !AURPkgs.empty())
        pkgsToCleanBuild = askUserForList<std::string_view>(AURPkgs, PROMPT_LIST_CLEANBUILDS);

    if (!config->noconfirm && !AURPkgs.empty())
        pkgsToReview = askUserForList<std::string_view>(AURPkgs, PROMPT_LIST_REVIEWS);

    for (const std::string_view pkg_name : AURPkgs)
    {
        const path& pkgDir = cacheDir / pkg_name;

        stat = useGit ? backend->download_git(AUR_URL_GIT(pkg_name), pkgDir)
                      : backend->download_tar(AUR_URL_TAR(pkg_name), pkgDir);
        if (!stat)
        {
            log_println(ERROR, _("Failed to download {}"), pkg_name);
            returnStatus = false;
            continue;
        }
    }

    for (const std::string_view pkg : pkgsToCleanBuild)
    {
        const path& pkgDir = cacheDir / pkg;
        if (!useGit)
        {
            log_println(INFO, _("Removing {}"), pkgDir.c_str());
            std::filesystem::remove_all(pkgDir);
        }
        else
        {
            log_println(INFO, _("Cleaning {}"), pkgDir.c_str());
            taur_exec({ config->git, "-C", pkgDir.c_str(), "clean", "-xffd" });
        }
    }

    // cmd is just a workaround for making it possible
    // to pass flags to the editor, e.g nano --modernbindings
    // instead of creating another config variable
    for (const std::string_view pkg : pkgsToReview)
    {
        const path& pkgDir = cacheDir / pkg;

        std::vector<std::string> cmd;
        cmd.reserve(config->editor.size());
        for (auto& str : config->editor)
            cmd.push_back(str);

        cmd.push_back((pkgDir / "PKGBUILD").string());

        taur_exec(cmd);
    }

    if (!pkgsToReview.empty() && !askUserYorN(YES, PROMPT_YN_PROCEED_INSTALL))
        return false;

    if (!config->aurOnly && (op.op_s_upgrade || !pacmanPkgs.empty()))
    {
        if (op.op_s_upgrade)
            log_println(INFO, _("Upgrading system packages!"));
        else
            log_println(INFO, _("Installing system packages!"));

        std::string op_s = "-S";

        if (op.op_s_sync)
            op_s += 'y';

        if (op.op_s_upgrade)
            op_s += 'u';

        pacman_exec(op_s, pacmanPkgs);
    }

    if (op.op_s_upgrade)
    {
        log_println(INFO, _("Upgrading AUR packages!"));
        backend->update_all_aur_pkgs(cacheDir, useGit);

        pkgs_to_install = "";  // Reset the list.
    }

    for (size_t i = 0; i < AURPkgs.size(); i++)
    {
        const std::vector<TaurPkg_t>& pkgs = backend->search(AURPkgs[i], useGit, config->aurOnly, true);

        const std::optional<std::vector<TaurPkg_t>>& oSelectedPkgs = askUserForPkg(pkgs, *backend, useGit);

        if (!oSelectedPkgs)
        {
            returnStatus = false;
            continue;
        }

        const std::vector<TaurPkg_t>& selectedPkgs = oSelectedPkgs.value();

        for (const TaurPkg_t& pkg : selectedPkgs)
        {
            path pkgDir = cacheDir / pkg.name;

            stat = backend->handle_aur_depends(pkg, cacheDir, backend->get_all_local_pkgs(true), useGit);

            if (!stat)
            {
                log_println(ERROR, _("Installing AUR dependencies for your package has failed."));
                returnStatus = false;
                continue;
            }

            stat = backend->build_pkg(pkg.name, pkgDir.string(), false);

            if (!stat)
            {
                log_println(ERROR, _("Building '{}' has failed."), pkg.name);
                returnStatus = false;
                pkgs_failed_to_build += pkg.name + ' ';
                log_println(DEBUG, "pkgs_failed_to_build = {}", pkgs_failed_to_build);
                continue;
            }

            pkgs_to_install += built_pkg + ' ';
        }
    }

    if (!pkgs_to_install.empty())
    {
        log_println(DEBUG, _("Installing {}"), fmt::join(pkgNamesVec, " "));
        pkgs_to_install.pop_back();
        if (!pacman_exec("-U", split(pkgs_to_install, ' '), false))
        {
            log_println(ERROR, _("Failed to install {}"), fmt::join(pkgNamesVec, " "));
            returnStatus = false;
        }
    }

    if (!pkgs_failed_to_build.empty())
    {
        pkgs_failed_to_build.erase(pkgs_failed_to_build.end() - 1);
        log_println(WARN, fg(color.red),  _("Failed to upgrade: {}"), pkgs_failed_to_build);
        log_println(INFO, fg(color.cyan), _("Tip: try to run taur with \"-S {}\" and cleanbuild every failed packages"),
                    pkgs_failed_to_build, pkgs_failed_to_build);
    }
    return returnStatus;
}

bool removePkg(alpm_list_t* pkgNames)
{
    if (!pkgNames)
        return false;

    alpm_list_t* exactMatches     = nullptr;
    alpm_list_t* searchResults    = nullptr;
    alpm_list_t* filteredPkgNames = nullptr;
    alpm_db_t*   localdb          = alpm_get_localdb(config->handle);

    for (alpm_list_t* i = pkgNames; i; i = i->next)
    {
        alpm_pkg_t* result = alpm_db_get_pkg(localdb, reinterpret_cast<const char *>(i->data));
        if (result != NULL)
            exactMatches = alpm_list_add(exactMatches, result);
        else
            filteredPkgNames = alpm_list_add(filteredPkgNames, i->data);
    }

    if (filteredPkgNames && util_db_search(localdb, filteredPkgNames, &searchResults) != 0)
    {
        if (exactMatches)
            alpm_list_free(exactMatches);
        alpm_list_free(filteredPkgNames);
        log_println(ERROR, _("Searching failed, Bailing out!"));
        return false;
    }

    alpm_list_smart_pointer ret(alpm_list_join(exactMatches, searchResults), alpm_list_free);

    const size_t ret_size = alpm_list_count(ret.get());

    if (ret_size == 0)
    {
        log_println(ERROR, _("No packages found!"));
        return false;
    }

    if (ret_size == 1)
        return backend->remove_pkg(reinterpret_cast<alpm_pkg_t *>(ret->data));

    std::vector<std::string_view> pkgs;
    pkgs.reserve(ret_size);

    // fmt::println("Choose packages to remove, (Seperate by spaces, type * to remove all):");

    for (size_t i = 0; i < ret_size; i++)
        pkgs.push_back(alpm_pkg_get_name(reinterpret_cast<alpm_pkg_t *>(alpm_list_nth(ret.get(), i)->data)));

    const std::vector<std::string_view>& includedPkgs = askUserForList<std::string_view>(pkgs, PROMPT_LIST_REMOVE_PKGS, true);

    alpm_list_t* finalPackageList      = nullptr;
    alpm_list_t* finalPackageListStart = nullptr;

    for (size_t i = 0; i < includedPkgs.size(); i++)
    {
        try
        {
            const auto& pkg = std::find(pkgs.begin(), pkgs.end(), includedPkgs[i]);

            if (pkg >= pkgs.end())
                continue;

            const size_t pkgIndex = std::distance(pkgs.begin(), pkg);

            alpm_list_nth(ret.get(), pkgIndex);

            finalPackageList = alpm_list_add(finalPackageList, alpm_list_nth(ret.get(), pkgIndex)->data);

            if (finalPackageList != nullptr && finalPackageListStart == nullptr)
                finalPackageListStart = finalPackageList;
        }
        catch (std::invalid_argument const&)
        {
            log_println(WARN, _("Invalid argument! Assuming all."));

            if (finalPackageListStart != nullptr)
                alpm_list_free(finalPackageListStart);
            else if (finalPackageList != nullptr)
                alpm_list_free(finalPackageList);

            return backend->remove_pkgs(ret);
        }
    }

    // take control of the list and pass it to the smart pointer
    const alpm_list_smart_pointer& finalList = make_list_smart_pointer(finalPackageListStart);

    return backend->remove_pkgs(finalList);
}

bool queryPkgs(alpm_list_t* pkgNames)
{
    log_println(DEBUG, "AUR Only: {}", config->aurOnly);

    // we seperate pkgs that needs to be searched, like they'll include a description, to the ones that we just wanna
    // query out pkgs will be used for -Qs, for then using it on printPkgInfo() pkgs_name and pkgs_ver will be used for
    // bare operations (only -Q)
    std::vector<const char *> pkgs_name, pkgs_ver;
    std::vector<alpm_pkg_t *> pkgs;
    alpm_db_t*                localdb = alpm_get_localdb(config->handle);

    // just -Q, no options other than --quiet and global ones
    if (!pkgNames)
    {
        alpm_list_t* pkg;
        for (pkg = alpm_db_get_pkgcache(localdb); pkg; pkg = alpm_list_next(pkg))
        {
            pkgs.push_back(reinterpret_cast<alpm_pkg_t *>(pkg->data));
            pkgs_name.push_back(alpm_pkg_get_name(reinterpret_cast<alpm_pkg_t *>(pkg->data)));
            pkgs_ver.push_back(alpm_pkg_get_version(reinterpret_cast<alpm_pkg_t *>(pkg->data)));
        }
    }
    else if (op.op_q_search)
    {
        alpm_list_t* result = nullptr;
        util_db_search(localdb, pkgNames, &result);

        for (; result; result = result->next)
        {
            pkgs.push_back(reinterpret_cast<alpm_pkg_t *>(result->data));
            pkgs_name.push_back(alpm_pkg_get_name(reinterpret_cast<alpm_pkg_t *>(result->data)));
            pkgs_ver.push_back(alpm_pkg_get_version(reinterpret_cast<alpm_pkg_t *>(result->data)));
        }
    } 
    else
    {
        for (; pkgNames; pkgNames = pkgNames->next)
        {
            const char* strname = reinterpret_cast<const char *>(pkgNames->data);
            alpm_pkg_t* pkg     = alpm_db_get_pkg(localdb, strname);

            if (pkg == nullptr)
                pkg = alpm_find_satisfier(alpm_db_get_pkgcache(localdb), strname);

            if (pkg == nullptr)
            {
                log_println(ERROR, _("package \"{}\" was not found"), strname);
                continue;
            }

            pkgs.push_back(pkg);
            pkgs_name.push_back(alpm_pkg_get_name(pkg));
            pkgs_ver.push_back(alpm_pkg_get_version(pkg));
        }
    }

    if (config->aurOnly)
    {
        alpm_list_t* syncdbs = alpm_get_syncdbs(config->handle);

        if (!syncdbs)
        {
            log_println(ERROR, _("Failed to get syncdbs!"));
            return false;
        }

        for (; syncdbs; syncdbs = syncdbs->next)
            for (size_t i = 0; i < pkgs_name.size(); i++)
                if (alpm_db_get_pkg((alpm_db_t*)(syncdbs->data), pkgs_name[i]))
                    pkgs_name[i] = NULL;  // wont be printed
    }

    if (config->quiet)
    {
        for (size_t i = 0; i < pkgs_name.size(); i++)
        {
            if (!pkgs_name[i])
                continue;
            fmt::println("{}", pkgs_name[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < pkgs_name.size(); i++)
        {
            if (!pkgs_name[i])
                continue;

            if (!op.op_q_info) {
                fmt::print(BOLD, "{} ", pkgs_name[i]);
                fmt::println(BOLD_COLOR(color.green), "{}", pkgs_ver[i]);
            } else {
                printLocalFullPkgInfo(pkgs[i]);
            }
        }
    }

    return true;
}

bool upgradePkgs(alpm_list_t* pkgNames)
{
    if (!pkgNames)
        return false;

    if (alpm_trans_init(config->handle, config->flags))
    {
        log_println(ERROR, _("Failed to initialize transaction ({})"), alpm_strerror(alpm_errno(config->handle)));
        return false;
    }

    for (; pkgNames; pkgNames = pkgNames->next)
    {
        alpm_pkg_t* pkg;
        alpm_pkg_load(config->handle, reinterpret_cast<const char *>(pkgNames->data), 1,
                      alpm_option_get_local_file_siglevel(config->handle), &pkg);

        if (!pkg)
        {
            log_println(ERROR, _("Failed to load package {}! ({})"), reinterpret_cast<const char *>(pkgNames->data),
                        alpm_strerror(alpm_errno(config->handle)));
            return false;  // Yes, I am ignoring the transaction we just created.
        }

        if (alpm_add_pkg(config->handle, pkg))
        {
            log_println(ERROR, _("Failed to add package {} to transaction! ({})"), reinterpret_cast<const char *>(pkgNames->data),
                        alpm_strerror(alpm_errno(config->handle)));
            return false;
        }
    }

    return commitTransactionAndRelease(true);
}

/** Sets up gettext localization. Safe to call multiple times.
 */
/* Inspired by the monotone function localize_monotone. */
// taken from pacman
#if defined(ENABLE_NLS)
static void localize(void)
{
    static int init = 0;
    if (!init)
    {
        setlocale(LC_ALL, "");
        bindtextdomain("taur", LOCALEDIR);
        textdomain("taur");
        init = 1;
    }
}
#endif

// clang-format off
// parseargs() but only for parsing the user config path trough args
// and so we can directly construct Config
static bool parse_config_path(int argc, char* argv[], std::string& configFile, std::string& themeFile)
{
    int opt = 0;
    int option_index = 0;
    opterr = 0;
    const char *optstring = "-C:";
    static const struct option opts[] =
    {
        {"config", required_argument, 0, OP_CONFIG},
        {"theme",  required_argument, 0, OP_THEME},
        {0,0,0,0}
    };
    
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1)
    {
        if (opt == 0 || opt == '?')
            continue;

        switch (opt)
        {
            case OP_CONFIG:
                configFile = strndup(optarg, PATH_MAX); 
                if (!std::filesystem::exists(configFile))
                    die("config file '{}' doesn't exist", configFile);

                break;

            case OP_THEME:
                themeFile = strndup(optarg, PATH_MAX); 
                if (!std::filesystem::exists(themeFile))
                    die("theme file '{}' doesn't exist", themeFile);

                break;

            default:
                return false;
        }
    }

    return true;
}

// function taken from pacman
int parseargs(int argc, char* argv[])
{
    // default
    op.op = OP_MAIN;

    int opt = 0;
    int option_index = 0;
    int result = 0;
    opterr = 1; // re-enable since before we disabled for "invalid option" error
    optind = 0;
    const char *optstring = "DFQRSTUVaihqsurytns";
    static const struct option opts[] =
    {
        {"database",   no_argument,       0, 'D'},
        {"files",      no_argument,       0, 'F'},
        {"query",      no_argument,       0, 'Q'},
        {"remove",     no_argument,       0, 'R'},
        {"sync",       no_argument,       0, 'S'},
        {"deptest",    no_argument,       0, 'T'}, /* used by makepkg */
        {"upgrade",    no_argument,       0, 'U'},
        {"version",    no_argument,       0, 'V'},
        {"help",       no_argument,       0, 'h'},
        {"test-colors",no_argument,       0, 't'},
        {"recipe",     no_argument,       0, 'r'},

        {"refresh",    no_argument,       0, OP_REFRESH},
        {"sysupgrade", no_argument,       0, OP_SYSUPGRADE},
        {"search",     no_argument,       0, OP_SEARCH},
        {"info",       no_argument,       0, OP_INFO},
        {"cleanbuild", no_argument,       0, OP_CLEANBUILD},
        {"cachedir",   required_argument, 0, OP_CACHEDIR},
        {"colors",     required_argument, 0, OP_COLORS},
        {"config",     required_argument, 0, OP_CONFIG},
        {"theme",      required_argument, 0, OP_THEME},
        {"aur-only",   no_argument,       0, OP_AURONLY},
        {"sudo",       required_argument, 0, OP_SUDO},
        {"use-git",    required_argument, 0, OP_USEGIT},
        {"quiet",      no_argument,       0, OP_QUIET},
        {"debug",      no_argument,       0, OP_DEBUG},
        {"noconfirm",  no_argument,       0, OP_NOCONFIRM},
        {"nosave",     no_argument,       0, OP_NOSAVE},
        {"recursive",  no_argument,       0, OP_RECURSIVE},
        {0,0,0,0}
    };

    // clang-format on
    /* parse operation */
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1)
    {
        if (opt == 0)
            continue;
        else if (opt == '?')
            return 1;
        parsearg_op(opt, 0);
    }

    if (op.op == OP_PACMAN)
    {
        log_println(NONE, _("Please use pacman for this command (may need root too)"));
        execPacman(argc, argv);
    }
    if (op.op == 0)
    {
        log_println(ERROR, _("only one operation may be used at a time"));
        return 1;
    }

    if (op.version)
    {
        fmt::println("TabAUR {} branch {}", VERSION, BRANCH);
        exit(0);
    }
    if (op.help)
    {
        usage(op.op);
        exit(1);
    }

    optind = 1;
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1)
    {
        if (opt == 0)
            continue;
        /* this should have failed during first pass already */
        else if (opt == '?')
            return 1;
        /* opt is an operation */
        else if (parsearg_op(opt, 1) == 0)
            continue;

        /* parse all other options */
        switch (op.op)
        {
            case OP_SYNC:  result = parsearg_sync(opt); break;
            case OP_QUERY: result = parsearg_query(opt); break;
            case OP_REM:   result = parsearg_remove(opt); break;
            default:       result = 1; break;
        }

        if (result == 0)
        {
            continue;
        }

        /* fall back to global options */
        result = parsearg_global(opt);
        if (result != 0)
        {
            if (result == 1)
            {
                /* global option parsing failed, abort */
                if (opt < 1000)
                {
                    log_println(ERROR, _("invalid option '-{:c}'"), opt);
                }
                else
                {
                    log_println(ERROR, _("invalid option '--{}'"), opts[option_index].name);
                }
            }
            return result;
        }
    }

    while (optind < argc)
    {
        /* add the target to our target array */
        alpm_list_t* taur_targets_get = taur_targets.get();
        (void)taur_targets.release();
        alpm_list_append_strdup(&taur_targets_get, argv[optind]);
        taur_targets = make_list_smart_deleter(taur_targets_get);
        optind++;
    }

    return 0;
}

// main
int main(int argc, char* argv[])
{
    const std::string& configDir = getConfigDir();

#if defined(ENABLE_NLS)
    localize();
#endif

    std::string configfile = (configDir + "/config.toml");
    std::string themefile  = (configDir + "/theme.toml");
    parse_config_path(argc, argv, configfile, themefile);

    config = std::make_unique<Config>(configfile, themefile, configDir);

    if (parseargs(argc, argv))
        return 1;

    if (op.test_colors)
    {
        test_colors();
        return 0;
    }

    if (op.show_recipe)
    {
        if (config->secretRecipe)
        {
            log_println(INFO, _("Secret recipe unlocked!"));
            log_println(INFO, _("Loading secret recipe..."));
            for (auto const& i : secret)
            {
                fmt::println("{}", i);
                usleep(650000);  // 0.65 seconds
            }
            return 0;
        }
    }

    signal(SIGINT, &interruptHandler);

    // the code you're likely interested in
    backend = std::make_unique<TaurBackend>(*config);

    if (op.requires_root && geteuid() != 0)
    {
        log_println(ERROR, _("You need to be root to do this."));
        return 1;
    }
    else if (!op.requires_root && geteuid() == 0)
    {
        log_println(ERROR, _("You are trying to run TabAUR as root when you don't need it."));
        return 1;
    }

    // clang-format off
    switch (op.op)
    {
        case OP_SYNC:
            return installPkg(taur_targets.get()) ? 0 : 1;
        case OP_REM:
            log_println(WARN, _("Watch out when using the -R operation in TabAUR, it has been tested pretty well, but you should always watch out for any errors, Please use pacman or be careful"));
            return removePkg(taur_targets.get()) ? 0 : 1;
        case OP_QUERY:
            return queryPkgs(taur_targets.get()) ? 0 : 1;
        case OP_UPGRADE:
            return upgradePkgs(taur_targets.get()) ? 0 : 1;
        default:
            log_println(ERROR, _("no operation specified (use {} -h for help)"), argv[0]);
    }

    return EXIT_SUCCESS;
}
