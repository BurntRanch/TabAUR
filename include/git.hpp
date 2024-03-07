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
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

using std::string;
using std::filesystem::path;

class TaurBackend {
  public:
    Config config;
    TaurBackend(Config cfg, string dbLocation);
    ~TaurBackend();
    std::optional<TaurPkg_t>     search_aur(string query, int* status, bool useGit = false);
    bool            download_tar(string url, string out_path);
    bool            download_git(string url, string out_path);
    bool            download_pkg(string url, string out_path);
    vector<TaurPkg_t> fetch_pkgs(vector<TaurPkg_t> pkgs);
    bool            remove_pkg(string name);
    bool            install_pkg(TaurPkg_t pkg, string extracted_path);
    bool            update_all_pkgs(path cacheDir);
    vector<TaurPkg_t> get_all_local_pkgs();
  private:
    git_clone_options git_opt;
    int               git2_inits;
    DB                db;
};

#endif
