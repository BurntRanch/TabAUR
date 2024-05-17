// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include "taur.hpp"
#include "config.hpp"
#include "util.hpp"
#include <filesystem>

TaurBackend::TaurBackend(Config& cfg) : config(cfg) {}

bool TaurBackend::download_git(string_view url, path out_path) {
    if (std::filesystem::exists(path(out_path) / ".git")) {
        return taur_exec({config.git.c_str(), "-C", out_path.c_str(), "pull", "--rebase", "--autostash", "--ff-only"});
    } else {
        if (std::filesystem::exists(path(out_path)))
            std::filesystem::remove_all(out_path);
        return taur_exec({config.git.c_str(), "clone", url.data(), out_path.c_str()});
    }
}

bool TaurBackend::download_tar(string_view url, path out_path) {
    std::ofstream out(out_path.replace_extension("tar.gz"));
    if (!out.is_open())
        return false;

    cpr::Session session;
    session.SetUrl(cpr::Url(url));
    cpr::Response response = session.Download(out);

    out.close();

    // if this is in a directory, it will change to that directory first.
    if (out_path.has_parent_path())
        std::filesystem::current_path(out_path.parent_path());

    return taur_exec({"tar", "-xf", out_path.c_str()});
}

/** Downloads a package from the AUR repository.
 * @param url a link to the download page, it will detect the extension and call the cooresponding download_x function.
 * @param out_path the path to extract the folder, for git folders, this will be where the repo is cloned.
 * @returns bool, true = success, false = failure.
*/
bool TaurBackend::download_pkg(string_view url, path out_path) {
    if (hasEnding(url, ".git"))
        return this->download_git(url, out_path);
    else if (hasEnding(url, ".tar.gz"))
        return this->download_tar(url, out_path);
    return false;
}

string getUrl(rapidjson::Value& pkgJson, bool returnGit = false) {
    std::stringstream ss;
    if (returnGit)
        ss << "https://aur.archlinux.org/" << pkgJson["Name"].GetString() << ".git";
    else
        ss << "https://aur.archlinux.org" << pkgJson["URLPath"].GetString();

    return ss.str();
}

TaurPkg_t parsePkg(rapidjson::Value& pkgJson, bool returnGit = false) {
    vector<string> makedepends, depends, totaldepends, licenses;

    // yes, it will get depends and makedepends even one doesn't have it
    if (pkgJson.HasMember("Depends") && pkgJson["Depends"].IsArray() 
        && pkgJson.HasMember("MakeDepends") && pkgJson["MakeDepends"].IsArray()
        && pkgJson.HasMember("License") && pkgJson["License"].IsArray()) 
    {

        const rapidjson::Value& dependsArray     = pkgJson["Depends"].GetArray();
        const rapidjson::Value& makeDependsArray = pkgJson["MakeDepends"].GetArray();
        const rapidjson::Value& licensesArray    = pkgJson["License"].GetArray();

        depends.reserve(dependsArray.Size());
        makedepends.reserve(makeDependsArray.Size());
        totaldepends.reserve(dependsArray.Size());
        licenses.reserve(licensesArray.Size());

        for (size_t i = 0; i < licensesArray.Size(); ++i)
            licenses.push_back(licensesArray[i].GetString());

        for (size_t i = 0; i < dependsArray.Size(); i++)
            depends.push_back(dependsArray[i].GetString());

        for (size_t i = 0; i < makeDependsArray.Size(); i++)
            makedepends.push_back(makeDependsArray[i].GetString());

        totaldepends = depends;
        for (size_t i = 0; i < makeDependsArray.Size(); i++) {
            // make sure it isn't in the depends list
            if (std::find(totaldepends.begin(), totaldepends.end(), makeDependsArray[i].GetString()) != totaldepends.end())
                continue;
            totaldepends.push_back(makeDependsArray[i].GetString());
        }
    }

    TaurPkg_t out = {
        .name          = pkgJson["Name"].GetString(),
        .version       = pkgJson["Version"].GetString(),
        .aur_url       = getUrl(pkgJson, returnGit),
        .desc          = pkgJson["Description"].IsString() ? pkgJson["Description"].GetString() : "",
        .maintainer    = pkgJson["Maintainer"].IsString() ? pkgJson["Maintainer"].GetString() : "\1", // it's impossible that the maintainer name is a binary char
        .last_modified = pkgJson["LastModified"].GetInt64(),
        .outofdate     = pkgJson["OutOfDate"].IsInt64() ? pkgJson["OutOfDate"].GetInt64() : 0,
        .popularity    = pkgJson["Popularity"].GetFloat(),
        .votes         = pkgJson["NumVotes"].GetFloat(),
        .licenses      = licenses,
        .makedepends   = makedepends,
        .depends       = depends,
        .totaldepends  = totaldepends,
        .installed     = alpm_db_get_pkg(alpm_get_localdb(config->handle), pkgJson["Name"].GetString()) != nullptr,
    };

    return out;
}

