// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include "util.hpp"
#include "taur.hpp"
#include "config.hpp"
#include <unistd.h>

namespace fs = std::filesystem;

TaurBackend::TaurBackend(Config& cfg) : config(cfg) {}

bool TaurBackend::download_git(string url, string out_path) {
    if (fs::exists(path(out_path) / ".git")) {
        log_printf(LOG_DEBUG, "running {} -C {} pull --rebase --autostash --ff-only\n", config.git, out_path);
        return taur_exec({config.git.c_str(), "-C", out_path.c_str(), "pull", "--rebase", "--autostash", "--ff-only"});
    } else {
        if (fs::exists(path(out_path)))
                fs::remove_all(out_path);
        log_printf(LOG_DEBUG, "running {} clone {} {}\n", config.git, url, out_path);
        return taur_exec({config.git.c_str(), "clone", url.c_str(), out_path.c_str()});
    }
}

bool TaurBackend::download_tar(string url, string out_path) {
    std::ofstream out(out_path);
    if (!out.is_open())
        return false;

    cpr::Session session;
    session.SetUrl(cpr::Url(url));
    cpr::Response response = session.Download(out);

    out.close();

    // if this is in a directory, it will change to that directory first.
    bool isNested = out_path.find("/") != (size_t)-1;
    sanitizeStr(out_path);

    if (isNested)
        fs::current_path(out_path.substr(0, out_path.rfind("/")));

    log_printf(LOG_DEBUG, "running tar -xf {}\n", out_path);
    return taur_exec({"tar", "-xf", out_path.c_str()});
}

bool TaurBackend::download_pkg(string url, string out_path) {
    if (hasEnding(url, ".git"))
        return this->download_git(url, out_path);
    else if (hasEnding(url, ".tar.gz"))
        return this->download_tar(url, out_path);
    return false;
}

string getUrl(rapidjson::Value& pkgJson, bool returnGit = false) {
    if (returnGit)
        return ("https://aur.archlinux.org/" + string(pkgJson["Name"].GetString()) + ".git");
    else
        return ("https://aur.archlinux.org" + string(pkgJson["URLPath"].GetString()));
}

TaurPkg_t parsePkg(rapidjson::Value& pkgJson, bool returnGit = false) {
    vector<string>          depends;
    if (pkgJson.HasMember("Depends") && pkgJson["Depends"].IsArray() && pkgJson.HasMember("MakeDepends") && pkgJson["MakeDepends"].IsArray()) {
        const rapidjson::Value& dependsArray     = pkgJson["Depends"].GetArray();
        const rapidjson::Value& makeDependsArray = pkgJson["MakeDepends"].GetArray();

        for (size_t i = 0; i < dependsArray.Size(); i++)
            depends.push_back(dependsArray[i].GetString());
        for (size_t i = 0; i < makeDependsArray.Size(); i++) {
            if (std::find(depends.begin(), depends.end(), makeDependsArray[i].GetString()) != depends.end())
                continue;
            depends.push_back(makeDependsArray[i].GetString());
        }
    }

    TaurPkg_t out =   {.name    = pkgJson["Name"].GetString(),
                       .version = pkgJson["Version"].GetString(),
                       .url     = getUrl(pkgJson, returnGit),
                       .desc    = pkgJson["Description"].IsString() ? pkgJson["Description"].GetString() : "",
                       .popularity = pkgJson["Popularity"].GetFloat(),
                       .depends = depends};

    return out;
}

optional<TaurPkg_t> TaurBackend::fetch_pkg(string pkg, bool returnGit) {
    std::string   urlStr = "https://aur.archlinux.org/rpc/v5/info/" + cpr::util::urlEncode(pkg);

    cpr::Url      url  = cpr::Url(urlStr);
    cpr::Response resp = cpr::Get(url);

    if (resp.status_code != 200)
        return {};

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    if (json["resultcount"].GetInt() > 0)
        return parsePkg(json["results"][0], returnGit);

    return {};
}

vector<TaurPkg_t> TaurBackend::fetch_pkgs(vector<string> pkgs, bool returnGit) {
    if (pkgs.empty())
        return vector<TaurPkg_t>();

    std::string varName = cpr::util::urlEncode("arg[]");
    std::string urlStr  = "https://aur.archlinux.org/rpc/v5/info?" + varName + "=" + pkgs[0];

    for (size_t i = 1; i < pkgs.size(); i++)
        urlStr += ("&" + varName + "=" + pkgs[i]);

    cpr::Url      url  = cpr::Url(urlStr);
    cpr::Response resp = cpr::Get(url);

    if (resp.status_code != 200)
        return vector<TaurPkg_t>();

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    vector<TaurPkg_t> out;
    for (int i = 0; i < json["resultcount"].GetInt(); i++)
        out.push_back(parsePkg(json["results"][i], returnGit));

    return out;
}

