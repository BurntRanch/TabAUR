#include <alpm.h>
#include <alpm_list.h>
#include <filesystem>
#include <iostream>
#pragma GCC diagnostic ignored "-Wvla"

#include "util.hpp"
#include "args.hpp"
#include "taur.hpp"

#define BRANCH  "libalpm-test"
#define VERSION "0.6.2"

std::unique_ptr<Config>      config;
std::unique_ptr<TaurBackend> backend;

// this may be hard to read, but better than calling fmt::println multiple times
void usage(int op) {
    if (op == OP_MAIN) {
        fmt::println("TabAUR usage: taur <op> [...]");
        fmt::print("options:\n{}", R"#(
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
    )#");
    } else {
        if (op == OP_SYNC) {
            fmt::println("usage:  taur {{-S --sync}} [options] [package(s)]");
            fmt::println("options:{}", R"#(
    -s, --search <regex> search remote repositories for matching strings
    -u, --sysupgrade     upgrade installed packages (-uu enables downgrades)
    -y, --refresh        download fresh package databases from the server
            )#");
        } else if (op == OP_QUERY) {
            fmt::println("usage: taur {{-Q --query}} [options] [package(s)]");
            fmt::print("options:{}", R"#(
    -q, --quiet          show less information for query and search
                         )#");
        }
    }

    fmt::println("{}", R"#(
    -a, --aur-only       do only AUR operations
    -g, --use-git        use git instead of tarballs for AUR repos
    --config    <path>   set an alternate configuration file
    --theme     <path>   set an alternate theme file
    --cachedir  <dir>    set an alternate package cache location
    --colors    <1,0>    colorize the output
    --debug     <1,0>    show debug messages
    --sudo      <path>   choose which binary to use for privilage-escalation
    --noconfirm          do not ask for any confirmation (passed to both makepkg and pacman)
    )#");

}

void test_colors() {
    TaurPkg_t pkg = {
        .name       = "TabAUR",
        .version    = VERSION,
        .desc       = "A customizable and lightweight AUR helper, designed to be simple but powerful.",
        .popularity = 100,
        .installed  = true,
        .db_name    = "aur",
    };

    if (fmt::disable_colors)
        fmt::println("Colors are disabled");
    log_println(LOG_DEBUG, "Debug color: {}",  fmt::format(BOLD_TEXT(color.magenta), "(bold) magenta"));
    log_println(LOG_INFO, "Info color: {}",    fmt::format(BOLD_TEXT(color.cyan),    "(bold) cyan"));
    log_println(LOG_WARN, "Warning color: {}", fmt::format(BOLD_TEXT(color.yellow),  "(bold) yellow"));
    log_println(LOG_ERROR, "Error color: {}",  fmt::format(BOLD_TEXT(color.red),     "(bold) red"));
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

    fmt::println(BOLD_TEXT(color.version), "\n(bold) version " VERSION);
    fmt::println(fg(color.popularity), "Popularity: {} ({})", pkg.popularity, getTitleForPopularity(pkg.popularity));
    fmt::println(fg(color.index), "index [1]");
    fmt::println(fg(color.installed), "indicator [Installed]");

    fmt::println("\nexamples package search preview:");
    printPkgInfo(pkg, pkg.db_name, 1);
}

bool execPacman(int argc, char* argv[]) {
    log_println(LOG_DEBUG, "Passing command to pacman! (argc: {:d})", argc);

    if (0 > argc || argc > 512)
        throw std::invalid_argument("argc is invalid! (512 > argc > 0)");

    char* args[argc + 3]; // sudo + pacman + null terminator

    args[0] = _(config->sudo.c_str());
    args[1] = _("pacman"); // The command to run as superuser (pacman)
    for (int i = 0; i < argc; ++i) {
        log_println(LOG_DEBUG, "args[{}] = argv[{}] ({})", i + 2, i, argv[i]);
        args[i + 2] = argv[i];
    }

    args[argc + 2] = nullptr; // null-terminate the array

    execvp(args[0], args);

    // If execvp returns, it means an error occurred
    perror("execvp");
    return false;
}

