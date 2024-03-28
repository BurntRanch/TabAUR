// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include "util.hpp"
#include "taur.hpp"
#include "config.hpp"
using std::filesystem::path;

TaurBackend::TaurBackend(Config& cfg) : config(cfg) {}

bool TaurBackend::download_git(string url, string out_path) {
    if (fs::exists(path(out_path) / ".git")) {
        return taur_exec({config.git.c_str(), "-C", out_path.c_str(), "pull", "--rebase", "--autostash", "--ff-only"});
    } else {
        if (fs::exists(path(out_path)))
                fs::remove_all(out_path);
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
    
    if (isNested) {
        fs::current_path(out_path.substr(0, out_path.rfind("/")));
        return taur_exec({"tar", "-xf", out_path.c_str()});
    }
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
    if (pkgJson.HasMember("Depends") && pkgJson["Depends"].IsArray() && pkgJson.HasMember("MakeDepends") && pkgJson["MakeDepends"].IsArray()) {
        const rapidjson::Value& dependsArray     = pkgJson["Depends"].GetArray();
        const rapidjson::Value& makeDependsArray = pkgJson["MakeDepends"].GetArray();

        vector<string>          depends;
        for (size_t i = 0; i < dependsArray.Size(); i++)
            depends.push_back(dependsArray[i].GetString());
        for (size_t i = 0; i < makeDependsArray.Size(); i++) {
            if (std::find(depends.begin(), depends.end(), makeDependsArray[i].GetString()) != depends.end())
                continue;
            depends.push_back(makeDependsArray[i].GetString());
        }

        return (TaurPkg_t){.name    = pkgJson["Name"].GetString(),
                           .version = pkgJson["Version"].GetString(),
                           .url     = getUrl(pkgJson, returnGit),
                           .desc    = pkgJson["Description"].IsString() ? pkgJson["Description"].GetString() : "",
                           .depends = depends};
    }
    return (TaurPkg_t){.name    = pkgJson["Name"].GetString(),
                       .version = pkgJson["Version"].GetString(),
                       .url     = getUrl(pkgJson, returnGit),
                       .desc    = pkgJson["Description"].IsString() ? pkgJson["Description"].GetString() : ""};
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
        log_printf(LOG_ERROR, "Failed to initialize transaction (%s)", alpm_strerror(alpm_errno(this->config.handle)));
        return false;
    }

    if (packages.size() == 1) {
        log_printf(LOG_INFO, "Removing package %s.", pkgName.c_str());
        if (alpm_remove_pkg(this->config.handle, packages[0])) {
            log_printf(LOG_ERROR, "Failed to remove package (%s)", alpm_strerror(alpm_errno(this->config.handle)));
            alpm_trans_release(this->config.handle);
            return false;
        }
        
        return commitTransactionAndRelease(this->config.handle);
    }

    std::cout << "Choose packages to remove, (Seperate by spaces, type * to remove all):" << std::endl;
    for (size_t i = 0; i < packages.size(); i++)
        std::cout << "[" << i << "] " << alpm_pkg_get_name(packages[i]) << std::endl;

    string included;
    std::cin >> included;

    if (included == "*") {
        for (size_t i = 0; i < packages.size(); i++) {
            log_printf(LOG_INFO, "Removing package %s.", alpm_pkg_get_name(packages[i]));
            int ret = alpm_remove_pkg(this->config.handle, packages[i]);
            if (ret != 0) {
                log_printf(LOG_ERROR, "Failed to remove package, Exiting!");
                alpm_trans_release(this->config.handle);
                return false;
            }
        }

        if (!commitTransactionAndRelease(this->config.handle)) {
            log_printf(LOG_ERROR, "Failed to prepare, commit, or release alpm transaction.");
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

            log_printf(LOG_INFO, "Removing package %s.", alpm_pkg_get_name(packages[includedIndex]));
            int ret = alpm_remove_pkg(this->config.handle, packages[includedIndex]);
            if (ret != 0) {
                log_printf(LOG_ERROR, "Failed to remove package, Exiting!");
                alpm_trans_release(this->config.handle);
                return false;
            }
        } catch (std::invalid_argument const&) {
            log_printf(LOG_WARN, "Invalid argument! Assuming all.");
        }
    }

    if (!commitTransactionAndRelease(this->config.handle)) {
        log_printf(LOG_ERROR, "Failed to prepare, commit, or release alpm transaction.");
        return false;
    }

    return true;
}

