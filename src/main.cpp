#include <iostream>
#include <ostream>
#include <stdbool.h>
#include <git.hpp>
#include <config.hpp>
#include <strutil.hpp>

using std::cout;
using std::cin;
using std::endl;
using std::string;

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

    TaurBackend backend;

    auto r = config.getConfigValue<string>("makepkgBin", "/usr/bin/makepkg");
    cout << r << "\n";

    int    status   = 0;
    string url      = backend.taur_search_aur(string(argv[1]), &status);
    string cacheDir = config.getCacheDir();

    if (status != 0) {
        cout << "An error has occurred: " << url << endl;
        return -1;
    }

    string filename = cacheDir + "/" + url.substr(url.rfind("/") + 1);

    /*if (hasEnding(url, ".git")) {
		status = 0;
		// get the end of the URL, in the function we also remove the ".git" at the end.
		backend.taur_clone_git(url, filename.substr(0, filename.find(".")), &status);

		if (status != 0) {
			print_error(git_error_last());
			return -1;
		}
	} else {*/
    bool stat = backend.taur_download_tar(url, filename);

    if (!stat) {
        cout << "An error has occurred and we could not download your package." << endl;
        return -1;
    }
    //}

    stat = backend.taur_install_pkg(filename.substr(0, filename.rfind(".tar.gz")));

    if (!stat) {
        cout << "Building/Installing your package has failed." << endl;
        return -1;
    }

    return 0;
}