int installPkg(alpm_list_t *pkgNames) {
    bool useGit   = config->useGit;
    string cacheDir = config->cacheDir;

    bool returnStatus = true;

    vector<string> pacmanPkgs; // list of pacman packages to install, to avoid spamming pacman.

    for (; pkgNames; pkgNames = pkgNames->next) {
        string            pkgName = string((char*)(pkgNames->data));
        vector<TaurPkg_t> pkgs    = backend->search(pkgName, useGit, !op.op_s_search);

        if (pkgs.empty()) {
            log_println(LOG_WARN, "No results found!");
            returnStatus = false;
            continue;
        }

        // ./taur -Ss -- list only, don't install.
        if (op.op_s_search) {
            for (size_t i = 0; i < pkgs.size(); i++)
                printPkgInfo(pkgs[i], pkgs[i].db_name);
            returnStatus = false;
            continue;
        }

        optional<vector<TaurPkg_t>> oSelectedPkgs = askUserForPkg(pkgs, *backend, useGit);

        if (!oSelectedPkgs) {
            returnStatus = false;
            continue;
        }

        vector<TaurPkg_t> selectedPkgs = oSelectedPkgs.value();

        for (size_t i = 0; i < selectedPkgs.size(); i++) {
            TaurPkg_t pkg = selectedPkgs[i];

            string    url = pkg.url;

            if (url == "") {
                log_printf(LOG_DEBUG, "Scheduled system package {} for installation!\n", pkg.name);
                pacmanPkgs.push_back(pkg.name);
                continue;
                // vector<const char *> cmd = {config->sudo.c_str(), "pacman", "-S"};
                // if(op.op_s_sync)
                //     cmd.push_back("-y");
                // if(op.op_s_upgrade)
                //     cmd.push_back("-u");

                // cmd.push_back(pkg.name.c_str());

                // if (taur_exec(cmd, false))
                //     continue;
                // else {
                //     returnStatus = false;
                //     continue;
                // }
            }

            string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

            if (useGit) {
                filename = filename.substr(0, filename.rfind(".git"));

                if (std::filesystem::exists(filename) && std::filesystem::exists(path(filename) / ".git")) {
                    std::filesystem::current_path(filename);
                    if (askUserYorN(true, PROMPT_YN_DIFF, pkg.name)) {
                        if (!taur_exec({config->git.c_str(), "fetch", "origin"}, false)) {
                            log_println(LOG_ERROR, "Failed to run `{} fetch`!", config->git);
                            return false;
                        }
                        if (!taur_exec({config->git.c_str(), "diff", "origin", "PKGBUILD"}, false)) {
                            log_println(LOG_ERROR, "Failed to run `{} diff`!", config->git);
                            return false;
                        }

                        // make sure the user 100% can read the diff.
                        sleep(3);
                    }
                }
            }

            bool stat = backend->download_pkg(url, filename);

            if (!stat) {
                log_println(LOG_ERROR, "An error has occurred and we could not download your package.");
                returnStatus = false;
                continue;
            }

            if (!useGit) {
                stat = backend->handle_aur_depends(pkg, cacheDir, backend->get_all_local_pkgs(true), useGit);
                stat = backend->build_pkg(pkg.name, filename.substr(0, filename.rfind(".tar.gz")), false);
            } else {
                stat = backend->handle_aur_depends(pkg, cacheDir, backend->get_all_local_pkgs(true), useGit);
                stat = backend->build_pkg(pkg.name, filename, false);
            }

            if (!stat) {
                log_println(LOG_ERROR, "Building your package has failed.");
                returnStatus = false;
                continue;
            }

            log_println(LOG_DEBUG, "Installing {}.", pkg.name);
            if (!pacman_exec("-U", split(built_pkg, ' '), false)) {
                log_println(LOG_ERROR, "Failed to install {}.", pkg.name);
                returnStatus = false;
                continue;
            }
        }
    }

    if (!config->aurOnly && (op.op_s_upgrade || !pacmanPkgs.empty())) {
        log_printf(LOG_DEBUG, "{} system packages!\n", (op.op_s_upgrade ? "Upgrading" : "Installing"));
        string op_s = "-S";

        if (op.op_s_sync)
            op_s += "y";
        if (op.op_s_upgrade)
            op_s += "u";

        pacman_exec(op_s, pacmanPkgs);
    }

    if (op.op_s_upgrade) {
        log_println(LOG_INFO, "-u flag specified, upgrading AUR packages.");
        return backend->update_all_aur_pkgs(cacheDir, useGit) && returnStatus;
    }

    return returnStatus;
}