optional<TaurPkg_t> TaurBackend::fetch_pkg(string_view pkg, bool returnGit) {
    string        urlStr = "https://aur.archlinux.org/rpc/v5/info/" + cpr::util::urlEncode(pkg.data());

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

vector<TaurPkg_t> TaurBackend::fetch_pkgs(vector<string> const& pkgs, bool returnGit) {
    if (pkgs.empty())
        return vector<TaurPkg_t>();

    string urlStr = "https://aur.archlinux.org/rpc/v5/info?arg%5B%5D=" + pkgs[0];

    for (auto const& pkg : pkgs)
        urlStr += ("&arg%5B%5D=" + pkg);

    log_println(DEBUG, "info url = {}", urlStr);

    cpr::Url      url  = cpr::Url(urlStr);
    cpr::Response resp = cpr::Get(url);

    if (resp.status_code != 200)
        return vector<TaurPkg_t>();

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    vector<TaurPkg_t> out;
    out.reserve(json["resultcount"].GetInt());
    for (int i = 0; i < json["resultcount"].GetInt(); i++)
        out.push_back(parsePkg(json["results"][i], returnGit));

    return out;
}

/** Removes a single packages using libalpm.
  @param pkgs an alpm package to remove.
  @param ownTransaction a bool that dictates whether this function owns the transaction, this is used for remove_pkgs, if not, it will not initialize it and not release it.
  @return success.
*/
bool TaurBackend::remove_pkg(alpm_pkg_t *pkg, bool ownTransaction) {
    if (!pkg)
        return false;

    if (ownTransaction && alpm_trans_init(this->config.handle, this->config.flags)) {
        log_println(ERROR, _("Failed to initialize transaction ({})"), alpm_strerror(alpm_errno(this->config.handle)));
        return false;
    }

    if (alpm_remove_pkg(this->config.handle, pkg) != 0) {
        log_println(ERROR, _("Failed to remove package ({})"), alpm_strerror(alpm_errno(this->config.handle)));
        if (ownTransaction)
            alpm_trans_release(this->config.handle);
        return false;
    }

    if (ownTransaction)
        return commitTransactionAndRelease();

    return true;
}

/** Removes a list of packages using libalpm.
  * Will automatically call remove_pkg instead, if the size of pkgs is equal to 1.
  @param pkgs alpm list of alpm_pkg_t pointers to remove.
  @return success.
*/
bool TaurBackend::remove_pkgs(alpm_list_smart_pointer& pkgs) {
    if (!pkgs)
        return false;

    if (alpm_trans_init(this->config.handle, this->config.flags)) {
        log_println(ERROR, _("Failed to initialize transaction ({})"), alpm_strerror(alpm_errno(this->config.handle)));
        return false;
    }

    size_t pkgs_length = alpm_list_count(pkgs.get());

    if (pkgs_length == 0) {
        log_println(ERROR, _("Couldn't find any packages!"));
        alpm_trans_release(this->config.handle);
        return false;
    } else if (pkgs_length == 1) {
        return this->remove_pkg((alpm_pkg_t *)(pkgs->data), false) && commitTransactionAndRelease();
    }

    for (alpm_list_t *pkgsGet = pkgs.get(); pkgsGet; pkgsGet = pkgsGet->next) {
        bool success = this->remove_pkg((alpm_pkg_t *)(pkgsGet->data), false);
        if (!success) {
            alpm_trans_release(this->config.handle);
            return false;
        }
    }

    if (!commitTransactionAndRelease()) {
        log_println(ERROR, _("Failed to prepare, commit, or release alpm transaction."));
        return false;
    }

    return true;
}

bool TaurBackend::build_pkg(string_view pkg_name, string extracted_path, bool alreadyprepared) {
    // never forget to sanitize
    sanitizeStr(extracted_path);
    std::filesystem::current_path(extracted_path);

    if (!alreadyprepared) {
        log_println(INFO, _("Verifying package sources.."));
        makepkg_exec("--verifysource --skippgpcheck -f -Cc");

        log_println(INFO, _("Preparing for compilation.."));
        makepkg_exec("--nobuild --skippgpcheck -fs -C --ignorearch");
    }

    built_pkg = makepkg_list(pkg_name.data(), extracted_path);
    log_println(DEBUG, "built_pkg = {}", built_pkg);

    if (!std::filesystem::exists(built_pkg)) {
        /*log_println(INFO, _("Compiling {} in 3 seconds, you can cancel at this point if you can't compile."), pkg_name);
        sleep(3);*/

        return makepkg_exec("-f --noconfirm --noextract --noprepare --nocheck --holdver --ignorearch -c", false);
    } else
        log_println(INFO, _("{} exists already, skipping..."), built_pkg);

    return true;
}

// I don't know but I feel this is shitty, atleast it works great
bool TaurBackend::handle_aur_depends(TaurPkg_t pkg, path out_path, vector<TaurPkg_t> const& localPkgs, bool useGit) {
    log_println(DEBUG, "pkg.name = {}", pkg.name);
    log_println(DEBUG, "pkg.totaldepends = {}", pkg.totaldepends);
    vector<string_view> aur_list = load_aur_list();

    for (size_t i = 0; i < pkg.totaldepends.size(); i++) {
        string_view pkg_depend = binarySearch(aur_list, pkg.totaldepends[i]);
        
        if (pkg_depend.empty())
            continue;
        
        optional<TaurPkg_t> oDepend = this->fetch_pkg(pkg_depend, useGit);
        if (!oDepend)
            continue;

        TaurPkg_t depend = oDepend.value();

        log_println(DEBUG, "depend = {} -- depend.totaldepends = {}", depend.name, depend.totaldepends);

        bool alreadyExists = false;
        for (size_t j = 0; (j < localPkgs.size() && !alreadyExists); j++)
            if (depend.name == localPkgs[j].name)
                alreadyExists = true;

        if (alreadyExists) {
            log_println(DEBUG, "dependency {} already exists, skipping!", depend.name); 
            continue;
        }

        for (size_t i = 0; i < depend.totaldepends.size(); i++) {
            string_view sub_pkg_depend = binarySearch(aur_list, depend.totaldepends[i]);
        
            if (sub_pkg_depend.empty())
                continue;
        
            optional<TaurPkg_t> oSubDepend = this->fetch_pkg(sub_pkg_depend, useGit);
            if (!oDepend)
                continue;

            TaurPkg_t subDepend = oSubDepend.value();

            alreadyExists = false;
            for (size_t j = 0; (j < localPkgs.size() && !alreadyExists); j++)
                if (subDepend.name == localPkgs[j].name)
                    alreadyExists = true;

            if (alreadyExists) {
                log_println(DEBUG, "dependency of {} ({}) already exists, skipping!", depend.name, subDepend.name); 
                continue;
            }

            string filename = out_path / subDepend.aur_url.substr(subDepend.aur_url.rfind("/") + 1);

            if (useGit)
                filename = filename.substr(0, filename.rfind(".git"));

            log_println(DEBUG, "Downloading dependency {} of dependency {}.", subDepend.name, depend.name); 
            if (!this->download_pkg(subDepend.aur_url, filename)) {
                log_println(ERROR, _("Failed to download dependency {} of dependency {}."), subDepend.name, depend.name);
                continue;
            }

            log_println(DEBUG, "Handling dependencies for dependency {} of dependency {}.", subDepend.name, depend.name);   
            if (!depend.totaldepends.empty() && !handle_aur_depends(subDepend, out_path, localPkgs, useGit)) {
                log_println(ERROR, _("Failed to handle dependencies for dependency {} of dependency {}."), subDepend.name, depend.name);
                continue;
            }

            if (!useGit)
                filename = filename.substr(0, filename.rfind(".tar.gz"));

            log_println(DEBUG, "Installing dependency {} of dependency {}.", subDepend.name, depend.name);  
            if (!this->build_pkg(subDepend.name, filename, false)) {
                log_println(ERROR, _("Failed to compile dependency {} of dependency {}."), subDepend.name, depend.name);
                continue;
            }

            log_println(DEBUG, "Installing dependency {} of dependency {}.", subDepend.name, depend.name);  
            if (!pacman_exec("-U", split(built_pkg, ' '), false)) {
                log_println(ERROR, _("Failed to install dependency {} of dependency {}."), subDepend.name, depend.name);
                continue;
            }
        }

        log_println(INFO, _("Downloading dependency {}"), depend.name);

        string filename = out_path / depend.aur_url.substr(depend.aur_url.rfind("/") + 1);

        if (useGit)
            filename = filename.substr(0, filename.rfind(".git"));

        bool downloadStatus = this->download_pkg(depend.aur_url, filename);

        if (!downloadStatus) {
            log_println(ERROR, _("Failed to download dependency of {} (Source: {})"), pkg.name, depend.aur_url);
            return false;
        }

        bool installStatus = this->build_pkg(depend.name, out_path / depend.name, false);

        if (!installStatus) {
            log_println(ERROR, _("Failed to install dependency of {} ({})"), pkg.name, depend.name);
            return false;
        }

        log_println(DEBUG, "Installing dependency of {} ({}).", pkg.name, depend.name); 
        if (!pacman_exec("-U", split(built_pkg, ' '), false)) {
            log_println(ERROR, _("Failed to install dependency of {} ({})."), pkg.name, depend.name);
            return false;
        }
    }

    return true;
}

bool TaurBackend::update_all_aur_pkgs(path cacheDir, bool useGit) {
    vector<TaurPkg_t> pkgs = this->get_all_local_pkgs(true);

    if (pkgs.empty()) {
        log_println(INFO, _("No AUR packages found in your system."));
        return true;
    }

    vector<string> pkgNames;
    pkgNames.reserve(pkgs.size());

    for (size_t i = 0; i < pkgs.size(); ++i)
        pkgNames.push_back(pkgs[i].name);

    vector<TaurPkg_t> onlinePkgs = this->fetch_pkgs(pkgNames, useGit);

    int               updatedPkgs        = 0;
    int               attemptedDownloads = 0;
    bool              alrprepared        = false;

    if (onlinePkgs.size() != pkgs.size())
        log_println(WARN, _("Couldn't get all packages! (searched {} packages, got {}) Still trying to update the others."), pkgs.size(), onlinePkgs.size());

    for (size_t i = 0; i < onlinePkgs.size(); i++) {
        size_t pkgIndex;
        bool   found = false;

        for (pkgIndex = 0; pkgIndex < pkgs.size(); pkgIndex++) {
            if (pkgs[pkgIndex].name == onlinePkgs[i].name) {
                found = true;
                break;
            }
        }

        log_println(DEBUG, "onlinePkgs.name = {}", onlinePkgs[i].name);
        log_println(DEBUG, "onlinePkgs.totaldepends = {}", onlinePkgs[i].totaldepends);

        if (!found) {
            log_println(WARN, _("We couldn't find {} in the local pkg database, This shouldn't happen."), onlinePkgs[i].name);
            continue;
        }

        if (pkgs[pkgIndex].version == onlinePkgs[i].version) {
            log_println(DEBUG, "pkg {} has no update, local: {}, online: {}, skipping!", pkgs[pkgIndex].name, pkgs[pkgIndex].version, onlinePkgs[i].version); 
            continue;
        }

        path pkgDir = cacheDir / onlinePkgs[i].name;

        log_println(INFO, _("Downloading {}."), onlinePkgs[i].name);

        if (!useGit)
            std::filesystem::remove_all(pkgDir);

        bool downloadSuccess = this->download_pkg(onlinePkgs[i].aur_url, pkgDir);

        if (!downloadSuccess) {
            log_println(WARN, _("Failed to download package {}!"), onlinePkgs[i].name);
            continue;
        }

        // workaround for -git package because they are "special"
        if (hasEnding(pkgs[pkgIndex].name, "-git")) {
            alrprepared = true;
            std::filesystem::current_path(pkgDir);
            makepkg_exec("--verifysource -fA");
            makepkg_exec("--nobuild -dfA");
        }

        string versionInfo = shell_exec("grep 'pkgver=' " + pkgDir.string() + "/PKGBUILD | cut -d= -f2");

        if (versionInfo.empty()) {
            log_println(WARN, _("Failed to parse version information from {}'s PKGBUILD, You might be able to ignore this safely."), pkgs[pkgIndex].name);
            continue;
        }

        string pkgrel = shell_exec("grep 'pkgrel=' " + pkgDir.string() + "/PKGBUILD | cut -d= -f2");
        string epoch  = shell_exec("grep 'epoch=' "  + pkgDir.string() + "/PKGBUILD | cut -d= -f2");

        if (!pkgrel.empty() && pkgrel[0] != '\0')
            versionInfo += '-' + pkgrel;

        if (!epoch.empty() && epoch[0] != '\0')
            versionInfo = epoch + ':' + versionInfo;

        log_println(DEBUG, "pkg {} versions: local {} vs online {}", pkgs[pkgIndex].name, pkgs[pkgIndex].version, onlinePkgs[i].version);   

        if ((alpm_pkg_vercmp(pkgs[pkgIndex].version.data(), versionInfo.c_str())) == 0) {
            
            log_println(DEBUG, _("pkg {} has the same version on the AUR than in its PKGBUILD, local: {}, online: {}, PKGBUILD: {}, skipping!"), pkgs[pkgIndex].name,
                        pkgs[pkgIndex].version, onlinePkgs[i].version, versionInfo);
            continue;
        }

        log_println(INFO, _("Upgrading package {} from version {} to version {}!"), pkgs[pkgIndex].name, pkgs[pkgIndex].version, onlinePkgs[i].version);
        attemptedDownloads++;

        this->handle_aur_depends(onlinePkgs[i], pkgDir, this->get_all_local_pkgs(true), useGit);
        bool installSuccess = this->build_pkg(pkgs[pkgIndex].name, pkgDir, alrprepared);
        alrprepared         = false;

        if (installSuccess) {
            pkgs_to_install += built_pkg + ' ';
            log_println(DEBUG, "pkgs_to_install = {}", pkgs_to_install);
            updatedPkgs++;
        } else {
            pkgs_failed_to_build += pkgs[pkgIndex].name + ' ';
            log_println(DEBUG, "pkgs_failed_to_build = {}", pkgs_failed_to_build);
        }
    }

    if (pkgs_to_install.size() <= 0) {
        log_println(WARN, _("No packages to be upgraded."));
        // oh no, a goto oh noooo this program is ruined
        goto text;
    }

    // remove the last ' ' so we can pass it to pacman
    pkgs_to_install.erase(pkgs_to_install.end() - 1);
    if (!pacman_exec("-U", split(pkgs_to_install, ' '), false)) {
        log_println(ERROR, _("Failed to install/upgrade packages"));
        return false;
    }

    log_println(INFO, _("Upgraded {}/{} packages."), updatedPkgs, attemptedDownloads);

text:
    if (attemptedDownloads > updatedPkgs) {
        pkgs_failed_to_build.erase(pkgs_failed_to_build.end() - 1);
        log_println(WARN, fg(color.red), _("Failed to upgrade: {}"), pkgs_failed_to_build);
        //log_println(WARN, _("Some packages failed to download/upgrade, Please redo this command and log the issue.\nIf it is an issue with TabAUR, feel free to open an issue in GitHub."));
        log_println(INFO, fg(color.cyan), _("Tip: try to run taur with \"-S {}\" (e.g \"taur -S {}\")"), pkgs_failed_to_build, pkgs_failed_to_build);
    }

    return true;
}

// all AUR local packages
// returns a barebones TaurPkg_t structure, with no description/depends list
vector<TaurPkg_t> TaurBackend::get_all_local_pkgs(bool aurOnly) {
    vector<alpm_pkg_t *> pkgs;

    alpm_list_t         *pkg, *syncdbs;

    syncdbs = config.repos;

    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config.handle)); pkg; pkg = pkg->next)
        pkgs.push_back((alpm_pkg_t *)(pkg->data));

    if (aurOnly)
        pkgs = filterAURPkgs(pkgs, syncdbs, true);

    vector<TaurPkg_t> out;
    out.reserve(pkgs.size());

    for (alpm_pkg_t *pkg : pkgs) {
        out.push_back({.name      = alpm_pkg_get_name(pkg),
                       .version   = alpm_pkg_get_version(pkg),
                       .aur_url   = "https://aur.archlinux.org/" + cpr::util::urlEncode(alpm_pkg_get_name(pkg)) + ".git",
                       .installed = true});
    }

    return out;
}

