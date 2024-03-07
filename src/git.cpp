// Functions for TabAUR, These include printing stuff and others.
// main.cpp simply pieces each function together to make the program work.
#include <git.hpp>
#include <config.hpp>
#include <optional>

TaurBackend::TaurBackend(Config cfg, string dbLocation) : config(cfg), db(DB((dbLocation + "/pkg.db"))) {
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

template <typename T>
T sanitize(T beg, T end) {
    T dest = beg;
    for (T itr = beg; itr != end; ++itr)
        // if its:
        // 1. a digit
        // 2. an uppercase letter
        // 3. a lowercase letter
        if (!(((*itr) >= 48 && (*itr) <= 57) && ((*itr) >= 65 && (*itr) <= 90) && ((*itr) >= 97 && (*itr) <= 122)))
            *(dest++) = *itr;
    return dest;
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
    bool isNested = out_path.find("/") != -1;

    out_path.erase(sanitize(out_path.begin(), out_path.end()), out_path.end());
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
	    return ("https://aur.archlinux.org/" + string(pkgJson["URLPath"].GetString()));
}

TaurPkg_t parsePkg(rapidjson::Value& pkgJson, bool returnGit = false) {
    return (TaurPkg_t) { pkgJson["Name"].GetString(), pkgJson["Version"].GetString(), getUrl(pkgJson, returnGit) };
}

vector<TaurPkg_t> TaurBackend::fetch_pkgs(vector<TaurPkg_t> pkgs) {
    if (pkgs.empty())
        return vector<TaurPkg_t>();

    std::string varName = cpr::util::urlEncode("arg[]");
    std::string  urlStr = "https://aur.archlinux.org/rpc/v5/info?" + varName + "=" + cpr::util::urlEncode(pkgs[0].name);

    for (int i = 1; i < pkgs.size(); i++)
        urlStr += ("&" + varName + "=" + cpr::util::urlEncode(pkgs[i].name));

    cpr::Url        url = cpr::Url(urlStr);
    cpr::Response  resp = cpr::Get(url);

    if (resp.status_code != 200)
        return vector<TaurPkg_t>();

    rapidjson::Document json;
    json.Parse(resp.text.c_str());

    vector<TaurPkg_t> out;
    for (int i = 0; i < json["resultcount"].GetInt(); i++)
        out.push_back(parsePkg(json["results"][i]));

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

bool runRemovePkg(DB *db, TaurPkg_t pkg, string finalPackageList) {
    bool removeSuccess = system(("sudo pacman -Rns " + finalPackageList).c_str()) == 0;
    if (!removeSuccess)
        return false;

    db->remove_pkg(pkg);

    return true;
}

bool TaurBackend::remove_pkg(string pkgName) {
    optional<TaurPkg_t> pkg = this->db.get_pkg(pkgName);
    if (!pkg) {
        std::cerr << "Failed to find your package " << pkgName << " in the TabAUR DB" << std::endl; 
        return false;
    }

    pkgName.erase(sanitize(pkgName.begin(), pkgName.end()), pkgName.end());

    string packages_str = exec("pacman -Qmq | grep \"" + pkgName + "\"");
    vector<string> packages = split(packages_str, '\n');

    if (packages.empty())
        return false;

    if (packages.size() == 1)
        return runRemovePkg(&this->db, pkg.value(), packages[0]);

    std::cout << "Choose packages to exclude from this removal, (Seperate by spaces):" << std::endl;
    for (size_t i = 0; i < packages.size(); i++)
        std::cout << "[" << i << "] " << packages[i] << std::endl;

    string excluded;
    std::cin >> excluded;

    if (!excluded.empty()) {
        vector<string> excludedIndexes = split(excluded, ' ');
        for (size_t i = 0; i < excludedIndexes.size(); i++) {
            try {
                int excludedIndex = stoi(excludedIndexes[i]);

                if (excludedIndex >= packages.size())
                    continue;

                packages.erase(packages.begin() + excludedIndex);
            } catch (std::invalid_argument) {
                std::cerr << "Invalid argument! Assuming no exclusion." << std::endl;
            }
        }
    }

    string finalPackageList = "";

    for (size_t i = 0; i < packages.size(); i++) {
        finalPackageList += packages[i] + " ";
    }

    return runRemovePkg(&this->db, pkg.value(), finalPackageList);
}

bool TaurBackend::install_pkg(TaurPkg_t pkg, string extracted_path) {
    string makepkg_bin = this->config.getConfigValue<string>("general.makepkgBin", "/bin/makepkg");

    // never forget to sanitize
    extracted_path.erase(sanitize(extracted_path.begin(), extracted_path.end()), extracted_path.end());
    makepkg_bin.erase(sanitize(makepkg_bin.begin(), makepkg_bin.end()), makepkg_bin.end());

    bool installSuccess = system(("cd \"" + extracted_path + "\" && " + makepkg_bin + " -si").c_str()) == 0;
    if (!installSuccess)
	    return false;

    this->db.add_pkg(pkg);

    return true;
}

bool TaurBackend::update_all_pkgs(path cacheDir) {
    vector<TaurPkg_t> pkgs = this->db.get_all_pkgs();
    vector<TaurPkg_t> onlinePkgs = this->fetch_pkgs(pkgs);

    int updatedPkgs = 0;

    if (onlinePkgs.size() != pkgs.size())
        std::cout << "Couldn't get all packages! Still trying to update the others." << std::endl;

    for (size_t i = 0; i < onlinePkgs.size(); i++) {
        int pkgIndex;
        bool found = false;

        for (pkgIndex = 0; pkgIndex < pkgs.size(); pkgIndex++) {
            if (pkgs[pkgIndex].name == onlinePkgs[i].name) {
                found = true;
                break;
            }
        }

        if (!found) {
            std::cout << "We couldn't find " << onlinePkgs[i].name << " in the local pkg database, This shouldn't happen." << std::endl;
            continue;
        }

        if (pkgs[pkgIndex].version == onlinePkgs[i].version) {
            continue;
        }

        std::cout << "Upgrading package " << pkgs[pkgIndex].name << " from version " << pkgs[pkgIndex].version << " to version " << onlinePkgs[i].version << "!" << std::endl;
        
        bool downloadSuccess = this->download_pkg(onlinePkgs[i].url, cacheDir / onlinePkgs[i].name);

        if (!downloadSuccess)
            return false;

        bool installSuccess = this->install_pkg(onlinePkgs[i], cacheDir / onlinePkgs[i].name);

        if (!installSuccess)
            return false;

        updatedPkgs++;
    }

    std::cout << "Upgraded " << updatedPkgs << " packages." << std::endl;

    return true;
}

vector<TaurPkg_t> TaurBackend::get_all_local_pkgs() {
    return this->db.get_all_pkgs();
}

// https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c#4654718
bool is_number(const string& s) {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

std::optional<TaurPkg_t> getPkg(rapidjson::Document& doc, bool useGit = false) {
    int resultcount = doc["resultcount"].GetInt();
    if (resultcount == 1) {
	    return (TaurPkg_t) {string(doc["results"][0]["Name"].GetString()), string(doc["results"][0]["Version"].GetString()), getUrl(doc["results"][0], useGit)};
    } else if (resultcount > 1) {
        std::cout << "TabAUR has found multiple packages relating to your search query, Please pick one." << std::endl;
        string input;
        do {
            if (!input.empty())
                std::cout << "Invalid input!" << std::endl;

            for (int i = 0; i < resultcount; i++)
                std::cout << "[" << i << "]: " << doc["results"][i]["Name"].GetString() << std::endl;

            std::cout << "Choose a package to download: ";
            std::cin >> input;
        } while (!is_number(input) || std::stoi(input) >= resultcount);

        rapidjson::Value& selected = doc["results"][std::stoi(input)];
        //if (selected["URL"].IsString())
        //	return (string(selected["URL"].GetString()) + ".git");
        //else
    	string url = getUrl(selected, useGit);
    	return (TaurPkg_t) { string(selected["Name"].GetString()), string(selected["Version"].GetString()), url };
    }

    return {};
}

// Returns an optional that is empty if an error occurs
// status will be set to -1 in the case of an error as well.
std::optional<TaurPkg_t> TaurBackend::search_aur(string query, int* status, bool useGit) {
    // link to AUR API
    cpr::Url            url(("https://aur.archlinux.org/rpc/v5/search/" + cpr::util::urlEncode(query) + "?by=name"));
    cpr::Response       r = cpr::Get(url);

    string         raw_text_response = r.text;

    rapidjson::Document json_response;
    json_response.Parse(raw_text_response.c_str());

    if (strcmp(json_response["type"].GetString(), "error") == 0) {
        *status = -1;
        return {};
    }

    if (json_response["resultcount"].GetInt() == 0) {
        *status = -2;
        return {};
    }

    *status = 0;

    return getPkg(json_response, useGit);
}