bool removePkg(alpm_list_t *pkgNames) {
    if (!pkgNames)
        return false;

    alpm_list_t *exactMatches = nullptr;
    alpm_list_t *searchResults = nullptr;
    alpm_list_t *filteredPkgNames = nullptr;
    alpm_db_t *localdb = alpm_get_localdb(config->handle);

    for (alpm_list_t *i = pkgNames; i; i = i->next) {
        alpm_pkg_t *result = alpm_db_get_pkg(localdb, (const char *)i->data);
        if (result != NULL)
            exactMatches = alpm_list_add(exactMatches, result);
        else
            filteredPkgNames = alpm_list_add(filteredPkgNames, i->data);
    }

    if (filteredPkgNames && alpm_db_search(localdb, filteredPkgNames, &searchResults) != 0) {
        if (exactMatches)
            alpm_list_free(exactMatches);
        alpm_list_free(filteredPkgNames);
        log_println(LOG_ERROR, "Searching failed, Bailing out!");
        return false;
    }

    alpm_list_smart_pointer ret(alpm_list_join(exactMatches, searchResults), alpm_list_free);

    size_t ret_length = alpm_list_count(ret.get());

    if (ret_length == 0) {
        log_println(LOG_ERROR, "No packages found!");
        return false;
    }

    if (ret_length == 1)
        return backend->remove_pkg((alpm_pkg_t *)ret->data);

    fmt::println("Choose packages to remove, (Seperate by spaces, type * to remove all):");

    for (size_t i = 0; i < ret_length; i++) {
        fmt::println("[{}] {}", i, alpm_pkg_get_name((alpm_pkg_t *)(alpm_list_nth(ret.get(), i)->data)));
    }

    string included;
    std::getline(std::cin, included);

    vector<string> includedIndexes = split(included, ' ');

    alpm_list_t *finalPackageList      = nullptr;
    alpm_list_t *finalPackageListStart = nullptr;

    if (included == "*")
        return backend->remove_pkgs(ret);

    for (size_t i = 0; i < includedIndexes.size(); i++) {
        try {
            size_t includedIndex = (size_t)stoi(includedIndexes[i]);

            if (includedIndex >= ret_length)
                continue;

            finalPackageList = alpm_list_add(finalPackageList, alpm_list_nth(ret.get(), includedIndex)->data);

            if (finalPackageList != nullptr && finalPackageListStart == nullptr)
                finalPackageListStart = finalPackageList;

        } catch (std::invalid_argument const&) {
            log_println(LOG_WARN, "Invalid argument! Assuming all.");

            if (finalPackageListStart != nullptr)
                alpm_list_free(finalPackageListStart);
            else if (finalPackageList != nullptr)
                alpm_list_free(finalPackageList);

            return backend->remove_pkgs(ret);
        }
    }

    // take control of the list and pass it to the smart pointer
    alpm_list_smart_pointer finalList = make_list_smart_pointer(finalPackageListStart);

    return backend->remove_pkgs(finalList);
}

bool updateAll() {
    return backend->update_all_aur_pkgs(config->cacheDir, config->useGit);
}

bool queryPkgs() {
    log_println(LOG_DEBUG, "AUR Only: {}", config->aurOnly);
    alpm_list_t *pkg;

    vector<const char*> pkgs, pkgs_ver;
    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config->handle)); pkg; pkg = alpm_list_next(pkg)) {
        pkgs.push_back(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data)));
        pkgs_ver.push_back(alpm_pkg_get_version((alpm_pkg_t *)(pkg->data)));
    }

    if (config->aurOnly) {
        alpm_list_t *syncdbs = alpm_get_syncdbs(config->handle);

        if (!syncdbs) {
            log_println(LOG_ERROR, "Failed to get syncdbs!");
            return false;
        }

        for (; syncdbs; syncdbs = syncdbs->next)
            for (size_t i = 0; i < pkgs.size(); i++)
                if (alpm_db_get_pkg((alpm_db_t*)(syncdbs->data), pkgs[i]))
                    pkgs[i] = NULL; // wont be printed
    }

    if (config->quiet) {
        for (size_t i = 0; i < pkgs.size(); i++) {
            if (!pkgs[i])
                continue;
            fmt::println("{}", pkgs[i]);
        }
    } else {
        for (size_t i = 0; i < pkgs.size(); i++) {
            if (!pkgs[i])
                continue;
            fmt::print(fmt::emphasis::bold, "{} ", pkgs[i]);
            fmt::println(BOLD_TEXT(color.green), "{}", pkgs_ver[i]);
        }
    }

    return true;
}