// They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and look it up later.
vector<TaurPkg_t> TaurBackend::getPkgFromJson(rapidjson::Document& doc, bool useGit) {
    int resultcount = doc["resultcount"].GetInt();

    vector<TaurPkg_t> out(resultcount);

    for (int i = 0; i < resultcount; i++)
        out[i] = parsePkg(doc["results"][i], useGit);

    return out;
}

vector<TaurPkg_t> TaurBackend::search_pac(string_view query) {
    // we search for the package name and print only the name, not the description
    alpm_list_t *syncdbs = config.repos;
    alpm_db_t   *localdb = alpm_get_localdb(config.handle);

    alpm_list_smart_pointer packages(nullptr, alpm_list_free);
    alpm_list_smart_pointer query_regex(alpm_list_add(nullptr, (void *)query.data()), alpm_list_free);

    for (; syncdbs; syncdbs = alpm_list_next(syncdbs)) {
        alpm_list_t *ret = nullptr;
        if (alpm_db_search((alpm_db_t *)(syncdbs->data), query_regex.get(), &ret) == 0) {
            for (alpm_list_t *retClone = ret; retClone; retClone = retClone->next) {
                alpm_list_t *packages_get = packages.get();
                (void)packages.release();
                packages = make_list_smart_pointer(alpm_list_add(packages_get, (alpm_pkg_t *)(retClone->data)));
            }
        }
    }

    vector<TaurPkg_t> out;

    for (alpm_list_t *packages_get = packages.get(); packages_get; packages_get = packages_get->next) {
        alpm_pkg_t *pkg = (alpm_pkg_t *)(packages_get->data);

        out.push_back({.name          = alpm_pkg_get_name(pkg),
                       .version       = alpm_pkg_get_version(pkg),
                       .desc          = alpm_pkg_get_desc(pkg),
                       .last_modified = alpm_pkg_get_builddate(pkg),
                       .votes         = -1,
                       .installed     = alpm_db_get_pkg(localdb, alpm_pkg_get_name(pkg)) != nullptr,
                       .db_name       = alpm_db_get_name(alpm_pkg_get_db(pkg))});
    }

    return out;
}

// Returns an optional that is empty if an error occurs
// status will be set to -1 in the case of an error as well.
vector<TaurPkg_t> TaurBackend::search(string_view query, bool useGit, bool checkExactMatch) {
    if (query.empty())
        return vector<TaurPkg_t>();
    // link to AUR API. Took search pattern from yay
    cpr::Url url(("https://aur.archlinux.org/rpc?arg%5B%5D=" + cpr::util::urlEncode(query.data()) + "&by=" + config.getConfigValue("searchBy", "name-desc") + "&type=search&v=5"));
    log_println(DEBUG, _("url search = {}"), url.str());
    cpr::Response       r = cpr::Get(url);

    string_view         raw_text_response = r.text;

    rapidjson::Document json_response;
    json_response.Parse(raw_text_response.data());

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

    if (!checkExactMatch) // caller doesn't want us to check for an exact match.
        return combined;

    for (size_t i = 0; i < combined.size(); i++)
        if (combined[i].name == query)
            return {combined[i]}; // return the exact match only.

    return combined;
}