bool TaurBackend::remove_pkg(string pkgName, bool searchForeignPackagesOnly) {
    alpm_list_t *pkg, *syncdbs;
    syncdbs = alpm_get_syncdbs(config.handle);

    vector<alpm_pkg_t *> packages;

    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config.handle)); pkg; pkg = alpm_list_next(pkg)) {
        string name = string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data)));
        if (name.find(pkgName) == string::npos)
            continue;
        if (searchForeignPackagesOnly && !is_package_from_syncdb((alpm_pkg_t *)(pkg->data), syncdbs))
            packages.push_back((alpm_pkg_t *)(pkg->data));
        else if (!searchForeignPackagesOnly)
            packages.push_back((alpm_pkg_t *)(pkg->data));
    }
  
    if (packages.empty())
        return false;

    int transCode = alpm_trans_init(this->config.handle, 0);

    if (transCode) {
        log_printf(LOG_ERROR, "Failed to initialize transaction ({})\n", alpm_strerror(alpm_errno(this->config.handle)));
        return false;
    }

    if (packages.size() == 1) {
        log_printf(LOG_INFO, "Removing package {}.\n", pkgName);
        if (alpm_remove_pkg(this->config.handle, packages[0])) {
            log_printf(LOG_ERROR, "Failed to remove package ({})\n", alpm_strerror(alpm_errno(this->config.handle)));
            alpm_trans_release(this->config.handle);
            return false;
        }

        return commitTransactionAndRelease();
    }

    fmt::println("Choose packages to remove, (Seperate by spaces, type * to remove all):");
    for (size_t i = 0; i < packages.size(); i++)
        fmt::println("[{}] {}", i, alpm_pkg_get_name(packages[i]));

    string included;
    std::cin >> included;

    if (included == "*") {
        for (size_t i = 0; i < packages.size(); i++) {
            log_printf(LOG_INFO, "Removing package {}.\n", alpm_pkg_get_name(packages[i]));
            int ret = alpm_remove_pkg(this->config.handle, packages[i]);
            if (ret != 0) {
                log_printf(LOG_ERROR, "Failed to remove package, Exiting!\n");
                alpm_trans_release(this->config.handle);
                return false;
            }
        }

        if (!commitTransactionAndRelease()) {
            log_printf(LOG_ERROR, "Failed to prepare, commit, or release alpm transaction.\n");
            return false;
        }

        return true;
    }

    vector<string> includedIndexes  = split(included, ' ');
    string         finalPackageList = "";

    for (size_t i = 0; i < includedIndexes.size(); i++) {
        try {
            size_t includedIndex = (size_t)stoi(includedIndexes[i]);

            if (includedIndex >= packages.size())
                continue;

            log_printf(LOG_INFO, "Removing package {}.\n", alpm_pkg_get_name(packages[includedIndex]));
            int ret = alpm_remove_pkg(this->config.handle, packages[includedIndex]);
            if (ret != 0) {
                log_printf(LOG_ERROR, "Failed to remove package, Exiting!\n");
                alpm_trans_release(this->config.handle);
                return false;
            }
        } catch (std::invalid_argument const&) {
            log_printf(LOG_WARN, "Invalid argument! Assuming all.\n");
        }
    }

    if (!commitTransactionAndRelease()) {
        log_printf(LOG_ERROR, "Failed to prepare, commit, or release alpm transaction.\n");
        return false;
    }

    return true;
}

// faster than makepkg --packagelist
string makepkg_list(string pkg_name, string path) {
    string ret;

    string versionInfo = shell_exec("grep 'pkgver=' " + path + "/PKGBUILD | cut -d= -f2");
    string pkgrel = shell_exec("grep 'pkgrel=' " + path + "/PKGBUILD | cut -d= -f2");
    
    if (!pkgrel.empty())
        versionInfo += "-" + pkgrel;

    string arch = shell_exec("awk -F '[()]' '/^arch=/ {gsub(/\"/,\"\",$2); print $2}' " + path + "/PKGBUILD | sed -e \"s/'//g\" -e 's/\"//g'");
    string pkgext = shell_exec("grep 'PKGEXT=' /etc/makepkg.conf | cut -d= -f2 | sed -e \"s/'//g\" -e 's/\"//g'");
    
    ret = path + "/" + pkg_name + '-' + versionInfo + '-' + arch + pkgext;
    return ret;
}

