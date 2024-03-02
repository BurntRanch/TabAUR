#define TOML_IMPLEMENTATION
#include <cstdlib>
#include <iostream>
#include <config.hpp>
#include <strutil.hpp>
#include <filesystem>

using std::string;
using std::getenv;
using std::optional;
using std::ofstream;

Config::Config() {
  string cacheDir = this->getCacheDir();
  string configDir = this->getConfigDir();
  if (!std::filesystem::exists(cacheDir)) {
    std::cout << "TabAUR cache folder was not found, Creating folder at " << cacheDir << "!" << std::endl;
    std::filesystem::create_directory(cacheDir);
  }
	
  if (!std::filesystem::exists(configDir)) {
    std::cout << "TabAUR config folder was not found, Creating folder at " << configDir << "!" << std::endl;
    std::filesystem::create_directory(configDir);
    ofstream configFile(configDir + "/config.toml");
    configFile.close();
  }
  string filename = configDir + "/config.toml";
  loadConfigFile(filename);
}

string Config::getHomeCacheDir() {
    char* dir = getenv("XDG_CACHE_HOME");
    if (dir != NULL && std::filesystem::exists(string(dir))) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else
        return string(getenv("HOME")) + "/.cache";
}

string Config::getCacheDir() {
	optional<string> cacheDir = this->tbl["cacheDir"].value<string>();
	if (cacheDir && std::filesystem::exists(cacheDir.value()))
		return cacheDir.value();
	// if no custom cache dir found, make it up
	
	return this->getHomeCacheDir() + "/TabAUR";
}

string Config::getHomeConfigDir() {
    char* dir = getenv("XDG_CONFIG_HOME");
    if (dir != NULL && std::filesystem::exists(string(dir))) {
        string str_dir(dir);
        return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
    } else
        return string(getenv("HOME")) + "/.config";
}

string Config::getConfigDir() {
    return this->getHomeConfigDir() + "/TabAUR";
}

void Config::loadConfigFile(string filename) {
    try
    {
        this->tbl = toml::parse_file(filename);
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << "Parsing failed:\n" << err << "\n";
        exit(-1);
    }
}
