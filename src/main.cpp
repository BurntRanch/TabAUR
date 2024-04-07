#pragma GCC diagnostic ignored "-Wvla"

#include "util.hpp"
#include "args.hpp"
#include "taur.hpp"

#define BRANCH "libalpm-test"
#define VERSION "0.1.0"

std::unique_ptr<Config> config;

void usage(int op) {
    string help_op = R"#(
options:
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
    )#";

    log_printf(LOG_NONE, "TabAUR usage: taur <op> [...]\n");
    
    if(op == OP_MAIN)
        log_printf(LOG_NONE, "{}", help_op);
    else if (op == OP_SYNC)
        log_printf(LOG_NONE, "-S,--sync test\n");

    log_printf(LOG_NONE, "TabAUR will assume -Syu if you pass no arguments to it.\n\n");

    if (config->secretRecipe) {
        log_printf(LOG_INFO, "Loading secret recipe...\n");
        for (auto const& i : secret) {
            log_printf(LOG_NONE, "{}\n", i);
            usleep(650000); // 0.65 seconds
        }
    }
}

void test_colors() {
    if(fmt::disable_colors)
        log_printf(LOG_NONE, "Colors are disabled\n");
    log_printf(LOG_DEBUG, "Debug color: {}\n",  fmt::format(BOLD_TEXT(config->getThemeValue("magenta", magenta)), "(bold) magenta"));
    log_printf(LOG_INFO, "Info color: {}\n",    fmt::format(BOLD_TEXT(config->getThemeValue("cyan", cyan)), "(bold) cyan"));
    log_printf(LOG_WARN, "Warning color: {}\n", fmt::format(BOLD_TEXT(config->getThemeValue("yellow", yellow)), "(bold) yellow"));
    log_printf(LOG_ERROR, "Error color: {}\n",  fmt::format(BOLD_TEXT(config->getThemeValue("red", red)), "(bold) red"));
    log_printf(LOG_NONE, "red: {}\n",    fmt::format(fg(config->getThemeValue("red", red)), config->getThemeHexValue("red", red)));
    log_printf(LOG_NONE, "blue: {}\n",   fmt::format(fg(config->getThemeValue("blue", blue)), config->getThemeHexValue("blue", blue)));
    log_printf(LOG_NONE, "yellow: {}\n", fmt::format(fg(config->getThemeValue("yellow", yellow)), config->getThemeHexValue("yellow", yellow)));
    log_printf(LOG_NONE, "green: {}\n",  fmt::format(fg(config->getThemeValue("green", green)), config->getThemeHexValue("green", green)));
    log_printf(LOG_NONE, "cyan: {}\n",   fmt::format(fg(config->getThemeValue("cyan", cyan)), config->getThemeHexValue("cyan", cyan)));
    log_printf(LOG_NONE, "magenta: {}\n",fmt::format(fg(config->getThemeValue("magenta", magenta)), config->getThemeHexValue("magenta", magenta)));
}

bool execPacman(int argc, char* argv[]) {
    log_printf(LOG_DEBUG, "Passing command to pacman! (argc: {:d})\n", argc);
    
    if (0 > argc || argc > 512)
        throw std::invalid_argument("argc is invalid! (512 > argc > 0)");

    char* args[argc+3]; // sudo + pacman + null terminator

    args[0] = _(config->sudo.c_str());
    args[1] = _("pacman"); // The command to run as superuser (pacman)
    for (int i = 0; i < argc; ++i) {
        log_printf(LOG_DEBUG, "args[{}] = argv[{}] ({})\n", i + 2, i, argv[i]);
        args[i+2] = argv[i];
    }

    args[argc+2] = nullptr; // null-terminate the array

    execvp(args[0], args);

    // If execvp returns, it means an error occurred
    perror("execvp");
    return false;
}

int installPkg(string pkgName, TaurBackend *backend) { 
    bool            useGit   = config->useGit;
    string          cacheDir = config->cacheDir;

    vector<TaurPkg_t>  pkgs = backend->search(pkgName, useGit);

    if (pkgs.empty() && !op.op_s_upgrade) {
        log_printf(LOG_WARN, "No results found, Exiting!\n");
        return -1;
    } else if (pkgs.empty()) {
        vector<const char *> cmd = {config->sudo.c_str(), "pacman", "-S"};
        if(op.op_s_sync)
            cmd.push_back("-y");
        if(op.op_s_upgrade)
            cmd.push_back("-u");

        return taur_exec(cmd) && backend->update_all_pkgs(cacheDir, useGit);
    }

    // ./taur -Ss -- list only, don't install.
    if (op.op_s_search) {
        for (size_t i = 0; i < pkgs.size(); i++)
            printPkgInfo(pkgs[i]);
        return true;
    }

    optional<TaurPkg_t> oPkg = askUserForPkg(pkgs, *backend, useGit);

    if (!oPkg)
        return false;

    TaurPkg_t pkg = oPkg.value();

    string url = pkg.url;

    if (url == "") {
        vector<const char*> cmd = {"-S"};
        if(op.op_s_sync)
            cmd.push_back("-y");
        if(op.op_s_upgrade)
            cmd.push_back("-u");

        cmd.push_back(pkg.name.c_str());

        return execPacman(cmd.size(), (char **)cmd.data()), backend->update_all_pkgs(config->cacheDir, useGit);
    }

    string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

    if (useGit)
        filename = filename.substr(0, filename.rfind(".git"));

    bool stat = backend->download_pkg(url, filename);

    if (!stat) {
        log_printf(LOG_ERROR, "An error has occurred and we could not download your package.\n");
        return false;
    }

    if (!useGit){
        stat = backend->handle_aur_depends(pkg, filename.substr(0, filename.rfind(".tar.gz")), useGit);
        stat = backend->install_pkg(pkg.name, filename.substr(0, filename.rfind(".tar.gz")));
    }
    else {
        stat = backend->handle_aur_depends(pkg, filename, useGit);
        stat = backend->install_pkg(pkg.name, filename);
    }

    if (!stat) {
        log_printf(LOG_ERROR, "Building/Installing your package has failed.\n");
        return false;
    }

    if (op.op_s_upgrade) {
        log_printf(LOG_INFO, "-u flag specified, upgrading AUR packages.\n");
        return backend->update_all_pkgs(cacheDir, useGit);
    }

    return true;
}

