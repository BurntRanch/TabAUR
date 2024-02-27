#ifndef GIT_HPP
#define GIT_HPP

#include <git2.h>
#include <iostream>
#include <fcntl.h>

#include <tar.h>
#include <libtar.h>
#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

// libtar hates .tar.gz so we have to spoonfeed it these functions
// how dumb
#include <zlib.h>
#include <errno.h>
int gzopen_frontend ( const char *pathname, int flags, mode_t mode );
int gzclose_frontend( int fd );
ssize_t gzread_frontend ( int fd, void *buf, size_t count );
ssize_t gzwrite_frontend( int fd, const void *buf, size_t count );

class TaurBackend {
public:
	TaurBackend();
	~TaurBackend();
	git_repository *taur_clone_git(std::string url, std::string out_path, int *status);
	std::string taur_search_aur(std::string query, int *status);
	bool taur_download_tar(std::string url, std::string out_path);
	bool taur_install_pkg(std::string path);
private:
	git_clone_options git_opt;
	int git2_inits;
};

#endif