bool TaurBackend::install_pkg(string pkg_name, string extracted_path) {
    // never forget to sanitize
    sanitizeStr(extracted_path);
    fs::current_path(extracted_path);

    const char* makepkg_bin = this->config.makepkgBin.c_str();
    
    log_printf(LOG_DEBUG, "running {} --verifysource -f -Cc\n", makepkg_bin);
    log_printf(LOG_INFO, "Verifying package sources..\n");
    taur_exec({makepkg_bin, "--verifysource", "-f", "-Cc"});

    log_printf(LOG_DEBUG, "running {} --nobuild -fd -C --ignorearch\n", makepkg_bin);
    log_printf(LOG_INFO, "Preparing for compilation..\n");
    taur_exec({makepkg_bin, "--nobuild", "-fd", "-C", "--ignorearch"});

    string built_pkg = makepkg_list(pkg_name, extracted_path);
    log_printf(LOG_DEBUG, "built_pkg = {}\n", built_pkg);
    
    if (!fs::exists(built_pkg)) {
        log_printf(LOG_INFO, "Compiling {} in 3 seconds, you can cancel at this point if you can't compile.\n", pkg_name);
        sleep(3);

        log_printf(LOG_DEBUG, "running {} -fs --noconfirm --noextract --noprepare --holdver --ignorearch -c\n", makepkg_bin);
        taur_exec({makepkg_bin, "-fs", "--noconfirm", "--noextract", "--noprepare", "--holdver", "--ignorearch", "-c"});   
    }
    else
        log_printf(LOG_INFO, "{} exists already, no need to build\n", built_pkg);

    log_printf(LOG_DEBUG, "running {} pacman -U {}\n", config.sudo, built_pkg);
    return taur_exec({config.sudo.c_str(), "pacman", "-U", built_pkg.c_str()});
}

bool TaurBackend::handle_aur_depends(TaurPkg_t pkg, string extracted_path, bool useGit){
    vector<TaurPkg_t> localPkgs = this->get_all_local_pkgs(true);
    log_printf(LOG_DEBUG, "pkg.name = {}\n", pkg.name);
    log_printf(LOG_DEBUG, "pkg.depends = {}\n", fmt::join(pkg.depends, "; "));
    
    for (size_t i = 0; i < pkg.depends.size(); i++) {
        
        optional<TaurPkg_t> oDepend = this->fetch_pkg(pkg.depends[i], useGit);
        if (!oDepend)
            continue;

        TaurPkg_t depend = oDepend.value();

        log_printf(LOG_DEBUG, "pkg = {} -- pkg.depends = {}\n", depend.name[i], depend.depends[i]);

        bool alreadyExists = false;
        for (size_t j = 0; (j < localPkgs.size() && !alreadyExists); j++)
            if (depend.name == localPkgs[j].name)
                alreadyExists = true;

        if (alreadyExists)
            continue;

        log_printf(LOG_INFO, "Downloading dependency {}\n", depend.name);

        string filename = path(this->config.cacheDir) / depend.url.substr(depend.url.rfind("/") + 1);

        if (useGit)
            filename = filename.substr(0, filename.rfind(".git"));

        bool downloadStatus = this->download_pkg(depend.url, filename);

        if (!downloadStatus) {
            log_printf(LOG_ERROR, "Failed to download dependency of {} (Source: {})\n", pkg.name, depend.url);
            return false;
        }

        string out_path = path(extracted_path.substr(0, extracted_path.rfind("/"))) / depend.name;

        bool   installStatus = this->install_pkg(pkg.name, out_path);

        if (!installStatus) {
            log_printf(LOG_ERROR, "Failed to install dependency of {} ({})\n", pkg.name, depend.name);
            return false;
        }
    }

    return true;
}

