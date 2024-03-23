#include <csignal>
#include <cstdlib>
#include <memory>
#include <stdbool.h>
#include <string>
#include <unistd.h>
#include <signal.h>

#include "util.hpp"
#include "args.hpp"
#include "taur.hpp"
#include "config.hpp"

using std::cout;
using std::endl;
using std::filesystem::path;
using std::vector;
using std::string;
using std::optional;

std::unique_ptr<Config> config;
struct Operation_t operation;

void interruptHandler(int) {
    log_printf(LOG_WARN, _("Caught CTRL-C, Exiting!\n"));

    std::exit(-1);
}

void usage() {
    string help_op = R"#(
operations:
    taur {-h --help}
    taur {-V --version}
    taur {-D --database} <options> <package(s)>
    taur {-F --files}    [options] [file(s)]
    taur {-Q --query}    [options] [package(s)]
    taur {-R --remove}   [options] <package(s)>
    taur {-S --sync}     [options] [package(s)]
    taur {-T --deptest}  [options] [package(s)]
    taur {-U --upgrade}  [options] <file(s)>
    )#";
    cout << "TabAUR usage: taur <operation> [...]" << endl;
    cout << help_op << endl;
    cout << "TabAUR will assume -Syu if you pass no arguments to it." << endl << endl;
    if(config->secretRecipe){
        log_printf(LOG_INFO, _("Loading secret recipe...\n"));
        for(auto const& i : secret){
            cout << i << endl;
            usleep(650000); // 0.65 seconds
        }
    }
}

