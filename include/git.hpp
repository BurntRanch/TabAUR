#ifndef GIT_HPP
#define GIT_HPP

#include <git2.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

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
    std::optional<TaurPkg_t>     search_aur(std::string query, int* status, bool useGit = false);
    bool            download_tar(std::string url, std::string out_path);
    bool            download_git(std::string url, std::string out_path);
    bool            download_pkg(std::string url, std::string out_path);
    bool            install_pkg(TaurPkg_t pkg, std::string extracted_path);

  private:
    git_clone_options git_opt;
    int               git2_inits;
    DB                db;
};

#endif
