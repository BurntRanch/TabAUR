#ifndef GIT_HPP
#define GIT_HPP

#include <git2.h>
#include <iostream>
#include <fcntl.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <config.hpp>
#include <db.hpp>
#include <cpr/cpr.h>

class TaurBackend {
  public:
    Config config;
    TaurBackend(Config cfg, std::string dbLocation);
    ~TaurBackend();
    git_repository* taur_clone_git(std::string url, std::string out_path, int* status);
    std::optional<TAUR_PKG>     taur_search_aur(std::string query, int* status);
    bool            taur_download_tar(std::string url, std::string out_path);
    bool            taur_install_pkg(TAUR_PKG pkg, std::string extracted_path);

  private:
    git_clone_options git_opt;
    int               git2_inits;
    DB                db;
};

#endif
