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

// this relies on main.cpp sanitizing the path itself
bool TaurBackend::download_tar(string url, string out_path) {
    std::ofstream out(out_path);
    if (!out.is_open())
        return false;

    cpr::Session session;
    session.SetUrl(cpr::Url(url));
    cpr::Response response = session.Download(out);

    out.close();

    bool isNested = out_path.find("/") != -1;

    if (isNested) {
    	return chdir(out_path.c_str()) == 0 && execlp("tar", "tar", "-xf", out_path.substr(out_path.rfind("/") + 1).c_str()) == 0;
    } else
    	return execlp("tar", "tar", "-xf", out_path.c_str()) == 0;

}

bool TaurBackend::download_pkg(string url, string out_path) {
    if (hasEnding(url, ".git"))
        return this->download_git(url, out_path);
    else if (hasEnding(url, ".tar.gz"))
        return this->download_tar(url, out_path);
    return false;
}

// this relies on main.cpp sanitizing the path itself
bool TaurBackend::install_pkg(TaurPkg_t pkg, string extracted_path) {
    string makepkg_bin = this->config.getConfigValue<string>("general.makepkgBin", "/bin/makepkg");

    extracted_path.erase(sanitize(extracted_path.begin(), extracted_path.end()), extracted_path.end());
    bool installSuccess = chdir(extracted_path.c_str()) == 0 && execlp(makepkg_bin.c_str(), makepkg_bin.c_str(), "-si") == 0;
    if (!installSuccess)
	    return false;

    this->db.addPkg(pkg);

    return true;
}

// https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c#4654718
bool is_number(const string& s) {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

string getUrl(rapidjson::Value& pkgJson, bool returnGit = false) {
    if (returnGit)
        return ("https://aur.archlinux.org/" + string(pkgJson["Name"].GetString()) + ".git");
    else
	    return ("https://aur.archlinux.org/" + string(pkgJson["URLPath"].GetString()));
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
