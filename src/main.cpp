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

Config config;

// Verifies the argument count and validity.
bool verify_arguments(int c, char** args) {
    if (c < 2)
        return false;
    return true;
}

void print_help() {
    cout << "TabAUR Launch Options:" << endl;
    cout << "\ttaur <AUR Search Query>" << endl;
}

void print_error(const git_error* error) {
    cout << "Got an error: " << error->message << endl;
}

// main
int main(int argc, char* argv[]) {
    if (!verify_arguments(argc, argv)) {
        cout << "Invalid arguments!" << endl << endl;
        print_help();
        return -1;
    }

    TaurBackend backend(config, config.getConfigDir());
    
    int         	status   = 0;
    string      	cacheDir = config.getCacheDir();
    bool            useGit   = config.getConfigValue<bool>("general.useGit", false);
    optional<TaurPkg_t>  pkg      = backend.search_aur(string(argv[1]), &status, useGit);

    if (status != 0) {
        cout << "An error has occurred and we could not search for your package." << endl;
        return -1;
    }

    string url;
    if (pkg)
	url = pkg.value().url;

    string filename = path(cacheDir) / url.substr(url.rfind("/") + 1);

    if (useGit)
        filename = filename.substr(0, filename.rfind(".git"));

    /*if (hasEnding(url, ".git")) {
		status = 0;
		// get the end of the URL, in the function we also remove the ".git" at the end.
		backend.taur_clone_git(url, filename.substr(0, filename.find(".")), &status);

		if (status != 0) {
			print_error(git_error_last());
			return -1;
		}
	} else {*/
    bool stat = backend.download_pkg(url, filename);

    if (!stat) {
        cout << "An error has occurred and we could not download your package." << endl;
        return -1;
    }
    //}

    if (useGit)
        stat = backend.install_pkg(pkg.value(), filename.substr(0, filename.rfind(".git")));
    else
        stat = backend.install_pkg(pkg.value(), filename.substr(0, filename.rfind(".tar.gz")));

    if (!stat) {
        cout << "Building/Installing your package has failed." << endl;
        return -1;
    }

    return 0;
}