bool TaurBackend::install_pkg(TaurPkg_t pkg, string extracted_path, bool useGit) {
    string makepkg_bin = this->config.makepkgBin;
    // never forget to sanitize
    sanitizeStr(extracted_path);

    vector<const char*> makepkg_cmd = {makepkg_bin.c_str(), "-si", "--verifysource"};
    
    if (pkg.depends.empty()) {
        fs::current_path(extracted_path);
        return taur_exec(makepkg_cmd);
    }

    vector<TaurPkg_t> localPkgs = this->get_all_local_pkgs(true);
    for (size_t i = 0; i < pkg.depends.size(); i++) {
        optional<TaurPkg_t> oDepend = this->fetch_pkg(pkg.depends[i], useGit);

        if (!oDepend)
            continue;

        TaurPkg_t depend = oDepend.value();

        bool alreadyExists = false;
        for (size_t j = 0; (j < localPkgs.size() && !alreadyExists); j++)
            if (depend.name == localPkgs[j].name)
                alreadyExists = true;

        if (alreadyExists)
            continue;

        log_printf(LOG_INFO, "Downloading dependency %s.", depend.name.c_str());

        string filename = path(this->config.cacheDir) / depend.url.substr(depend.url.rfind("/") + 1);

        if (useGit)
            filename = filename.substr(0, filename.rfind(".git"));

        bool downloadStatus = this->download_pkg(depend.url, filename);

        if (!downloadStatus) {
            log_printf(LOG_ERROR, "Failed to download dependency of %s (Source: %s)", pkg.name.c_str(), depend.url.c_str());
            return false;
        }

        string out_path = path(extracted_path.substr(0, extracted_path.rfind("/"))) / depend.name;

        bool   installStatus = this->install_pkg(depend, out_path, useGit);

        if (!installStatus) {
            log_printf(LOG_ERROR, "Failed to install dependency of %s (%s)", pkg.name.c_str(), depend.name.c_str());
            return false;
        }
    }

    fs::current_path(extracted_path);
    return taur_exec(makepkg_cmd);
}

bool TaurBackend::update_all_pkgs(path cacheDir, bool useGit) {
    // let's not run pacman -Syu if I we just want to update AUR packages 
    if (!config.aurOnly)
        taur_exec({config.sudo.c_str(), "pacman", "-Syu"});

    vector<TaurPkg_t> pkgs = this->get_all_local_pkgs(true);

    if (pkgs.empty()) {
        log_printf(LOG_INFO, "No AUR packages found in your system.");
        return true;
    }

    vector<string> pkgNames;
    for (size_t i = 0; i < pkgs.size(); i++)
        pkgNames.push_back(pkgs[i].name);

    // we don't care about url so lets set useGit to true
    vector<TaurPkg_t> onlinePkgs = this->fetch_pkgs(pkgNames, useGit);

    int updatedPkgs = 0;
    int attemptedDownloads = 0;

    if (onlinePkgs.size() != pkgs.size())
        log_printf(LOG_WARN, "Couldn't get all packages! (searched %d packages, got %d.) Still trying to update the others.", pkgs.size(), onlinePkgs.size());

    for (size_t i = 0; i < onlinePkgs.size(); i++) {
        size_t pkgIndex;
        bool   found = false;

        for (pkgIndex = 0; pkgIndex < pkgs.size(); pkgIndex++) {
            if (pkgs[pkgIndex].name == onlinePkgs[i].name) {
                found = true;
                break;
            }
        }

        if (!found) {
            log_printf(LOG_WARN, "We couldn't find %s in the local pkg database, This shouldn't happen.", onlinePkgs[i].name.c_str());
            continue;
        }

        if (pkgs[pkgIndex].version == onlinePkgs[i].version) {
            continue;
        }

        log_printf(LOG_INFO, "Downloading %s.", pkgs[pkgIndex].name.c_str());

        string pkgFolder = cacheDir / onlinePkgs[i].name;
        sanitizeStr(pkgFolder);
        log_printf(LOG_DEBUG, "extracted_path = %s", pkgFolder.c_str());

        bool downloadSuccess = this->download_pkg(onlinePkgs[i].url, pkgFolder);

        if (!downloadSuccess) {
            log_printf(LOG_WARN, "Failed to download package %s!", onlinePkgs[i].name.c_str());
            continue;
        }

        // get the pkgver, but because cut is dumb we have to strip the newline.
        string versionInfo = shell_exec("grep 'pkgver=' " + pkgFolder + "/PKGBUILD | cut -d= -f2");
        
        if (versionInfo.empty()) {
            log_printf(LOG_WARN, "Failed to parse version information from %s's PKGBUILD, You might be able to ignore this safely.", pkgs[pkgIndex].name.c_str());
            continue;
        }
        
        string pkgrel = shell_exec("grep 'pkgrel=' " + pkgFolder + "/PKGBUILD | cut -d= -f2");
        
        if (!pkgrel.empty())
            versionInfo += "-" + pkgrel;

        log_printf(LOG_DEBUG, "pkg %s versions: local %s vs online %s", pkgs[pkgIndex].name.c_str(), pkgs[pkgIndex].version.c_str(),
                   onlinePkgs[i].version.c_str());

        if (!alpm_pkg_vercmp(pkgs[pkgIndex].version.c_str(), versionInfo.c_str()))
            continue;

        log_printf(LOG_INFO, "Upgrading package %s from version %s to version %s!", pkgs[pkgIndex].name.c_str(), pkgs[pkgIndex].version.c_str(),
                   onlinePkgs[i].version.c_str());
        attemptedDownloads++;

        bool installSuccess = this->install_pkg(onlinePkgs[i], pkgFolder, useGit);

        if (!installSuccess)
            continue;

        updatedPkgs++;
    }

    log_printf(LOG_INFO, "Upgraded %d/%d packages.", updatedPkgs, attemptedDownloads);

    if (attemptedDownloads > updatedPkgs)
        log_printf(LOG_WARN,
                   "Some packages failed to download, Please redo this command and log the issue.\nIf it is an issue with TabAUR, feel free to open an issue in GitHub.");

    return true;
}

