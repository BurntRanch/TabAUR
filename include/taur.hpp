#ifndef GIT_HPP
#define GIT_HPP

#include <git2.h>
#include <alpm.h>
#include <fcntl.h>
#include <unistd.h>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <cpr/cpr.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include "util.hpp"
#include "config.hpp"

using std::string;
using std::filesystem::path;
using std::vector;
using std::optional;
using std::unique_ptr;

struct TaurPkg_t {
    string         name;
    string         version;
    string         url;
    string         desc;
    vector<string> depends;
    string         db_name;
};

struct db_colors {
    const string extra = BOLDGREEN;
    const string multilib = BOLDCYAN;
    const string core = BOLDYELLOW;
};

class TaurBackend {
  public:
    Config *config;
    TaurBackend(Config *cfg);
    ~TaurBackend();
    // They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and look it up later.
    vector<TaurPkg_t>            getPkgFromJson(rapidjson::Document& doc, bool useGit);
    vector<TaurPkg_t>            search_pac(string query);
    optional<TaurPkg_t>          search(string query, bool useGit);
    bool            download_tar(string url, string out_path);
    bool            download_git(string url, string out_path);
    bool            download_pkg(string url, string out_path);
    optional<TaurPkg_t> fetch_pkg(string pkg, bool returnGit);
    vector<TaurPkg_t> fetch_pkgs(vector<string> pkgs, bool returnGit);
    bool            remove_pkg(string name, bool searchForeignPackages);
    bool            install_pkg(TaurPkg_t pkg, string extracted_path, bool useGit);
    bool            update_all_pkgs(path cacheDir, bool useGit);
    vector<TaurPkg_t> get_all_local_pkgs(bool aurOnly);
    vector<string> list_all_local_pkgs(bool aurOnly, bool stripVersion);
  private:
    git_clone_options git_opt;
    int               git2_inits;
};

#endif
