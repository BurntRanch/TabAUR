#include <iostream>
#include <ostream>
#include <stdbool.h>
#include <git.hpp>
#include <config.hpp>
#include <strutil.hpp>

using std::cout;
using std::cin;
using std::endl;
using std::filesystem::path;
using std::vector;
using std::string;
using std::cerr;

enum OperationType {
    OP_INSTALL,
    OP_UPDATE_ALL,
    OP_REMOVE,
};

struct Operation_t {
    OperationType op;
    vector<string> args;
};

Config config;

// Verifies the argument count and validity.
bool verify_arguments(int c, char** args) {
    if (c < 2)
        return false;
    return true;
}

bool parse_arguments(int c, char** args, Operation_t *operation) {
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
}

void print_help() {
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

// main
int main(int argc, char* argv[]) {
    Operation_t operation = {OP_INSTALL, vector<string>()};
    if (!parse_arguments(argc, argv, &operation)) {
        print_help();
        return 1;
    }

    TaurBackend backend(config, config.getConfigDir());

    switch (operation.op) {
    case OP_INSTALL:
        return (installPkg(operation.args[0], &backend)) ? 0 : 1;
    case OP_REMOVE:
        return (removePkg(operation.args[0], &backend)) ? 0 : 1;
    case OP_UPDATE_ALL:
        return (updateAll(&backend)) ? 0 : 1;
    }

    return 3;
}
