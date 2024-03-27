#include <alpm.h>
#define BRANCH "libalpm-test"
#define VERSION "0.1.0"

#include <stdbool.h>
#include <signal.h>

#include "util.hpp"
#include "args.hpp"
#include "taur.hpp"

using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::optional;

std::unique_ptr<Config> config;
struct Operation_t operation;

void interruptHandler(int) {
    log_printf(LOG_WARN, "Caught CTRL-C, Exiting!\n");

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
    log_printf(LOG_INFO, "test");
    if (config->secretRecipe) {
        log_printf(LOG_INFO, "Loading secret recipe...\n");
        for (auto const& i : secret) {
            cout << i << endl;
            usleep(650000); // 0.65 seconds
        }
    }
}

bool execPacman(int argc, char* argv[]) {
    log_printf(LOG_DEBUG, "Passing command to pacman! (argc: %d)\n", argc);
    char* args[argc+3]; // sudo + pacman + null terminator
  
    args[0] = _(config->sudo.c_str());
    args[1] = _("pacman"); // The command to run as superuser (pacman)
    for (int i = 0; i < argc; ++i) {
        log_printf(LOG_DEBUG, "args[%d] = argv[%d] (%s)\n", i + 2, i, argv[i]);
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
    
    optional<TaurPkg_t>  oPkg = backend->search(pkgName, useGit);

    if (!oPkg) {
        log_printf(LOG_ERROR, "An error has occurred and we could not search for your package.\n");
        return -1;
    }

    TaurPkg_t pkg = oPkg.value();

    string url = pkg.url;

    if (url == "") {
        // -S <pkg name>
        char *argv[2] = {(char *)"-S", (char *)pkg.name.c_str()};
        return execPacman(2, argv);
    }

    // This was here because in past versions, libalpm was being used next to makepkg, which one requires sudo, and the other requires normal user privileges.
    // I'll keep this here as it's convenient for some users.
    // TODO: Add an option to suppress its messages or disable it entirely.
    char *real_uid = getenv("SUDO_UID");
    if (real_uid) {
        int realuid = std::atoi(real_uid);
        if (realuid > 0) {
            log_printf(LOG_WARN, "You are trying to install an AUR package with sudo, This is unsupported by makepkg.\n We will try to reduce your privilege, but there is no guarantee it'll work.\n");
            setuid(realuid);
            setenv("HOME", ("/home/" + string(getenv("SUDO_USER"))).c_str(), 1);
            config->initializeVars();
        }
    }

    string cacheDir = config->cacheDir;

    string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

    if (useGit)
        filename = filename.substr(0, filename.rfind(".git"));

    bool stat = backend->download_pkg(url, filename);

    if (!stat) {
        log_printf(LOG_ERROR, "An error has occurred and we could not download your package.\n");
        return false;
    }

    if (!useGit)
        stat = backend->install_pkg(pkg, filename.substr(0, filename.rfind(".tar.gz")), useGit);
    else
        stat = backend->install_pkg(pkg, filename, useGit);

    if (!stat) {
        log_printf(LOG_ERROR, "Building/Installing your package has failed.\n");
        return false;
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

int parsearg_op(int opt) {
    switch (opt) {
        case 'S':
            operation.op = OP_SYNC;
            operation.args.push_back(optarg);
            break;
        case 'R':
            operation.op = OP_REM;
            operation.requires_root = true;
            operation.args.push_back(optarg);
            break;
        case 'Q': operation.op = OP_QUERY; break;
        case 'a': config->aurOnly = true; break;
        case 'h':
            usage();
            exit(0);
            break;
        case 'V': std::cout << "TabAUR version " << VERSION << " (" << BRANCH << " branch)" << std::endl; exit(0); break;
        case 'D':
        case 'T':
        case 'U':
        case 'F': operation.op = OP_PACMAN; break;
        case ':': std::cerr << "Option requires an argument!" << std::endl; break;
        default: return 1;
    }
    return 0;
}

int parseargs(int argc, char* argv[]) {
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
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1) {
        if (opt == 0) {
            continue;
        } else if (opt == '?') {
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
    alpm_list_t *pkg, *syncdbs;

    syncdbs = alpm_get_syncdbs(config->handle);

    if (!syncdbs) {
        log_printf(LOG_ERROR, "Failed to get syncdbs!\n");
        return false;
    }

    vector<const char *> pkgs;
    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config->handle)); pkg; pkg = alpm_list_next(pkg))
        pkgs.push_back(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data)));
    
    if (config->aurOnly)
        for (; syncdbs; syncdbs = syncdbs->next)
            for (size_t i = 0; i < pkgs.size(); i++)
                if (alpm_db_get_pkg((alpm_db_t *)(syncdbs->data), pkgs[i]))
                    pkgs[i] = NULL; // wont be printed
    
    for (size_t i = 0; i < pkgs.size(); i++) {
        if (!pkgs[i])
            continue;
        std::cout << pkgs[i] << std::endl;
    }

    return true;
}

// main
int main(int argc, char* argv[]) {
    config = std::make_unique<Config>();

    signal(SIGINT, &interruptHandler);

    TaurBackend backend(*config);
    
    if (parseargs(argc, argv))
        return 1;

    if (operation.requires_root && geteuid() != 0) {
        log_printf(LOG_ERROR, "You need to be root to do this.\n");
        return 1;
    }

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