int parseargs(int argc, char* argv[]) {
    // default
    op.op = OP_MAIN;

    int opt = 0;
    int option_index = 0;
    int result = 0;
    const char *optstring = "DFQRSTUVahqsuryt";
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
        {"aur-only",   no_argument,       0, 'a'},
        {"help",       no_argument,       0, 'h'},
        {"test-colors",no_argument,       0, 't'},

        {"refresh",    no_argument,       0, OP_REFRESH},
        {"sysupgrade", no_argument,       0, OP_SYSUPGRADE},
        {"search",     no_argument,       0, OP_SEARCH},
        {"cachedir",   required_argument, 0, OP_CACHEDIR},
        {"colors",     required_argument, 0, OP_COLORS},
        {"config",     required_argument, 0, OP_CONFIG},
        {"theme",      required_argument, 0, OP_THEME},
        {"sudo",       required_argument, 0, OP_SUDO},
        {"use-git",    required_argument, 0, OP_USEGIT},
        {"quiet",      no_argument,       0, OP_QUIET},
        {"debug",      no_argument,       0, OP_DEBUG},
        {"recipe",     no_argument,       0, OP_RECIPE},
        {"noconfirm",  no_argument,       0, OP_NOCONFIRM},
        {0,0,0,0}
    };

    /* parse operation */
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1) {
        if (opt == 0)
            continue;
        else if (opt == '?')
            return 1;
        parsearg_op(opt, 0);

        if (op.op == 0) {
            log_println(LOG_ERROR, "only one operation may be used at a time");
            return 1;
        }

        if (op.version) {
            fmt::println("TabAUR version {}, branch {}", VERSION, BRANCH);
            exit(0);
        }
        if (op.help) {
            usage(op.op);
            exit(1);
        }

        /* parse all other options */
        switch(op.op) {
            case OP_SYNC:
                result = parsearg_sync(opt);
                break;
            case OP_QUERY:
                result = parsearg_query(opt);
                break;
            default:
            result = 1;
            break;
        }
    }

    optind = 1;
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1) {
        if (opt == 0)
            continue;
        
        /* this should have failed during first pass already */
        else if (opt == '?')
            return 1;
        
        /* opt is an operation */
        else if (parsearg_op(opt, 1) == 0)
            continue;

        if (result == 0)
            continue;

        /* fall back to global options */
        result = parsearg_global(opt);
        if (result != 0) {
            if (result == 1) {
                /* global option parsing failed, abort */
                if (opt < 1000) {
                    log_println(LOG_NONE, "Error: invalid option '-{}'", (char)opt);
                } else {
                    log_println(LOG_NONE, "Error: invalid option '--{}'", opts[option_index].name);
                }
            }
            return 1;
        }
    }

    while (optind < argc) {
        /* add the target to our target array */
        alpm_list_t *taur_targets_get = taur_targets.get();
        (void)taur_targets.release();
        alpm_list_append_strdup(&taur_targets_get, argv[optind]);
        taur_targets = make_list_smart_deleter(taur_targets_get);
        optind++;
    }

    return 0;
}

// main
int main(int argc, char* argv[]) {
    config         = std::make_unique<Config>();
    char* no_color = getenv("NO_COLOR");

    configfile = (getConfigDir() + "/config.toml");
    themefile  = (getConfigDir() + "/theme.toml");

    if (parseargs(argc, argv))
        return 1;

    config->init(configfile, themefile);

    if (no_color != NULL && no_color[0] != '\0') {
        fmt::disable_colors = true;
        config->colors      = false;
    }

    if (op.test_colors) {
        test_colors();
        return 0;
    }

    signal(SIGINT, &interruptHandler);

    // the code you're likely interested in
    backend = std::make_unique<TaurBackend>(*config);

    if (op.requires_root && geteuid() != 0) {
        log_println(LOG_ERROR, "You need to be root to do this.");
        return 1;
    } else if (!op.requires_root && geteuid() == 0) {
        log_println(LOG_ERROR, "You are trying to run TabAUR as root when you don't need it.");
        return 1;
    }

    switch (op.op) {
    case OP_SYNC:
        return installPkg(taur_targets.get()) ? 0 : 1;
    case OP_REM:
        return removePkg(taur_targets.get()) ? 0 : 1;
    case OP_QUERY:
        return (queryPkgs()) ? 0 : 1;
    case OP_SYSUPGRADE:
        return (updateAll()) ? 0 : 1;
    case OP_PACMAN:
        // we are gonna use pacman to other ops than -S,-R,-Q
        return execPacman(argc, argv);
    default:
        log_println(LOG_ERROR, _("no operation specified (use {} -h for help)"), argv[0]);
        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}
