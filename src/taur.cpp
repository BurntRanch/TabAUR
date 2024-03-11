// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include "util.hpp"
#include <taur.hpp>
#include <config.hpp>
#include <optional>

TaurBackend::TaurBackend(Config cfg) : config(cfg) {
    git2_inits = 0;

    int status = git_clone_options_init(&git_opt, GIT_CLONE_OPTIONS_VERSION);
    if (status != 0)
        return;

    git2_inits++;
    git_libgit2_init();
}

// Wrapper for git_libgit2_shutdown, This will call the function
// multiple times until all resources are freed.
TaurBackend::~TaurBackend() {
    while (git2_inits-- > 0)
        git_libgit2_shutdown();
}

// Clone from a remote git repository, and return a bool back.
bool clone_git(string url, string out_path) {
    git_repository* repo = nullptr;
    git_clone_options options = GIT_CLONE_OPTIONS_INIT;
    int status          = git_clone(&repo, url.c_str(), out_path.c_str(), &options);

    return (status == 0 && repo != nullptr);
}

// Pull from a local git repository, and return a bool back.
bool pull_git(string path) {
    git_repository* repo;
    int status           = git_repository_open(&repo, path.c_str());
    if (status != 0)
        return false;

    git_remote *remote;
    status               = git_remote_lookup(&remote, repo, "origin");
    if (status != 0)
        return false;

    git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
    status               = git_remote_fetch(remote, NULL, &options, NULL);
    if (status != 0)
        return false;

    return true;
}

using std::filesystem::path;

bool TaurBackend::download_git(string url, string out_path) {
    if (std::filesystem::exists(path(out_path) / ".git"))
        return pull_git(out_path);
    else {
        if (std::filesystem::exists(path(out_path)))
            std::filesystem::remove_all(out_path);
        return clone_git(url, out_path);
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
    	return system(("cd \"" + out_path.substr(0, out_path.rfind("/")) + "\" && tar -xf \"" + out_path.substr(out_path.rfind("/") + 1) + "\"").c_str()) == 0;
    else
    	return system((string("tar -xf \"") + out_path + string("\"")).c_str()) == 0;

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
        const rapidjson::Value& dependsArray = pkgJson["Depends"].GetArray();
        const rapidjson::Value& makeDependsArray = pkgJson["MakeDepends"].GetArray();

        vector<string> depends;
        for (size_t i = 0; i < dependsArray.Size(); i++)
            depends.push_back(dependsArray[i].GetString());
        for (size_t i = 0; i < makeDependsArray.Size(); i++) {
            if (std::find(depends.begin(), depends.end(), makeDependsArray[i].GetString()) != depends.end())
                continue;
            depends.push_back(makeDependsArray[i].GetString());
        }

        return (TaurPkg_t) { 
                             .name = pkgJson["Name"].GetString(),
                             .version = pkgJson["Version"].GetString(),
                             .url = getUrl(pkgJson, returnGit), 
                             .desc = pkgJson["Description"].GetString(),
                             .depends = depends
                           };
    }
    return (TaurPkg_t) {
                         .name = pkgJson["Name"].GetString(),
                         .version = pkgJson["Version"].GetString(),
                         .url = getUrl(pkgJson, returnGit),
                         .desc = pkgJson["Description"].GetString()
                       };
}

optional<TaurPkg_t> TaurBackend::fetch_pkg(string pkg, bool returnGit) {
    std::string  urlStr = "https://aur.archlinux.org/rpc/v5/info/" + cpr::util::urlEncode(pkg);

    cpr::Url        url = cpr::Url(urlStr);
    cpr::Response  resp = cpr::Get(url);

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
    std::string  urlStr = "https://aur.archlinux.org/rpc/v5/info?" + varName + "=" + pkgs[0];

    for (size_t i = 1; i < pkgs.size(); i++)
        urlStr += ("&" + varName + "=" + pkgs[i]);

    cpr::Url        url = cpr::Url(urlStr);
    cpr::Response  resp = cpr::Get(url);

    if (resp.status_code != 200)
        return vector<TaurPkg_t>();

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    vector<TaurPkg_t> out;
    for (int i = 0; i < json["resultcount"].GetInt(); i++)
        out.push_back(parsePkg(json["results"][i], returnGit));

    return out;
}

