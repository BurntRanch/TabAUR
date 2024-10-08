#ifndef TAUR_HPP
#define TAUR_HPP

#include <alpm.h>
#include <alpm_list.h>
#include <fcntl.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ctime>
#include <optional>

#include "cpr/cpr.h"
#include "util.hpp"

class Config;

using std::filesystem::path;

struct TaurPkg_t
{
    std::string              name;
    std::string              version;
    std::string              aur_url;
    std::string              url;
    std::string              desc;
    std::string              arch;
    std::string              maintainer;
    time_t                   last_modified;
    time_t                   outofdate;
    float                    popularity = 1;  // normal
    float                    votes      = 0;  // system packages have no votes
    std::vector<std::string> licenses;
    std::vector<std::string> makedepends;
    std::vector<std::string> depends;
    std::vector<std::string> totaldepends;
    bool                     installed = false;
    std::string              db_name   = "aur";

    alpm_list_t* licenses_list;
    alpm_list_t* depends_list;
};

class TaurBackend
{
public:
    Config& config;
    TaurBackend(Config& cfg);
    // They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and
    // look it up later.
    std::vector<TaurPkg_t>   getPkgFromJson(const rapidjson::Document& doc, const bool useGit);
    std::vector<TaurPkg_t>   search_pac(const std::string_view query);
    std::vector<TaurPkg_t>   search(const std::string_view query, const bool useGit, const bool aurOnly, const bool checkExactMatch = true);
    bool                     download_tar(const std::string_view url, const path& out_path);
    bool                     download_git(const std::string_view url, const path& out_path);
    bool                     download_pkg(const std::string_view url, const path out_path);
    std::optional<TaurPkg_t> fetch_pkg(const std::string_view pkg, const bool returnGit);
    std::vector<TaurPkg_t>   fetch_pkgs(std::vector<std::string> const& pkgs, const bool returnGit);
    bool                     remove_pkgs(const alpm_list_smart_pointer& pkgs);
    bool                     remove_pkg(alpm_pkg_t* pkgs, const bool ownTransaction = true);
    bool                     handle_aur_depends(const TaurPkg_t& pkg, const path& out_path, std::vector<TaurPkg_t> const& localPkgs, const bool useGit);
    bool                     build_pkg(const std::string_view pkg_name, const std::string_view extracted_path, const bool alreadyprepared);
    bool                     update_all_aur_pkgs(const path& cacheDir, const bool useGit);
    std::vector<TaurPkg_t>   get_all_local_pkgs(const bool aurOnly);
};

inline std::string built_pkg, pkgs_to_install, pkgs_failed_to_build;

#endif
