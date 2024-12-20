// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include "taur.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>

#include "config.hpp"
#include "util.hpp"

TaurBackend::TaurBackend(Config& cfg) : config(cfg) {}

bool TaurBackend::download_git(const std::string_view url, const path& out_path)
{
    if (std::filesystem::exists(path(out_path) / ".git"))
    {
        return taur_exec({ config.git.c_str(), "-C", out_path, "pull", "--autostash", "--rebase", "--force" });
    }
    else
    {
        if (std::filesystem::exists(path(out_path)))
            std::filesystem::remove_all(out_path);
        return taur_exec({ config.git.c_str(), "clone", url.data(), out_path });
    }
}

bool TaurBackend::download_tar(const std::string_view url, const path& out_path)
{
    const std::string& out_path_str = out_path.string();
    std::ofstream out(out_path_str.substr(0, out_path_str.length() - "tar.gz"_len));
    if (!out.is_open())
        return false;

    cpr::Session session;
    session.SetUrl(cpr::Url(url));
    const cpr::Response& r = session.Download(out);

    if (r.status_code != 200)
        return false;

    out.close();

    // if this is in a directory, it will change to that directory first.
    if (out_path.has_parent_path())
        std::filesystem::current_path(out_path.parent_path());

    return taur_exec({ "tar", "-xf", out_path.c_str() });
}

/** Downloads a package from the AUR repository.
 * @param url a link to the download page, it will detect the extension and call the cooresponding download_x function.
 * @param out_path the path to extract the folder, for git folders, this will be where the repo is cloned.
 * @returns bool, true = success, false = failure.
 */
bool TaurBackend::download_pkg(const std::string_view url, const path out_path)
{
    if (hasEnding(url, ".git"))
        return this->download_git(url, out_path);
    else if (hasEnding(url, ".tar.gz"))
        return this->download_tar(url, out_path);

    return false;
}

static std::string getUrl(const rapidjson::Value& pkgJson, const bool returnGit = false)
{
    if (returnGit)
        return fmt::format("https://aur.archlinux.org/{}.git", pkgJson["Name"].GetString());

    // URLPath starts with a / 
    return fmt::format("https://aur.archlinux.org{}", pkgJson["URLPath"].GetString());
}