// http://stackoverflow.com/questions/478898/ddg#478960
string exec(string cmd) {
    std::array<char, 128> buffer;
    string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

bool sanitizeAndRemove(string& input) {
    sanitizeStr(config.sudo);
    sanitizeStr(input);

    return system((config.sudo + " pacman -R " + input).c_str()) == 0;
}

bool TaurBackend::remove_pkg(string pkgName, bool searchForeignPackagesOnly) {
    /*if (!pkg) {
        std::cerr << "Failed to find your package " << pkgName << " in the TabAUR DB" << std::endl; 
        return false;
    }*/
    sanitizeStr(pkgName);

    string packages_str;
    if (searchForeignPackagesOnly)
        packages_str = exec("pacman -Qmq | grep \"" + pkgName + "\"");
    else
        packages_str = exec("pacman -Qq | grep \"" + pkgName + "\"");

    vector<string> packages = split(packages_str, '\n');

    if (packages.empty())
        return false;

    if (packages.size() == 1)
        return sanitizeAndRemove(packages[0]);


    std::cout << "Choose packages to remove, (Seperate by spaces, type * to remove all):" << std::endl;
    for (size_t i = 0; i < packages.size(); i++)
        std::cout << "[" << i << "] " << packages[i] << std::endl;

    string included;
    std::cin >> included;

    if (included == "*") {
        string finalPackageList = "";
        for (size_t i = 0; i < packages.size(); i++)
            finalPackageList += packages[i] + " ";
        return sanitizeAndRemove(finalPackageList);
    }

    vector<string> includedIndexes = split(included, ' ');
    string finalPackageList = "";

    for (size_t i = 0; i < includedIndexes.size(); i++) {
        try {
            size_t includedIndex = (size_t)stoi(includedIndexes[i]);

            if (includedIndex >= packages.size())
                continue;

            finalPackageList += packages[includedIndex] + " ";
        } catch (std::invalid_argument const&) {
            log_printf(LOG_WARN, _("Invalid argument! Assuming all.\n"));
        }
    }

    return sanitizeAndRemove(finalPackageList);
}

bool TaurBackend::install_pkg(TaurPkg_t pkg, string extracted_path, bool useGit) {
    string makepkg_bin = this->config.makepkgBin;

    // never forget to sanitize
    sanitizeStr(extracted_path);
    sanitizeStr(makepkg_bin);

    if (pkg.depends.empty())
        return system(("cd \"" + extracted_path + "\" && " + makepkg_bin + " -si").c_str()) == 0;

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

        log_printf(LOG_INFO, _("Downloading dependency %s.\n"), depend.name.c_str());

        string filename = path(this->config.cacheDir) / depend.url.substr(depend.url.rfind("/") + 1);

        if (useGit)
            filename = filename.substr(0, filename.rfind(".git"));
        
        bool downloadStatus = this->download_pkg(depend.url, filename);

        if (!downloadStatus) {
            log_printf(LOG_ERROR, _("Failed to download dependency of %s (Source: %s)\n"), pkg.name.c_str(), depend.url.c_str());
            return false;
        }

        string out_path = path(extracted_path.substr(0, extracted_path.rfind("/"))) / depend.name;
        
        bool installStatus = this->install_pkg(depend, out_path, useGit);

        if (!installStatus) {
            log_printf(LOG_ERROR, _("Failed to install dependency of %s (%s)\n"), pkg.name.c_str(), depend.name.c_str());
            return false;
        }
    }

    return system(("cd \"" + extracted_path + "\" && " + makepkg_bin + " -si").c_str()) == 0;
}

