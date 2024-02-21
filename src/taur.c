// Functions for TabAUR, These include printing stuff and others.
// main.c simply pieces each function together to make the program work.
#include <git2.h>

// git2 options & variables
git_clone_options git_opt;
int git2_inits = 0;


// Basically a wrapper for git_libgit2_init, This keeps count of how many times its been called.
bool taur_init() {
	int status = git_clone_options_init(&git_opt, GIT_CLONE_OPTIONS_VERSION);
	if (status != 0)
		return false;

	git2_inits++;
	git_libgit2_init();

	return true;
}

// Wrapper for git_libgit2_shutdown, This will call the function
// multiple times until all resources are freed.
void taur_shutdown() {
	while (git2_inits-- > 0)
		git_libgit2_shutdown();
}

git_repository *taur_clone_git(char *url, char *out_path) {
	git_repository *out = NULL;
	git_clone(&out, url, out_path, git_opt);
	
	// If an error were to happen, it would just return NULL.
	return out;
}
