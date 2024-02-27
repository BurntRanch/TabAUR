#include <iostream>
#include <ostream>
#include <stdbool.h>
#include <git.hpp>
#include <config.hpp>

using namespace std;

// Verifies the argument count and validity.
bool verify_arguments(int c, char **args) {
	if (c < 2)
		return false;
	return true;
}

void print_help() {
	cout << "TabAUR Launch Options:" << endl;
	cout << "\ttaur <AUR Search Query>" << endl;
}

void print_error(const git_error *error) {
	cout << "Got an error: " << error->message << endl;
}

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (ending.length() > fullString.length())
	    return false;
    return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
}


// main
int main(int argc, char *argv[]) {
	if (!verify_arguments(argc, argv)) {
		cout << "Invalid arguments!" << endl << endl;
		print_help();
		return -1;
	}

  Config config;	
	TaurBackend backend;

	int status = 0;
	string url = backend.taur_search_aur(string(argv[1]), &status), cacheDir = config.getCacheDir();
	
	if (status != 0) {
		cout << "An error has occurred: " << url << endl;
		return -1;
	}
  
  cout << "Cache dir located at " << cacheDir << endl;

	string filename = url.substr(url.rfind("/") + 1);

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
	
	stat = backend.taur_install_pkg(filename.substr(0, filename.find('.')));

	if (!stat) {
		cout << "Building/Installing your package has failed." << endl;
		return -1;
	}

	return 0;
}