bool TaurBackend::update_all_pkgs(path cacheDir, bool useGit) {
    // let's not run pacman -Syu if I we just want to update AUR packages 
    /*if (!config.aurOnly) {
        taur_exec({});
    }*/
    vector<TaurPkg_t> pkgs = this->get_all_local_pkgs(true);

    if (pkgs.empty()) {
        log_printf(LOG_INFO, "No AUR packages found in your system.\n");
        return true;
    }

    vector<string> pkgNames;
    for (size_t i = 0; i < pkgs.size(); i++)
        pkgNames.push_back(pkgs[i].name);

    vector<TaurPkg_t> onlinePkgs = this->fetch_pkgs(pkgNames, useGit);

    int updatedPkgs = 0;
    int attemptedDownloads = 0;

    if (onlinePkgs.size() != pkgs.size())
        log_printf(LOG_WARN, "Couldn't get all packages! (searched {} packages, got {}) Still trying to update the others.\n", pkgs.size(), onlinePkgs.size());

    for (size_t i = 0; i < onlinePkgs.size(); i++) {
        size_t pkgIndex;
        bool   found = false;

        for (pkgIndex = 0; pkgIndex < pkgs.size(); pkgIndex++) {
            if (pkgs[pkgIndex].name == onlinePkgs[i].name) {
                found = true;
                break;
            }
        }

        log_printf(LOG_DEBUG, "onlinePkgs.name = {}\n", onlinePkgs[i].name);
        log_printf(LOG_DEBUG, "onlinePkgs.depends = {}\n", fmt::join(onlinePkgs[i].depends, "; "));

        if (!found) {
            log_printf(LOG_WARN, "We couldn't find {} in the local pkg database, This shouldn't happen.\n", onlinePkgs[i].name);
            continue;
        }

        if (pkgs[pkgIndex].version == onlinePkgs[i].version) {
            log_printf(LOG_DEBUG, "pkg {} has no update, local: {}, online: {}, skipping!\n", pkgs[pkgIndex].name, pkgs[pkgIndex].version, onlinePkgs[i].version);
            continue;
        }

        log_printf(LOG_INFO, "Downloading {}.\n", pkgs[pkgIndex].name);

        string pkgFolder = cacheDir / onlinePkgs[i].name;
        sanitizeStr(pkgFolder);

        bool downloadSuccess = this->download_pkg(onlinePkgs[i].url, pkgFolder);

        if (!downloadSuccess) {
            log_printf(LOG_WARN, "Failed to download package {}!\n", onlinePkgs[i].name);
            continue;
        }

        string versionInfo = shell_exec("grep 'pkgver=' " + pkgFolder + "/PKGBUILD | cut -d= -f2");
        
        if (versionInfo.empty()) {
            log_printf(LOG_WARN, "Failed to parse version information from {}'s PKGBUILD, You might be able to ignore this safely.\n", pkgs[pkgIndex].name);
            continue;
        }
        
        string pkgrel = shell_exec("grep 'pkgrel=' " + pkgFolder + "/PKGBUILD | cut -d= -f2");
        
        if (!pkgrel.empty())
            versionInfo += "-" + pkgrel;

        log_printf(LOG_DEBUG, "pkg {} versions: local {} vs online {}\n", pkgs[pkgIndex].name, pkgs[pkgIndex].version,
                   onlinePkgs[i].version);

        if (!alpm_pkg_vercmp(pkgs[pkgIndex].version.c_str(), versionInfo.c_str())) {
            log_printf(LOG_DEBUG, "pkg {} has a different version on the AUR than in its PKGBUILD, local: {}, online: {}, PKGBUILD: {}, skipping!\n", pkgs[pkgIndex].name, pkgs[pkgIndex].version, onlinePkgs[i].version, versionInfo);
            continue;
        }

        log_printf(LOG_INFO, "Upgrading package {} from version {} to version {}!\n", pkgs[pkgIndex].name, pkgs[pkgIndex].version,
                   onlinePkgs[i].version);
        attemptedDownloads++;
        
        this->handle_aur_depends(onlinePkgs[i], pkgFolder, useGit);
        bool installSuccess = this->install_pkg(pkgs[pkgIndex].name, pkgFolder);

        if (!installSuccess)
            continue;

        updatedPkgs++;
    }

    log_printf(LOG_INFO, "Upgraded {}/{} packages.\n", updatedPkgs, attemptedDownloads);

    if (attemptedDownloads > updatedPkgs)
        log_printf(LOG_WARN,
                   "Some packages failed to download, Please redo this command and log the issue.\nIf it is an issue with TabAUR, feel free to open an issue in GitHub.\n");

    return true;
}