bool removePkg(string pkgName, TaurBackend *backend) {
    return backend->remove_pkg(pkgName, config->aurOnly);
}

bool updateAll(TaurBackend *backend) {
    string cacheDir = config->cacheDir;

    return backend->update_all_pkgs(path(cacheDir), config->useGit);
}

bool queryPkgs(TaurBackend *backend) {
    log_printf(LOG_DEBUG, "AUR Only: {}\n", config->aurOnly);
    alpm_list_t *pkg, *syncdbs;

    syncdbs = alpm_get_syncdbs(config->handle);

    if (!syncdbs) {
        log_printf(LOG_ERROR, "Failed to get syncdbs!\n");
        return false;
    }

    vector<const char *> pkgs, pkgs_ver;
    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config->handle)); pkg; pkg = alpm_list_next(pkg)){
        pkgs.push_back(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data)));
        pkgs_ver.push_back(alpm_pkg_get_version((alpm_pkg_t *)(pkg->data)));
    }

    if (config->aurOnly)
        for (; syncdbs; syncdbs = syncdbs->next)
            for (size_t i = 0; i < pkgs.size(); i++)
                if (alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), pkgs[i]))
                    pkgs[i] = NULL; // wont be printed

    for (size_t i = 0; i < pkgs.size(); i++) {
        if (!pkgs[i])
            continue;
        log_printf(LOG_NONE, fmt::emphasis::bold, "{} ", pkgs[i]);
        log_printf(LOG_NONE, BOLD_TEXT(config->getThemeValue("green", green)), "{}\n", pkgs_ver[i]);
    }

    return true;
}

int parseargs(int argc, char* argv[]) {
    // default
    op.op = OP_MAIN;

    int opt = 0;
    int option_index = 0;
    int result = 0;
    const char *optstring = "SRQVDFTUahyust";
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
        {"use-git",    no_argument,       0, OP_USEGIT},
        {"debug",      no_argument,       0, OP_DEBUG},
        {0,0,0,0}
    };

    /* parse operation */
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1) {
        if (opt == 0) {
            continue;
        } else if (opt == '?') {
            return 1;
        }
        parsearg_op(opt, 0);
    }

    if(op.op == 0) {
		log_printf(LOG_ERROR, "only one operation may be used at a time\n");
		return 1;
    }

    if (op.version) {
        log_printf(LOG_NONE, "TabAUR version {}, branch {}\n", VERSION, BRANCH);
        exit(0);
    }
    if(op.help) {
		usage(op.op);
		exit(0);
    }
        
    /* parse all other options */
	optind = 1;
	while((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1) {
		if(opt == 0) {
			continue;
		} else if(opt == '?') {
			/* this should have failed during first pass already */
			return 1;
		} else if(parsearg_op(opt, 1) == 0) {
			/* opt is an operation */
			continue;
		}

		switch(op.op) {
            case OP_SYNC:
                result = parsearg_sync(opt);
                break;
            default:
				result = 1;
				break;
        }

        if(result == 0)
		continue;

        /* fall back to global options */
	    result = parsearg_global(opt);
	    if(result != 0) {
		    if(result == 1) {
		    /* global option parsing failed, abort */
			    if(opt < 1000) {
				    log_printf(LOG_ERROR, "invalid option '-{}'\n", opt);
			    } else {
				    log_printf(LOG_ERROR, "invalid option '--{}'\n", opts[option_index].name);
			    }
		    }
            return 1;
	    }       
    }

    while(optind < argc) {
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
    config = std::make_unique<Config>();
    configfile = def_conffile;
    themefile = def_themefile;

    config->initializeVars();
    fmt::disable_colors = config->colors == 0;

    if (parseargs(argc, argv))
        return 1;
    
    config->loadConfigFile(configfile);
    config->loadThemeFile(themefile);

    signal(SIGINT, &interruptHandler);

    TaurBackend backend(*config);
    
    
    if (!fs::exists(config->cacheDir)) {
        log_printf(LOG_WARN, "TabAUR cache folder was not found, Creating folders at {}!\n", config->cacheDir);
        fs::create_directories(config->cacheDir);
    }

    if(op.test_colors) {
        test_colors();
        return 0;
    }

    if (op.requires_root && geteuid() != 0) {
        log_printf(LOG_ERROR, "You need to be root to do this.\n");
        return 1;
    } else if (!op.requires_root && geteuid() == 0) {
        log_printf(LOG_ERROR, "You are trying to run TabAUR as root when you don't need it.\n");
        return 1;
    }

    switch (op.op) {
    case OP_SYNC:
        return (installPkg(taur_targets ? string((const char *)(taur_targets->data)) : "", &backend)) ? 0 : 1;
    case OP_REM:
        return (removePkg(taur_targets ? string((const char *)(taur_targets->data)) : "", &backend)) ? 0 : 1;
    case OP_QUERY:
        return (queryPkgs(&backend)) ? 0 : 1;
    case OP_SYSUPGRADE:
        return (updateAll(&backend)) ? 0 : 1;
    case OP_PACMAN:
        // we are gonna use pacman to other ops than -S,-R,-Q
        return execPacman(argc, argv);
    }

    return 3;
}
