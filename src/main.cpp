#include <iostream>
#include <stdbool.h>
#include <git.hpp>

using namespace std;

// Verifies the argument count and validity.
bool verify_arguments(int c, char **args) {
	if (c < 3)
		return false;
	return true;
}

void print_help() {
	cout << "TabAUR Launch Options:" << endl;
	cout << "\ttaur <clone URL> <clone directory>" << endl;
}

void print_error(const git_error *error) {
	cout << "Got an error: " << error->message << endl;
}

// main
int main(int argc, char *argv[]) {
	if (!verify_arguments(argc, argv)) {
		cout << "Invalid arguments!" << endl << endl;
		print_help();
		return -1;
	}
	
	TaurBackend backend;

	cout << "Cloning from " << argv[1] << " to " << argv[2] << "!" << endl;
	
	int status;
	backend.taur_clone_git(argv[1], argv[2], &status);
	
	if (status != 0)
		print_error(git_error_last());

	return status;
}