// all AUR local packages
vector<TaurPkg_t> TaurBackend::get_all_local_pkgs(bool aurOnly) {
    vector<string> pkgs;

    alpm_list_t *pkg, *syncdbs;

    syncdbs = alpm_get_syncdbs(config.handle);

    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config.handle)); pkg; pkg = alpm_list_next(pkg)) {
        if ((aurOnly && !is_package_from_syncdb((alpm_pkg_t *)(pkg->data), syncdbs)) || !aurOnly) {
            pkgs.push_back(string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data))) + " " + string(alpm_pkg_get_version((alpm_pkg_t *)(pkg->data))));
        }
    }

    if (pkgs.empty())
        return {};

    vector<TaurPkg_t> out;
    for (size_t i = 0; i < pkgs.size(); i++) {
        vector<string> pkg = split(pkgs[i], ' ');
        if (pkg.size() < 2)
            continue;
        out.push_back((TaurPkg_t){pkg[0], pkg[1], "https://aur.archlinux.org/" + cpr::util::urlEncode(pkg[0]) + ".git"});
    }

    return out;
}

// all local packages names
vector<string> TaurBackend::list_all_local_pkgs(bool aurOnly, bool stripVersion) {
    vector<string> pkgs;

    alpm_list_t *pkg, *syncdbs;

    syncdbs = alpm_get_syncdbs(config.handle);

    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config.handle)); pkg; pkg = alpm_list_next(pkg)) {
        if ((aurOnly && !is_package_from_syncdb((alpm_pkg_t *)(pkg->data), syncdbs)) || !aurOnly) {
            if (stripVersion) {
                pkgs.push_back(string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data))));
            } else {
                pkgs.push_back(string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data))) + " " + string(alpm_pkg_get_version((alpm_pkg_t *)(pkg->data))));
            }
        }
    }

    return pkgs;
}

// They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and look it up later.
vector<TaurPkg_t> TaurBackend::getPkgFromJson(rapidjson::Document& doc, bool useGit) {
    int resultcount = doc["resultcount"].GetInt();

    vector<TaurPkg_t> out(resultcount);
    for (int i = 0; i < resultcount; i++) {
        out[i] = parsePkg(doc["results"][i], useGit);
    }

    return out;
}

vector<TaurPkg_t> TaurBackend::search_pac(string query) {
    // we search for the package name and print only the name, not the description
    alpm_list_t *pkg, *syncdbs;
    string packages_str;

    syncdbs = alpm_get_syncdbs(config.handle);

    vector<alpm_pkg_t *> packages;

    for (; syncdbs; syncdbs = alpm_list_next(syncdbs)) {
        for (pkg = alpm_db_get_pkgcache((alpm_db_t *)(syncdbs->data)); pkg; pkg = alpm_list_next(pkg)) {
            string name = string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data)));
            if (name.find(query) == string::npos)
                continue;
            packages.push_back((alpm_pkg_t *)(pkg->data));
        }
    }

    vector<TaurPkg_t> out;

    for (size_t i = 0; i < packages.size(); i++) {
        TaurPkg_t taur_pkg = { 
                                .name = string(alpm_pkg_get_name(packages[i])),
                                .version = string(alpm_pkg_get_version(packages[i])),
                                .desc = string(alpm_pkg_get_desc(packages[i])),
                                .popularity = 100,  // system packages are very trustable
                                .db_name = string(alpm_db_get_name(alpm_pkg_get_db(packages[i]))),
                             };

        out.push_back(taur_pkg);
    }

    return out;
}


// Returns an optional that is empty if an error occurs
// status will be set to -1 in the case of an error as well.
vector<TaurPkg_t> TaurBackend::search(string query, bool useGit) {
    if (query.empty())
        return vector<TaurPkg_t>();
    // link to AUR API
    cpr::Url            url(("https://aur.archlinux.org/rpc/v5/search/" + cpr::util::urlEncode(query) + "?by=name"));
    cpr::Response       r = cpr::Get(url);

    string              raw_text_response = r.text;

    rapidjson::Document json_response;
    json_response.Parse(raw_text_response.c_str());

    vector<TaurPkg_t> aurPkgs;
    vector<TaurPkg_t> pacPkgs = this->search_pac(query);
    
    if (json_response["resultcount"].GetInt() > 0)
        aurPkgs = this->getPkgFromJson(json_response, useGit);

    size_t            count = aurPkgs.size() + pacPkgs.size();

    vector<TaurPkg_t> combined;

    combined.reserve(count);

    if (!aurPkgs.empty())
        combined.insert(combined.end(), aurPkgs.begin(), aurPkgs.end());
    if (!pacPkgs.empty())
        combined.insert(combined.end(), pacPkgs.begin(), pacPkgs.end());

    return combined;
}
