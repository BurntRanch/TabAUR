#ifndef GIT_HPP
#define GIT_HPP

#include <git2.h>
#include <iostream>
#include <fcntl.h>

#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

class TaurBackend {
  public:
    TaurBackend();
    ~TaurBackend();
    git_repository* taur_clone_git(std::string url, std::string out_path, int* status);
    std::string     taur_search_aur(std::string query, int* status);
    bool            taur_download_tar(std::string url, std::string out_path);
    bool            taur_install_pkg(std::string path);

  private:
    git_clone_options git_opt;
    int               git2_inits;
};

#endif
