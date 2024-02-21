#ifndef GIT_HPP
#define GIT_HPP

#include <git2.h>

class TaurBackend {
public:
	TaurBackend();
	~TaurBackend();
	git_repository *taur_clone_git(char *url, char *out_path, int *status);
private:
	git_clone_options git_opt;
	int git2_inits;
};

#endif