bool TaurBackend::update_all_pkgs(path cacheDir, bool useGit) {
    string sudo = config.sudo;
    sanitizeStr(sudo);
    
    // first thing first
    bool pacmanUpgradeSuccess = system((sudo + " pacman -Syu").c_str()) == 0;

    if (!pacmanUpgradeSuccess)
        return false;

    vector<TaurPkg_t> pkgs = this->get_all_local_pkgs(true);

    vector<string> pkgNames;
    for (size_t i = 0; i < pkgs.size(); i++)
        pkgNames.push_back(pkgs[i].name);

    // we don't care about url so lets set useGit to true
    vector<TaurPkg_t> onlinePkgs = this->fetch_pkgs(pkgNames, useGit);

    int updatedPkgs = 0;
    int attemptedDownloads = 0;

    if (onlinePkgs.size() != pkgs.size())
        log_printf(LOG_WARN, _("Couldn't get all packages! (looked up %d AUR packages online, looked up %d AUR packages locally) Still trying to update the others.\n"), onlinePkgs.size(), pkgs.size());

    for (size_t i = 0; i < onlinePkgs.size(); i++) {
        size_t pkgIndex;
        bool found = false;

        for (pkgIndex = 0; pkgIndex < pkgs.size(); pkgIndex++) {
            if (pkgs[pkgIndex].name == onlinePkgs[i].name) {
                found = true;
                break;
            }
        }

        if (!found) {
            log_printf(LOG_WARN, _("We couldn't find %s in the local pkg database, This shouldn't happen.\n"), onlinePkgs[i].name.c_str());
            continue;
        }

        if (pkgs[pkgIndex].version == onlinePkgs[i].version) {
            continue;
        }

        log_printf(LOG_INFO, _("Downloading %s, This could take a bit.\n"), pkgs[pkgIndex].name.c_str());

        string pkgFolder = cacheDir / onlinePkgs[i].name;
        sanitizeStr(pkgFolder);

        bool downloadSuccess = this->download_pkg(onlinePkgs[i].url, pkgFolder);

        if (!downloadSuccess) {
            log_printf(LOG_WARN, _("Failed to download package %s!\n"), onlinePkgs[i].name.c_str());
            continue;
        }

        string versionInfo = exec("grep 'pkgver=' " + pkgFolder + "/PKGBUILD | cut -d= -f2");
        
        if (versionInfo.empty()) {
            log_printf(LOG_WARN, _("Failed to parse version information from %s's PKGBUILD, You might be able to ignore this safely.\n"), pkgs[pkgIndex].name.c_str());
            continue;
        }

        if (alpm_pkg_vercmp(pkgs[pkgIndex].version.c_str(), versionInfo.c_str()))
            continue;

        log_printf(LOG_INFO, _("Upgrading package %s from version %s to version %s!\n"), pkgs[pkgIndex].name.c_str(), pkgs[pkgIndex].version.c_str(), onlinePkgs[i].version.c_str());
        attemptedDownloads++;

        bool installSuccess = this->install_pkg(onlinePkgs[i], pkgFolder, useGit);

        if (!installSuccess)
            continue;

        updatedPkgs++;
    }

    log_printf(LOG_INFO, _("Upgraded %d/%d packages.\n"), updatedPkgs, attemptedDownloads);

    if (attemptedDownloads > updatedPkgs)
        log_printf(LOG_WARN, _("Some packages failed to download, Please redo this command and log the issue.\nIf it is an issue with TabAUR, feel free to open an issue in GitHub.\n"));

    return true;
}

// all AUR local packages
vector<TaurPkg_t> TaurBackend::get_all_local_pkgs(bool aurOnly) {
    string pkgs_str;
    if (aurOnly)
        pkgs_str = exec("pacman -Qm");
    else
        pkgs_str = exec("pacman -Q");

    vector<string> pkgs = split(pkgs_str, '\n');

    if (pkgs.empty())
        return {};

    vector<TaurPkg_t> out;
    for (size_t i = 0; i < pkgs.size(); i++) {
        vector<string> pkg = split(pkgs[i], ' ');
        if (pkg.size() < 2)
            continue;
        out.push_back((TaurPkg_t) { pkg[0], pkg[1], "https://aur.archlinux.org/" + cpr::util::urlEncode(pkg[0]) + ".git" });
    }

    return out;
}

// all local packages names
vector<string> TaurBackend::list_all_local_pkgs(bool aurOnly, bool stripVersion) {
    string cmd = "pacman -Q";
    if (aurOnly)
        cmd += "m";

    if (stripVersion)
        cmd += "q";

    vector<string> pkgs = split(exec(cmd), '\n');

    return pkgs;
}