static TaurPkg_t parsePkg(const rapidjson::Value& pkgJson, const bool returnGit = false)
{
    std::vector<std::string> makedepends, depends, totaldepends, licenses;

    // yes, it will get depends and makedepends even one doesn't have it
    if (pkgJson.HasMember("Depends") && pkgJson["Depends"].IsArray())
    {
        const rapidjson::Value& dependsArray = pkgJson["Depends"].GetArray();

        depends.reserve(dependsArray.Size());

        for (size_t i = 0; i < dependsArray.Size(); i++)
            depends.push_back(dependsArray[i].GetString());

        totaldepends.insert(totaldepends.end(), depends.begin(), depends.end());
        // for (size_t i = 0; i < makeDependsArray.Size(); i++) {
        //     // make sure it isn't in the depends list
        //     if (std::find(totaldepends.begin(), totaldepends.end(), makeDependsArray[i].GetString()) !=
        //     totaldepends.end())
        //         continue;
        //     totaldepends.push_back(makeDependsArray[i].GetString());
        // }
    }

    if (pkgJson.HasMember("MakeDepends") && pkgJson["MakeDepends"].IsArray())
    {
        const rapidjson::Value& makeDependsArray = pkgJson["MakeDepends"].GetArray();

        makedepends.reserve(makeDependsArray.Size());

        for (size_t i = 0; i < makeDependsArray.Size(); ++i)
            makedepends.push_back(makeDependsArray[i].GetString());

        totaldepends.insert(totaldepends.end(), makedepends.begin(), makedepends.end());
    }

    if (pkgJson.HasMember("License") && pkgJson["License"].IsArray())
    {
        const rapidjson::Value& licensesArray = pkgJson["License"].GetArray();

        licenses.reserve(licensesArray.Size());

        for (size_t i = 0; i < licensesArray.Size(); ++i)
            licenses.push_back(licensesArray[i].GetString());
    }

    TaurPkg_t out = {
        .name          = pkgJson["Name"].GetString(),
        .version       = pkgJson["Version"].GetString(),
        .aur_url       = getUrl(pkgJson, returnGit),
        .desc          = pkgJson["Description"].IsString() ? pkgJson["Description"].GetString() : "",
        .maintainer    = pkgJson["Maintainer"].IsString()
                             ? pkgJson["Maintainer"].GetString()
                             : "\1",  // it's impossible that the maintainer name is a binary char
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

std::optional<TaurPkg_t> TaurBackend::fetch_pkg(const std::string_view pkg, const bool returnGit)
{
    const std::string& urlStr = "https://aur.archlinux.org/rpc/v5/info/" + cpr::util::urlEncode(pkg.data());

    const cpr::Response& resp = cpr::Get(cpr::Url(urlStr));

    if (resp.status_code != 200)
        return {};

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    if (json["resultcount"].GetInt() > 0)
        return parsePkg(json["results"][0], returnGit);

    return {};
}

std::vector<TaurPkg_t> TaurBackend::fetch_pkgs(std::vector<std::string> const& pkgs, const bool returnGit)
{
    if (pkgs.empty())
        return {};

    std::string urlStr = "https://aur.archlinux.org/rpc/v5/info?arg%5B%5D=" + pkgs[0];
    urlStr.reserve(pkgs.size());

    for (size_t i = 1; i < pkgs.size(); i++)
        urlStr += ("&arg%5B%5D=" + pkgs[i]);

    log_println(DEBUG, "info url = {}", urlStr);

    const cpr::Response& resp = cpr::Get(cpr::Url(urlStr));

    if (resp.status_code != 200)
        return {};

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    std::vector<TaurPkg_t> out;
    out.reserve(json["resultcount"].GetInt());
    for (int i = 0; i < json["resultcount"].GetInt(); i++)
        out.push_back(parsePkg(json["results"][i], returnGit));

    return out;
}

/** Removes a single packages using libalpm.
  * @param pkgs an alpm package to remove.
  * @param ownTransaction a bool that dictates whether this function owns the transaction, this is used for remove_pkgs,
  * if not, it will not initialize it and not release it.
  * @return success.
*/
bool TaurBackend::remove_pkg(alpm_pkg_t* pkg, const bool ownTransaction)
{
    if (!pkg)
        return false;

    if (ownTransaction && alpm_trans_init(this->config.handle, this->config.flags))
    {
        log_println(ERROR, _("Failed to initialize transaction ({})"), alpm_strerror(alpm_errno(this->config.handle)));
        return false;
    }

    if (alpm_remove_pkg(this->config.handle, pkg) != 0)
    {
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
  * @param pkgs alpm list of alpm_pkg_t pointers to remove.
  * @return success.
 */
bool TaurBackend::remove_pkgs(const alpm_list_smart_pointer& pkgs)
{
    if (!pkgs)
        return false;

    if (alpm_trans_init(this->config.handle, this->config.flags))
    {
        log_println(ERROR, _("Failed to initialize transaction ({})"), alpm_strerror(alpm_errno(this->config.handle)));
        return false;
    }

    size_t pkgs_length = alpm_list_count(pkgs.get());

    if (pkgs_length == 0)
    {
        log_println(ERROR, _("Couldn't find any packages!"));
        alpm_trans_release(this->config.handle);
        return false;
    }

    else if (pkgs_length == 1)
    {
        return this->remove_pkg((alpm_pkg_t*)(pkgs->data), false) && commitTransactionAndRelease();
    }

    for (alpm_list_t* pkgsGet = pkgs.get(); pkgsGet; pkgsGet = pkgsGet->next)
    {
        bool success = this->remove_pkg((alpm_pkg_t*)(pkgsGet->data), false);
        if (!success)
        {
            alpm_trans_release(this->config.handle);
            return false;
        }
    }

    if (!commitTransactionAndRelease())
    {
        log_println(ERROR, _("Failed to prepare, commit, or release alpm transaction."));
        return false;
    }

    return true;
}

bool TaurBackend::build_pkg(const std::string_view pkg_name, const std::string_view extracted_path, const bool alreadyprepared)
{
    std::filesystem::current_path(extracted_path);

    if (!alreadyprepared)
    {
        log_println(INFO, _("Verifying package sources.."));
        makepkg_exec({ "--verifysource", "--skippgpcheck", "-fs", "-Cc" });

        log_println(INFO, _("Preparing for compilation.."));
        makepkg_exec({ "--nobuild", "--skippgpcheck", "-fs", "-C", "--ignorearch" });
    }

    built_pkg = makepkg_list(pkg_name.data(), extracted_path);
    log_println(DEBUG, "built_pkg = {}", built_pkg);

    if (!std::filesystem::exists(built_pkg))
    {
        /*log_println(INFO, _("Compiling {} in 3 seconds, you can cancel at this point if you can't compile."),
        pkg_name); sleep(3);*/

        return makepkg_exec(
            { "-fs", "--noconfirm", "--noextract", "--noprepare", "--nocheck", "--holdver", "--ignorearch", "-c" },
            false);
    }
    else
        log_println(INFO, _("{} exists already, skipping..."), built_pkg);

    return true;
}

// I don't know but I feel this is shitty, atleast it works great
bool TaurBackend::handle_aur_depends(const TaurPkg_t& pkg, const path& out_path, std::vector<TaurPkg_t> const& localPkgs, const bool useGit)
{
    log_println(DEBUG, "pkg.name = {}", pkg.name);
    log_println(DEBUG, "pkg.totaldepends = {}", pkg.totaldepends);
    const std::vector<std::string>& aur_list = load_aur_list();

    for (size_t i = 0; i < pkg.totaldepends.size(); i++)
    {
        std::string_view pkg_depend;

        if (std::binary_search(aur_list.begin(), aur_list.end(), pkg.totaldepends.at(i)))
            pkg_depend = pkg.totaldepends.at(i);

        if (pkg_depend.empty())
            continue;

        const std::optional<TaurPkg_t>& oDepend = this->fetch_pkg(pkg_depend, useGit);
        if (!oDepend)
            continue;

        const TaurPkg_t& depend = oDepend.value();

        log_println(DEBUG, "depend = {} -- depend.totaldepends = {}", depend.name, depend.totaldepends);

        bool alreadyExists = false;
        for (size_t j = 0; (j < localPkgs.size() && !alreadyExists); j++)
        {
            if (depend.name == localPkgs[j].name)
            {
                alreadyExists = true;
                break;
            }
        }

        if (alreadyExists)
        {
            log_println(DEBUG, "dependency {} already exists, skipping!", depend.name);
            continue;
        }

        for (size_t i = 0; i < depend.totaldepends.size(); i++)
        {
            std::string_view sub_pkg_depend;

            if (std::binary_search(aur_list.begin(), aur_list.end(), depend.totaldepends.at(i)))
                sub_pkg_depend = depend.totaldepends.at(i);

            if (sub_pkg_depend.empty())
                continue;

            const std::optional<TaurPkg_t>& oSubDepend = this->fetch_pkg(sub_pkg_depend, useGit);
            if (!oDepend)
                continue;

            const TaurPkg_t& subDepend = oSubDepend.value();

            alreadyExists = false;
            for (size_t j = 0; (j < localPkgs.size() && !alreadyExists); j++)
            {
                if (subDepend.name == localPkgs[j].name)
                {
                    alreadyExists = true;
                    break;
                }
            }

            if (alreadyExists)
            {
                log_println(DEBUG, "dependency of {} ({}) already exists, skipping!", depend.name, subDepend.name);
                continue;
            }

            std::string filename = out_path / subDepend.aur_url.substr(subDepend.aur_url.rfind('/') + 1);

            if (useGit)
                filename = filename.substr(0, filename.rfind(".git"));

            log_println(DEBUG, "Downloading dependency {} of dependency {}.", subDepend.name, depend.name);
            if (!this->download_pkg(subDepend.aur_url, filename))
            {
                log_println(ERROR, _("Failed to download dependency {} of dependency {}."), subDepend.name,
                            depend.name);
                continue;
            }

            log_println(DEBUG, "Handling dependencies for dependency {} of dependency {}.", subDepend.name,
                        depend.name);
            if (!depend.totaldepends.empty() && !handle_aur_depends(subDepend, out_path, localPkgs, useGit))
            {
                log_println(ERROR, _("Failed to handle dependencies for dependency {} of dependency {}."),
                            subDepend.name, depend.name);
                continue;
            }

            if (!useGit)
                filename = filename.substr(0, filename.rfind(".tar.gz"));

            log_println(DEBUG, "Installing dependency {} of dependency {}.", subDepend.name, depend.name);
            if (!this->build_pkg(subDepend.name, filename, false))
            {
                log_println(ERROR, _("Failed to compile dependency {} of dependency {}."), subDepend.name, depend.name);
                continue;
            }

            log_println(DEBUG, "Installing dependency {} of dependency {}.", subDepend.name, depend.name);
            if (!pacman_exec("-U", split(built_pkg, ' '), false))
            {
                log_println(ERROR, _("Failed to install dependency {} of dependency {}."), subDepend.name, depend.name);
                continue;
            }
        }

        log_println(INFO, _("Downloading dependency {}"), depend.name);

        std::string filename = out_path / depend.aur_url.substr(depend.aur_url.rfind('/') + 1);

        if (useGit)
            filename = filename.substr(0, filename.rfind(".git"));

        bool downloadStatus = this->download_pkg(depend.aur_url, filename);

        if (!downloadStatus)
        {
            log_println(ERROR, _("Failed to download dependency of {} (Source: {})"), pkg.name, depend.aur_url);
            return false;
        }

        bool installStatus = this->build_pkg(depend.name, (out_path / depend.name).string(), false);

        if (!installStatus)
        {
            log_println(ERROR, _("Failed to install dependency of {} ({})"), pkg.name, depend.name);
            return false;
        }

        log_println(DEBUG, "Installing dependency of {} ({}).", pkg.name, depend.name);
        if (!pacman_exec("-U", split(built_pkg, ' '), false))
        {
            log_println(ERROR, _("Failed to install dependency of {} ({})."), pkg.name, depend.name);
            return false;
        }
    }

    return true;
}

bool TaurBackend::update_all_aur_pkgs(const path& cacheDir, const bool useGit)
{
    const std::vector<TaurPkg_t>& localPkgs = this->get_all_local_pkgs(true);

    if (localPkgs.empty())
    {
        log_println(INFO, _("No AUR packages found in your system."));
        return true;
    }

    std::string line;
    std::vector<std::string> pkgNames;
    pkgNames.reserve(localPkgs.size());

    for (const TaurPkg_t& pkg : localPkgs)
        pkgNames.push_back(pkg.name);

    const std::vector<TaurPkg_t>& onlinePkgs = this->fetch_pkgs(pkgNames, useGit);

    int updatedPkgs        = 0;
    int attemptedDownloads = 0;

    if (onlinePkgs.size() != localPkgs.size())
        log_println(WARN,
                    _("Couldn't get all packages! (searched {} packages, got {}) Still trying to update the others."),
                    localPkgs.size(), onlinePkgs.size());

    log_println(INFO, "Here's a list of packages that may be upgraded:");

    std::vector<std::tuple<TaurPkg_t, TaurPkg_t>> potentialUpgradeTargets;

    for (size_t i = 0; i < onlinePkgs.size(); i++)
    {
        const TaurPkg_t&  pkg                    = onlinePkgs[i];
        const auto&       pkgIteratorInLocalPkgs = std::find_if(localPkgs.begin(), localPkgs.end(), 
                                                          [&pkg](const TaurPkg_t& element) { return element.name == pkg.name; });

        const size_t      pkgIndexInLocalPkgs    = std::distance(localPkgs.begin(), pkgIteratorInLocalPkgs);
        const TaurPkg_t&  localPkg               = localPkgs[pkgIndexInLocalPkgs];

        if (hasEnding(pkg.name, "-git"))
        {
            potentialUpgradeTargets.push_back(std::make_tuple(pkg, localPkg));
            log_println(INFO, "- {} (from {} to {}, (dev package, may change despite AUR version))", localPkg.name,
                        localPkg.version, pkg.version);
            continue;
        }

        if ((localPkg.version != pkg.version) && alpm_pkg_vercmp(pkg.version.c_str(), localPkg.version.c_str()) == 1)
        {
            potentialUpgradeTargets.push_back(std::make_tuple(pkg, localPkg));
            log_println(INFO, "- {} (from {} to {})", localPkg.name, localPkg.version, pkg.version);
            continue;
        }
    }

    log_println(INFO, _("{} packages to upgrade."), potentialUpgradeTargets.size());

    if (!askUserYorN(true, PROMPT_YN_PROCEED_UPGRADE))
        return false;

    for (const auto& potentialUpgrade : potentialUpgradeTargets)
    {
        // size_t pkgIndex;
        // bool   found = false;
        bool alrprepared = false;

        // for (pkgIndex = 0; pkgIndex < localPkgs.size(); pkgIndex++) {
        //     if (localPkgs[pkgIndex].name == onlinePkgs[i].name) {
        //         found = true;
        //         break;
        //     }
        // }

        const TaurPkg_t& potentialUpgradeTargetTo   = std::get<0>(potentialUpgrade);
        const TaurPkg_t& potentialUpgradeTargetFrom = std::get<1>(potentialUpgrade);

        log_println(DEBUG, "potentialUpgradeTarget.name = {}", potentialUpgradeTargetTo.name);
        log_println(DEBUG, "potentialUpgradeTarget.totaldepends = {}", potentialUpgradeTargetTo.totaldepends);

        // if (!found) {
        //     log_println(WARN, _("We couldn't find {} in the local pkg database, This shouldn't happen."),
        //     onlinePkgs[i].name); continue;
        // }

        const bool isGitPackage = hasEnding(potentialUpgradeTargetTo.name, "-git");

        // if (!isGitPackage && localPkgs[pkgIndex].version == onlinePkgs[i].version) {
        //     log_println(DEBUG, "pkg {} has no update, local: {}, online: {}, skipping!", localPkgs[pkgIndex].name,
        //     localPkgs[pkgIndex].version, onlinePkgs[i].version); continue;
        // }

        const path& pkgDir = cacheDir / potentialUpgradeTargetTo.name;

        log_println(INFO, _("Downloading {}."), potentialUpgradeTargetTo.name);

        if (!useGit)
            std::filesystem::remove_all(pkgDir);

        bool downloadSuccess = this->download_pkg(potentialUpgradeTargetTo.aur_url, pkgDir);

        if (!downloadSuccess)
        {
            log_println(WARN, _("Failed to download package {}!"), potentialUpgradeTargetTo.name);
            continue;
        }

        // workaround for -git package because they are "special"
        if (isGitPackage)
        {
            alrprepared = true;
            std::filesystem::current_path(pkgDir);
            makepkg_exec({ "--verifysource", "-fA" });
            makepkg_exec({ "--nobuild", "-dfA" });
        }

        u_short iter_index = 0;
        std::ifstream pkgbuild(pkgDir.string() + "/PKGBUILD", std::ios::in);
        std::string versionInfo, pkgrel, epoch;
        while (std::getline(pkgbuild, line) && iter_index < 3)
        {
            if (hasStart(line, "pkgver="))
                getFileValue(iter_index, line, versionInfo, "pkgver="_len);

            else if (hasStart(line, "pkgrel="))
                getFileValue(iter_index, line, pkgrel, "pkgrel"_len);

            else if (hasStart(line, "epoch="))
                getFileValue(iter_index, line, epoch, "epoch="_len);
        }

        if (versionInfo.empty())
        {
            log_println(
                WARN,
                _("Failed to parse version information from {}'s PKGBUILD, You might be able to ignore this safely."),
                potentialUpgradeTargetTo.name);
            continue;
        }

        if (!pkgrel.empty() && pkgrel[0] != '\0')
            versionInfo += '-' + pkgrel;

        if (!epoch.empty() && epoch[0] != '\0')
            versionInfo = epoch + ':' + versionInfo;

        log_println(DEBUG, "pkg {} versions: local {} vs online {}", potentialUpgradeTargetTo.name,
                    potentialUpgradeTargetTo.version, potentialUpgradeTargetFrom.version);

        if ((alpm_pkg_vercmp(potentialUpgradeTargetFrom.version.data(), versionInfo.c_str())) == 0)
        {
            log_println(DEBUG,
                        _("pkg {} has the same version on the AUR than in its PKGBUILD, local: {}, online: {}, "
                          "PKGBUILD: {}, skipping!"),
                        potentialUpgradeTargetFrom.name, potentialUpgradeTargetFrom.version,
                        potentialUpgradeTargetTo.version, versionInfo);
            continue;
        }

        log_println(INFO, _("Upgrading package {} from version {} to version {}!"), potentialUpgradeTargetFrom.name,
                    potentialUpgradeTargetTo.version, potentialUpgradeTargetTo.version);
        attemptedDownloads++;

        this->handle_aur_depends(potentialUpgradeTargetTo, pkgDir, this->get_all_local_pkgs(true), useGit);
        bool installSuccess = this->build_pkg(potentialUpgradeTargetTo.name, pkgDir.string(), alrprepared);

        if (installSuccess)
        {
            pkgs_to_install += built_pkg + ' ';
            log_println(DEBUG, "pkgs_to_install = {}", pkgs_to_install);
            updatedPkgs++;

            // prevent duplicated entries
            // localPkgs.erase(localPkgs.begin() + pkgIndex);
        }
        else
        {
            pkgs_failed_to_build += potentialUpgradeTargetTo.name + ' ';
            log_println(DEBUG, "pkgs_failed_to_build = {}", pkgs_failed_to_build);
        }
    }

    if (pkgs_to_install.size() <= 0)
    {
        log_println(WARN, _("No packages to be upgraded."));
        return false;
    }

    if (!pacman_exec("-U", split(pkgs_to_install, ' '), false))
    {
        log_println(ERROR, _("Failed to install/upgrade packages"));
        return false;
    }

    log_println(INFO, _("Upgraded {}/{} packages."), updatedPkgs, attemptedDownloads);

    if (attemptedDownloads > updatedPkgs)
    {
        pkgs_failed_to_build.erase(pkgs_failed_to_build.end() - 1);
        log_println(WARN, fg(color.red), _("Failed to upgrade: {}"), pkgs_failed_to_build);
        // log_println(WARN, _("Some packages failed to download/upgrade, Please redo this command and log the
        // issue.\nIf it is an issue with TabAUR, feel free to open an issue in GitHub."));
        log_println(INFO, fg(color.cyan), _("Tip: try to run taur with \"-S {}\" (e.g \"taur -S {}\") and cleanbuild"),
                    pkgs_failed_to_build, pkgs_failed_to_build);
    }

    return true;
}

// all AUR local packages
// returns a barebones TaurPkg_t structure, with no description/depends list
std::vector<TaurPkg_t> TaurBackend::get_all_local_pkgs(const bool aurOnly)
{
    std::vector<alpm_pkg_t*> pkgs;

    alpm_list_t *pkg, *syncdbs = config.repos;

    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config.handle)); pkg; pkg = pkg->next)
        pkgs.push_back((alpm_pkg_t*)(pkg->data));

    if (aurOnly)
        pkgs = filterAURPkgs(pkgs, syncdbs, true);

    std::vector<TaurPkg_t> out;
    out.reserve(pkgs.size());

    for (alpm_pkg_t* pkg : pkgs)
    {
        out.push_back({ .name    = alpm_pkg_get_name(pkg),
                        .version = alpm_pkg_get_version(pkg),
                        .aur_url = "https://aur.archlinux.org/" + cpr::util::urlEncode(alpm_pkg_get_name(pkg)) + ".git",
                        .installed = true });
    }

    return out;
}

// They are different because we found that fetching each AUR pkg is very time consuming, so we store the name and look
// it up later.
std::vector<TaurPkg_t> TaurBackend::getPkgFromJson(const rapidjson::Document& doc, const bool useGit)
{
    int resultcount = doc["resultcount"].GetInt();

    std::vector<TaurPkg_t> out(resultcount);

    for (int i = 0; i < resultcount; i++)
        out[i] = parsePkg(doc["results"][i], useGit);

    return out;
}

std::vector<TaurPkg_t> TaurBackend::search_pac(const std::string_view query)
{
    // we search for the package name and print only the name, not the description
    alpm_list_t* syncdbs = config.repos;
    alpm_db_t*   localdb = alpm_get_localdb(config.handle);

    alpm_list_smart_pointer packages(nullptr, alpm_list_free);
    alpm_list_smart_pointer query_regex(alpm_list_add(nullptr, (void*)query.data()), alpm_list_free);

    for (; syncdbs; syncdbs = alpm_list_next(syncdbs))
    {
        alpm_list_t* ret = nullptr;
        if (alpm_db_search((alpm_db_t*)(syncdbs->data), query_regex.get(), &ret) == 0)
        {
            for (alpm_list_t* retClone = ret; retClone; retClone = retClone->next)
            {
                alpm_list_t* packages_get = packages.get();
                (void)packages.release();
                packages = make_list_smart_pointer(alpm_list_add(packages_get, (alpm_pkg_t*)(retClone->data)));
            }
        }
    }

    std::vector<TaurPkg_t> out;

    for (alpm_list_t* packages_get = packages.get(); packages_get; packages_get = packages_get->next)
    {
        alpm_pkg_t* pkg = (alpm_pkg_t*)(packages_get->data);

        out.push_back({ .name          = alpm_pkg_get_name(pkg),
                        .version       = alpm_pkg_get_version(pkg),
                        .desc          = alpm_pkg_get_desc(pkg),
                        .last_modified = alpm_pkg_get_builddate(pkg),
                        .votes         = -1,
                        .installed     = alpm_db_get_pkg(localdb, alpm_pkg_get_name(pkg)) != nullptr,
                        .db_name       = alpm_db_get_name(alpm_pkg_get_db(pkg)) });
    }

    return out;
}

// Returns an optional that is empty if an error occurs
// status will be set to -1 in the case of an error as well.
std::vector<TaurPkg_t> TaurBackend::search(const std::string_view query, const bool useGit, const bool aurOnly, const bool checkExactMatch)
{
    if (query.empty())
        return {};

    // link to AUR API. Took search pattern from yay
    const cpr::Url& url = fmt::format("https://aur.archlinux.org/rpc?arg%5B%5D={}&by={}&type=search&v=5", cpr::util::urlEncode(query.data()), config.getConfigValue<std::string>("searchBy", "name-desc"));
    log_println(DEBUG, "url search = {}", url.str());

    const cpr::Response r = cpr::Get(url);
    const std::string_view raw_text_response = r.text;

    rapidjson::Document json_response;
    json_response.Parse(raw_text_response.data());

    // clang-format off
    const std::vector<TaurPkg_t>& aurPkgs = (json_response["resultcount"].GetInt64() > 0) ? 
                                            this->getPkgFromJson(json_response, useGit) : std::vector<TaurPkg_t>();

    const std::vector<TaurPkg_t>& pacPkgs = (!aurOnly) ? this->search_pac(query)        : std::vector<TaurPkg_t>();

    if (std::string_view(json_response["type"].GetString(), json_response["type"].GetStringLength()) == "error")
        log_println(ERROR, "AUR Search error: {}", json_response["error"].GetString());

    const size_t& allPkgsSize = aurPkgs.size() + pacPkgs.size();

    std::vector<TaurPkg_t> combined;

    combined.reserve(allPkgsSize);

    if (!aurPkgs.empty())
        combined.insert(combined.end(), aurPkgs.begin(), aurPkgs.end());
    if (!pacPkgs.empty())
        combined.insert(combined.end(), pacPkgs.begin(), pacPkgs.end());

    if (!checkExactMatch)  // caller doesn't want us to check for an exact match.
        return combined;

    for (size_t i = 0; i < combined.size(); i++)
        if (combined[i].name == query)
            return { combined[i] };  // return the exact match only.

    return combined;
}