// all AUR local packages
vector<TaurPkg_t> TaurBackend::get_all_local_pkgs(bool aurOnly) {
    vector<string> pkgs;

    alpm_list_t *pkg, *syncdbs;

    syncdbs = alpm_get_syncdbs(config.handle);

    for (pkg = alpm_db_get_pkgcache(alpm_get_localdb(config.handle)); pkg; pkg = alpm_list_next(pkg)) {
        if (aurOnly && !is_package_from_syncdb((alpm_pkg_t *)(pkg->data), syncdbs)) {
            pkgs.push_back(string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data))) + " " + string(alpm_pkg_get_version((alpm_pkg_t *)(pkg->data))));
        } else if (!aurOnly) {
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
        if (aurOnly && !is_package_from_syncdb((alpm_pkg_t *)(pkg->data), syncdbs)) {
            if (stripVersion) {
                pkgs.push_back(string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data))));
            } else {
                pkgs.push_back(string(alpm_pkg_get_name((alpm_pkg_t *)(pkg->data))) + " " + string(alpm_pkg_get_version((alpm_pkg_t *)(pkg->data))));
            }
        } else if (!aurOnly) {
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

    vector<TaurPkg_t> out;
    for (int i = 0; i < resultcount; i++) {
        out.push_back(parsePkg(doc["results"][i], useGit));
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
                                .db_name = string(alpm_db_get_name(alpm_pkg_get_db(packages[i]))),
                             };

        out.push_back(taur_pkg);
    }

    return out;
}


// Returns an optional that is empty if an error occurs
// status will be set to -1 in the case of an error as well.
optional<TaurPkg_t> TaurBackend::search(string query, bool useGit) {
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

    db_colors         db_color;
    string            dbColorStr;

    if (count == 0) {
        return {};
    } else if (count == 1) {
        return aurPkgs.size( )>= 1 ? this->fetch_pkg(aurPkgs[0].name, useGit) : pacPkgs[0];
    } else if (count > 1) {
        log_printf(LOG_INFO, "TabAUR has found multiple packages relating to your search query, Please pick one.");
        string input;
        do {
            // CTRL-D
            if (!std::cin)
                return {};

            if (!input.empty())
                log_printf(LOG_WARN, "Invalid input!");

            for (size_t i = 0; i < aurPkgs.size(); i++)
                std::cout 
                    << MAGENTA << i
                    << " " << BOLDBLUE << "aur/" << BOLD << aurPkgs[i].name
                    << " " << BOLDGREEN << aurPkgs[i].version
                    << "\n    " << NOCOLOR << BOLD << aurPkgs[i].desc
                    << NOCOLOR <<
                std::endl;

            for (size_t i = 0; i < pacPkgs.size(); i++) {
                if (pacPkgs[i].db_name == "extra")
                    dbColorStr = db_color.extra;
                else if (pacPkgs[i].db_name == "multilib")
                    dbColorStr = db_color.multilib;
                else
                    dbColorStr = db_color.core;

                std::cout
                    << MAGENTA << i + aurPkgs.size()
                    << " " << dbColorStr << pacPkgs[i].db_name << '/' << BOLD << pacPkgs[i].name
                    << " " << BOLDGREEN << pacPkgs[i].version
                    << "\n    " << NOCOLOR << BOLD << pacPkgs[i].desc
		            << NOCOLOR <<
                std::endl;
            }
            std::cout << "Choose a package to download: ";
            std::cin >> input;
        } while (!is_number(input) || (size_t)std::stoi(input) >= count);

        size_t selected = std::stoi(input);

        if (selected >= aurPkgs.size())
            return pacPkgs[selected - aurPkgs.size()];
        else
            return this->fetch_pkg(aurPkgs[selected].name, useGit);
    }

    return {};
}
