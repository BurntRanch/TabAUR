#include <stdio.h>
#include <stdbool.h>
#include <git2.h>

// Verifies the argument count and validity.
bool verify_arguments(int c, char **args) {
	if (c < 3)
		return false;
	return true;
}

void print_help() {
	printf("TabAUR Launch Options:\n");
	printf("\ttaur <clone URL> <clone directory>\n");
}

// main
int main(int argc, char **argv) {
	if (!verify_arguments(argc, argv)) {
		printf("Invalid arguments!\n\n");
		print_help();
		return -1;
	}
		
	taur_init();

	printf("Cloning from %s to %s!\n", argv[1], argv[2]);
	taur_clone_git(argv[1], argv[2]);

	taur_shutdown();
	return 0;
}