int installPkg(string pkgName, TaurBackend *backend) { 
    bool            useGit   = config->useGit;
    
    optional<TaurPkg_t>  pkg = backend->search(pkgName, useGit);

    if (!pkg) {
        log_printf(LOG_ERROR, _("An error has occurred and we could not search for your package.\n"));
        return -1;
    }

    string url = pkg.value().url;

    if (url == "") {
        // we search for the package name and print only the name, not the description
        alpm_list_t *pkg_cache, *syncdbs;
        string packages_str;

        syncdbs = config->repos;
        
        log_printf(LOG_INFO, _("Synchronizing databases, This might take a while.\n"));
        
        alpm_list_t *dbs = alpm_get_syncdbs(config->handle);
        int ret = alpm_db_update(config->handle, dbs, false);
        
        if(ret < 0) {
            log_printf(LOG_ERROR, _("Failed to synchronize all databases (%s)\n"),
                    alpm_strerror(alpm_errno(config->handle)));
            return false;
        }
        log_printf(LOG_INFO, _("Synchronized all databases.\n"));
        log_printf(LOG_INFO, _("Downloading package..\n"));

        alpm_pkg_t * alpm_pkg;
        bool found = false;

        for (; syncdbs && !found; syncdbs = alpm_list_next(syncdbs)) {
            for (pkg_cache = alpm_db_get_pkgcache((alpm_db_t *)(syncdbs->data)); pkg_cache; pkg_cache = alpm_list_next(pkg_cache)) {
                string name = string(alpm_pkg_get_name((alpm_pkg_t *)(pkg_cache->data)));
                if (name == pkg.value().name) {
                    alpm_pkg = (alpm_pkg_t *)(pkg_cache->data);
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            log_printf(LOG_ERROR, _("backend->search returned a system package but we couldn't find it.\n"));
            return false;
        }

        int transCode = alpm_trans_init(config->handle, 0);

        if (transCode != 0) {
            log_printf(LOG_ERROR, _("Failed to start transaction (%s)\n"), alpm_strerror(alpm_errno(config->handle)));
            return false;
        }

        if (alpm_add_pkg(config->handle, alpm_pkg) != 0) {
            log_printf(LOG_ERROR, _("Failed to add package (%s)\n"), alpm_strerror(alpm_errno(config->handle)));
            alpm_trans_release(config->handle);
            return false;
        }
        
        bool success = commitTransactionAndRelease(config->handle);

        if (!success) {
            log_printf(LOG_ERROR, _("Failed to prepare, commit, or release transaction!\n"));
            return false;
        }

        log_printf(LOG_INFO, _("Successfully installed package!\n"));

        return true;
    }

    string real_uid = getenv("SUDO_UID");
    int realuid = std::stoi(real_uid);
    if (!real_uid.empty() && realuid > 0) {
        log_printf(LOG_WARN, _("You are trying to install an AUR package with sudo, This is unsupported by makepkg.\nWe will try to reduce your privilege, but there is no guarantee it'll work."));
        setuid(realuid);
        setenv("HOME", ("/home/" + string(getenv("SUDO_USER"))).c_str(), 1);
        config->initializeVars();
    }

    string      	cacheDir = config->cacheDir;

    string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

    if (useGit)
        filename = filename.substr(0, filename.rfind(".git"));

    bool stat = backend->download_pkg(url, filename);

    if (!stat) {
        log_printf(LOG_ERROR, _("An error has occurred and we could not download your package.\n"));
        return false;
    }

    if (!useGit)
        stat = backend->install_pkg(pkg.value(), filename.substr(0, filename.rfind(".tar.gz")), useGit);
    else
        stat = backend->install_pkg(pkg.value(), filename, useGit);

    if (!stat) {
        log_printf(LOG_ERROR, _("Building/Installing your package has failed.\n"));
        return false;
    }

    return true;
}

bool removePkg(string pkgName, TaurBackend *backend) {
    return backend->remove_pkg(pkgName, config->aurOnly);
}

bool updateAll(TaurBackend *backend) {
    string          cacheDir = config->cacheDir;

    return backend->update_all_pkgs(path(cacheDir), config->useGit);
}

bool execPacman(int argc, char* argv[]){
    char* args[argc];
    args[0] = _("pacman"); // Set the command name to pacman
    for(int i = 1; i < argc; ++i) {
        args[i] = argv[i];
    }
    args[argc] = nullptr; // null-terminate the array
    for(int i = 0; i < argc; ++i)
            cout << "args: " << args[i] << endl;
    execvp(args[0], args);

    // If execvp returns, it means an error occurred
    perror("execvp");
    return false;    
}

int parsearg_op(int opt){
    switch(opt){
        case 'S':
            operation.op = OP_SYNC; operation.args.push_back(optarg); break;
        case 'R':
            operation.op = OP_REM; operation.args.push_back(optarg); break;
        case 'Q':
            operation.op = OP_QUERY; break;
        case 'a':
            config->aurOnly = true; break;
        case 'h':
            usage(); exit(0); break;
        case 'V':
            std::cout << "TabAUR version 0.0.1" << std::endl; break;
        case 'D':
        case 'T':
        case 'U':
        case 'F':
            operation.op = OP_PACMAN; break;
        case ':':
            std::cerr << "Option requires an argument!" << std::endl; break;
        default:
            return 1;
    }
    return 0;
}

int parseargs(int argc, char* argv[]){
    // default
    operation.op = OP_SYSUPGRADE;

    int opt;
    int option_index = 0;
	const char *optstring = "S:R:QVD:F:T:U:ah";
	static const struct option opts[] = 
    {
        {"sync",       required_argument, 0, 'S'},
        {"remove",     required_argument, 0, 'R'},
        {"query",      required_argument, 0, 'Q'},
        {"version",    required_argument, 0, 'V'},
        {"database",   required_argument, 0, 'D'},
        {"files",      required_argument, 0, 'F'},
        {"deptest",    required_argument, 0, 'T'}, /* used by makepkg */
		{"upgrade",    required_argument, 0, 'U'},

        {"aur-only",   no_argument,       0, 'a'},
        {"help",       no_argument,       0, 'h'},
        {0,0,0,0}
    };

    /* parse operation */
	while((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1) {
		if(opt == 0) {
			continue;
		} else if(opt == '?') {
			return 1;
		}
		parsearg_op(opt);
	}

    if ((operation.op == OP_SYNC || operation.op == OP_REM) && operation.args.size() < 1) {
       usage(); 
       return 1;
    }

    return 0;
}

bool queryPkgs(TaurBackend *backend) {
    vector<string> pkgs = backend->list_all_local_pkgs(config->aurOnly, false);

    for (size_t i = 0; i < pkgs.size(); i++)
        cout << pkgs[i] << endl;

    return true;
}

// main
int main(int argc, char* argv[]) {
    config = std::make_unique<Config>();

    signal(SIGINT, &interruptHandler);

    TaurBackend backend(*config);
    
    if (parseargs(argc, argv))
        return 1;

    switch (operation.op) {
    case OP_SYNC:
        return (installPkg(operation.args[0], &backend)) ? 0 : 1;
    case OP_REM:
        return (removePkg(operation.args[0], &backend)) ? 0 : 1;
    case OP_QUERY:
        return (queryPkgs(&backend)) ? 0 : 1;
    case OP_SYSUPGRADE:
        return (updateAll(&backend)) ? 0 : 1;
    case OP_PACMAN:
        // we are gonna use pacman to other operations than -S,-R,-Q
        return execPacman(argc, argv);
    }

    return 3;
}
