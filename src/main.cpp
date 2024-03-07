#include <stdbool.h>
#include <git.hpp>
#include <config.hpp>
#include <strutil.hpp>
#include <args.hpp>

using std::cout;
using std::cin;
using std::endl;
using std::filesystem::path;
using std::vector;
using std::string;
using std::cerr;

Config config;
struct Operation_t operation;

// Verifies the argument count and validity.
bool verify_arguments(int c, char** args) {
    if (c < 2)
        return false;
    return true;
}

/*bool parse_arguments(int c, char** args, Operation_t *operation) {
    if (c <= 1) {
        operation->op = OP_UPDATE_ALL;
        return true;
    }

    for (size_t i = 1; i < c; i++) {
        string arg(args[i]);

        std::transform(arg.begin(), arg.end(), arg.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (arg == "-s")
            operation->op = OP_INSTALL;
        else if (arg == "-syu") {
            operation->op = OP_UPDATE_ALL;
            return true;
        } else if (arg == "-r")
            operation->op = OP_REMOVE;
        // unknown
        else if (hasStart(arg, "-")) {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return false;
        } else {
            operation->args.push_back(arg);
        }
    }

    // make sure we captured atleast one argument for the following operations, OP_UPDATE_ALL doesn't care about arguments.
    if (operation->args.size() < 1 && (operation->op == OP_INSTALL || operation->op == OP_REMOVE))
        return false;

    return true;
}*/

void usage() {
    cout << "TabAUR Launch Options:" << endl;
    cout << "\ttaur <pacman-like arguments> <parameters to the operation>" << endl << endl;
    cout << "TabAUR will assume -Syu if you pass no arguments to it." << endl;
}

void print_error(const git_error* error) {
    cerr << "Got an error: " << error->message << endl;
}

bool installPkg(string pkgName, TaurBackend *backend) { 
    int         	status   = 0;
    string      	cacheDir = config.getCacheDir();
    bool            useGit   = config.getConfigValue<bool>("general.useGit", false);
    optional<TaurPkg_t>  pkg      = backend->search_aur(pkgName, &status, useGit);

    if (status != 0 || !pkg) {
        cerr << "An error has occurred and we could not search for your package." << endl;
        return -1;
    }

    string url = pkg.value().url;

    string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

    if (useGit)
        filename = filename.substr(0, filename.rfind(".git"));

    bool stat = backend->download_pkg(url, filename);

    if (!stat) {
        cerr << "An error has occurred and we could not download your package." << endl;
        return false;
    }

    if (useGit)
        stat = backend->install_pkg(pkg.value(), filename.substr(0, filename.rfind(".git")));
    else
        stat = backend->install_pkg(pkg.value(), filename.substr(0, filename.rfind(".tar.gz")));

    if (!stat) {
        cout << "Building/Installing your package has failed." << endl;
        return false;
    }

    return true;
}

bool removePkg(string pkgName, TaurBackend *backend) {
    return backend->remove_pkg(pkgName);
}

bool updateAll(TaurBackend *backend) {
    string          cacheDir = config.getCacheDir();

    return backend->update_all_pkgs(path(cacheDir));
}

int parsearg_op(int opt){
    switch(opt){
        case 'S':
            operation.op = OP_SYNC; operation.args.push_back(optarg); break;
        case 'R':
            operation.op = OP_REM; operation.args.push_back(optarg); break;
        case 'Q':
            operation.op = OP_QUERY; break;
        case 'h':
            usage(); break;
        case 'V':
            std::cout << "TabAUR version 0.0.1" << std::endl; break;
        case ':':
            std::cerr << "Option requires an argument!" << std::endl; break;
        default:
            return 1;
    }
    return 0;
}

int parseargs(int argc, char* argv[]){
    int opt;
    int option_index = 0;
	int result;
	const char *optstring = "S:R:QhV";
	static const struct option opts[] = 
    {
        {"sync",    required_argument, 0, 'S'},
        {"remove",  required_argument, 0, 'R'},
        {"query",   no_argument, 0, 'Q'},
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
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

    if ((operation.op == OP_SYNC || operation.op == OP_REM) && operation.args.size() < 1)
        return 1;

    return 0;
}

bool queryPkgs(TaurBackend *backend) {
    vector<TaurPkg_t> pkgs = backend->get_all_local_pkgs();
    for (size_t i = 0; i < pkgs.size(); i++)
        cout << pkgs[i].name << " " << pkgs[i].version << endl;

    return true;
}

// main
int main(int argc, char* argv[]) {
    //Operation_t operation = {OP_INSTALL, vector<string>()};
    /*if (!parse_arguments(argc, argv, &operation)) {
        print_help();
        return 1;
    }*/

    TaurBackend backend(config, config.getConfigDir());

    if (parseargs(argc, argv))
        return 1;

    switch (operation.op) {
    case OP_SYNC:
        return (installPkg(operation.args[0], &backend)) ? 0 : 1;
    case OP_REM:
        return (removePkg(operation.args[0], &backend)) ? 0 : 1;
    case OP_QUERY:
        return (queryPkgs(&backend)) ? 0 : 1;
    //case OP_UPDATE_ALL:
    //    return (updateAll(&backend)) ? 0 : 1;
    }

    return 3;
}
