#ifndef TAUR_HPP
#define TAUR_HPP

#include "cpr/cpr.h"
#include "util.hpp"
#include <alpm.h>
#include <alpm_list.h>
#include <ctime>
#include <fcntl.h>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sys/wait.h>
#include <unistd.h>

class Config;

using std::string;
using std::string_view;
using std::filesystem::path;
using std::vector;
using std::optional;

struct TaurPkg_t {
    string         name;
    string         version;
    string         aur_url;
    string         url;
    string         desc;
    string         arch;
    string         maintainer;
    time_t         last_modified;
    time_t         outofdate;
    float          popularity = 1; // normal
    float          votes      = 0; // system packages have no votes
    vector<string> licenses;
    vector<string> makedepends;
    vector<string> depends;
    vector<string> totaldepends;
    bool           installed = false;
    string         db_name   = "aur";

    alpm_list_t   *licenses_list;
    alpm_list_t   *depends_list;
};

class TaurBackend {
  public:
    Config& config;
    TaurBackend(Config& cfg);
    // They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and look it up later.
    vector<TaurPkg_t>   getPkgFromJson(rapidjson::Document& doc, bool useGit);
    vector<TaurPkg_t>   search_pac(string_view query);
    vector<TaurPkg_t>   search(string_view query, bool useGit, bool aurOnly, bool checkExactMatch = true);
    bool                download_tar(string_view url, path out_path);
    bool                download_git(string_view url, path out_path);
    bool                download_pkg(string_view url, path out_path);
    optional<TaurPkg_t> fetch_pkg(string_view pkg, bool returnGit);
    vector<TaurPkg_t>   fetch_pkgs(vector<string> const& pkgs, bool returnGit);
    bool                remove_pkgs(alpm_list_smart_pointer& pkgs);
    bool                remove_pkg(alpm_pkg_t *pkgs, bool ownTransaction = true);
    bool                handle_aur_depends(TaurPkg_t pkg, path out_path, vector<TaurPkg_t> const& localPkgs, bool useGit);
    bool                build_pkg(string_view pkg_name, string extracted_path, bool alreadyprepared);
    bool                update_all_aur_pkgs(path cacheDir, bool useGit);
    vector<TaurPkg_t>   get_all_local_pkgs(bool aurOnly);
};

inline string built_pkg, pkgs_to_install, pkgs_failed_to_build;

#endif
