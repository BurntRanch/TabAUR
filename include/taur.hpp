#ifndef GIT_HPP
#define GIT_HPP

#include <alpm.h>
#include <alpm_list.h>
#include <fcntl.h>
#include <unistd.h>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <cpr/cpr.h>
#include <sys/wait.h>

class Config;

using std::string;
using std::filesystem::path;
using std::vector;
using std::optional;

struct TaurPkg_t {
    string         name;
    string         version;
    string         url;
    string         desc;
    float          popularity = 1; // normal
    vector<string> depends;
    bool           installed = false;
    string         db_name = "aur";
};

class TaurBackend {
  public:
    Config& config;
    TaurBackend(Config& cfg);
    // They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and look it up later.
    vector<TaurPkg_t>   getPkgFromJson(rapidjson::Document& doc, bool useGit);
    vector<TaurPkg_t>   search_pac(string query);
    vector<TaurPkg_t>   search(string query, bool useGit);
    bool                download_tar(string url, string out_path);
    bool                download_git(string url, string out_path);
    bool                download_pkg(string url, string out_path);
    optional<TaurPkg_t> fetch_pkg(string pkg, bool returnGit);
    vector<TaurPkg_t>   fetch_pkgs(vector<string> pkgs, bool returnGit);
    bool                remove_pkg(alpm_pkg_t *pkgs, bool ownTransaction = true);
    bool                remove_pkgs(alpm_list_t *pkgs);
    bool                handle_aur_depends(TaurPkg_t pkg, path out_path, vector<TaurPkg_t> localPkgs, bool useGit);
    bool                install_pkg(string pkg_name, string extracted_path, bool onlydownload);
    bool                update_all_aur_pkgs(string cacheDir, bool useGit);
    vector<TaurPkg_t>   get_all_local_pkgs(bool aurOnly);
};

inline string built_pkg, pkgs_to_install;

#endif
