#include <stdbool.h>
#include <unistd.h>

#include "util.hpp"
#include "args.hpp"
#include "taur.hpp"

using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::optional;

Config config;
struct Operation_t operation;

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
    if (config.secretRecipe) {
        log_printf(LOG_INFO, "Loading secret recipe...\n");
        for (auto const& i : secret) {
            cout << i << endl;
            usleep(650000); // 0.65 seconds
        }
    }
}

int installPkg(string pkgName, TaurBackend* backend) {
    string              cacheDir = config.cacheDir;
    bool                useGit   = config.useGit;

    optional<TaurPkg_t> pkg = backend->search(pkgName, useGit);

    if (!pkg) {
        log_printf(LOG_ERROR, "An error has occurred and we could not search for your package.\n");
        return -1;
    }

    string url = pkg.value().url;

    if (url == "") {
        string name = pkg.value().name;
        sanitizeStr(config.sudo);
        sanitizeStr(name);
        return taur_exec({config.sudo.c_str(), "pacman", "-S", name.c_str()});
    }

    string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

    if (useGit)
        filename = filename.substr(0, filename.rfind(".git"));

    bool stat = backend->download_pkg(url, filename);

    if (!stat) {
        log_printf(LOG_ERROR, "An error has occurred and we could not download your package.\n");
        return false;
    }

    if (!useGit)
        stat = backend->install_pkg(pkg.value(), filename.substr(0, filename.rfind(".tar.gz")), useGit);
    else
        stat = backend->install_pkg(pkg.value(), filename, useGit);

    if (!stat) {
        log_printf(LOG_ERROR, "Building/Installing your package has failed.\n");
        return false;
    }

    return true;
}

bool removePkg(string pkgName, TaurBackend* backend) {
    return backend->remove_pkg(pkgName, config.aurOnly);
}

bool updateAll(TaurBackend* backend) {
    string cacheDir = config.cacheDir;

    return backend->update_all_pkgs(path(cacheDir), config.useGit);
}

bool execPacman(int argc, char* argv[]) {
    char* args[argc + 1];
    args[0] = _("pacman"); // Set the command name to pacman
    for (int i = 1; i < argc; ++i) {
        args[i] = argv[i];
    }
    args[argc] = nullptr; // null-terminate the array
    for (int i = 0; i < argc; ++i)
        cout << "args: " << args[i] << endl;
    execvp(args[0], args);

    // If execvp returns, it means an error occurred
    perror("execvp");
    return false;
}

int parsearg_op(int opt) {
    switch (opt) {
        case 'S':
            operation.op = OP_SYNC;
            operation.args.push_back(optarg);
            break;
        case 'R':
            operation.op = OP_REM;
            operation.args.push_back(optarg);
            break;
        case 'Q': operation.op = OP_QUERY; break;
        case 'a': config.aurOnly = true; break;
        case 'h':
            usage();
            exit(0);
            break;
        case 'V': std::cout << "TabAUR version 0.0.1" << std::endl; break;
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

bool queryPkgs(TaurBackend* backend) {
    vector<string> pkgs = backend->list_all_local_pkgs(config.aurOnly, false);

    for (size_t i = 0; i < pkgs.size(); i++)
        cout << pkgs[i] << endl;

    return true;
}

// main
int main(int argc, char* argv[]) {

    TaurBackend backend(config);

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
