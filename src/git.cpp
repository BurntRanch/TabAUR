// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include <git.hpp>

TaurBackend::TaurBackend() {
	git2_inits = 0;

	int status = git_clone_options_init(&git_opt, GIT_CLONE_OPTIONS_VERSION);
	if (status != 0)
		return;

	git2_inits++;
	git_libgit2_init();
}

// Wrapper for git_libgit2_shutdown, This will call the function
// multiple times until all resources are freed.
TaurBackend::~TaurBackend() {
	while (git2_inits-- > 0)
		git_libgit2_shutdown();
}

git_repository *TaurBackend::taur_clone_git(char *url, char *out_path, int *status) {
	git_repository *out = NULL;
	*status = git_clone(&out, url, out_path, &git_opt);
	
	// If an error were to happen, it would just return NULL.
	return out;
}