// https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c#4654718
bool is_number(const string& s) {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
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
    sanitizeStr(query);
    // we search for the package name and print only the name, not the description
    string cmd = "pacman -Ss \"" + query + "\"";
    vector<string> pkgs_string = split(exec(cmd), '\n');
    TaurPkg_t taur_pkg;
    vector<TaurPkg_t> out;

    for (size_t i = 0; i < pkgs_string.size(); i += 2) {
        try {
            vector<string> pkg = split(pkgs_string[i], '/');

            if (pkg.size() < 2) {
                log_printf(LOG_ERROR, "Pacman returned an unexpected response:\n%s\n", pkgs_string[i]); 
                continue;
            }

            taur_pkg.db_name = pkg[0];

            pkg = split(pkg[1], ' ');

            if (pkg.size() < 2) {
                log_printf(LOG_ERROR, "Pacman returned an unexpected response:\n%s\n", pkgs_string[i]); 
                continue;
            }

            taur_pkg.name = pkg[0]; 
            taur_pkg.version = pkg[1];

            if (pkgs_string.size() <= (i + 1)) {
                log_printf(LOG_ERROR, "Pacman provided no description for package %s! Command: %s\n", pkg[0], cmd); 
                out.push_back(taur_pkg);
                continue;
            }

            string desc = pkgs_string[i + 1];
            
            if (desc.size() > 4)
                taur_pkg.desc = desc.substr(4);
            else
                std::cout << pkgs_string.size() << " " << i << " " << desc.size() << std::endl;

            out.push_back(taur_pkg);
        } catch (std::out_of_range const &e) {
            log_printf(LOG_ERROR, _("Pacman did not return what we expected, Command: %s\n"), cmd.c_str());
            exit(1);
        }
    }

    return out;
}

// Returns an optional that is empty if an error occurs
// status will be set to -1 in the case of an error as well.
std::optional<TaurPkg_t> TaurBackend::search(string query, bool useGit) {
    // link to AUR API
    cpr::Url            url(("https://aur.archlinux.org/rpc/v5/search/" + cpr::util::urlEncode(query) + "?by=name"));
    cpr::Response       r = cpr::Get(url);

    string         raw_text_response = r.text;

    rapidjson::Document json_response;
    json_response.Parse(raw_text_response.c_str());

    if (strcmp(json_response["type"].GetString(), "error") == 0)
        return {};

    if (json_response["resultcount"].GetInt() == 0)
        return {};

    vector<TaurPkg_t> aurPkgs = this->getPkgFromJson(json_response, useGit);
    vector<TaurPkg_t> pacPkgs = this->search_pac(query);

    size_t count = aurPkgs.size() + pacPkgs.size();

    if (count == 1) {
	    return aurPkgs.size() == 1 ? this->fetch_pkg(aurPkgs[0].name, useGit) : pacPkgs[0];
    } else if (count > 1) {
        log_printf(LOG_INFO, _("TabAUR has found multiple packages relating to your search query, Please pick one.\n"));
        string input;
        do {
            // CTRL-D
            if (!std::cin)
                return {};

            if (!input.empty())
                log_printf(LOG_WARN, _("Invalid input!\n"));

            for (size_t i = 0; i < aurPkgs.size(); i++)
                std::cout 
                    << MAGENTA << i
                    << " " << BOLDBLUE << "aur/" << BOLD << aurPkgs[i].name
                    << " " << BOLDYELLOW << aurPkgs[i].desc
                    << NOCOLOR <<
                std::endl;
            for (size_t i = 0; i < pacPkgs.size(); i++)
                std::cout
                    << MAGENTA << i + aurPkgs.size() << " "
                    << BOLDMAGENTA << pacPkgs[i].db_name << '/' << BOLD << pacPkgs[i].name
                    << " " << BOLDYELLOW << pacPkgs[i].desc
                    << " " << BOLDGREEN << pacPkgs[i].version
                    << NOCOLOR << 
                std::endl;

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
