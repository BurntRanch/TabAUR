#define TOML_IMPLEMENTATION
#include <cstdlib>
#include <iostream>
#include <config.hpp>
#include <strutil.hpp>
#include <filesystem>

Config::Config(){
	if (!std::filesystem::exists(this->getCacheDir())) {
		std::cout << "TabAUR config folder was not found, Creating folder at " << this->getCacheDir() << "!" << std::endl;
		std::filesystem::create_directory(this->getCacheDir());
	}
  string filename = getConfigDir() + "/config.toml";
  
  if (!std::filesystem::exists(this->getConfigDir())) {
    std::cout << "TabAUR config folder was not found, Creating folder at " << this->getConfigDir() << "!" << std::endl;
		std::filesystem::create_directory(this->getConfigDir());
  }
  loadConfigFile(filename);
}

string Config::getSystemCacheDir() {
  char *dir = std::getenv("XDG_CACHE_HOME");
  if(dir != NULL && std::filesystem::exists(std::string(dir))) {
    std::string str_dir(dir);
    return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
  } else
    return std::string(std::getenv("HOME")) + "/.cache";
}

string Config::getCacheDir() {
<<<<<<< HEAD
	// insert cache lookup here
	
	// if no custom cache dir found, make it up
	
=======
>>>>>>> 00e7f3a096073915e128a1355cd25d979f63c978
	return this->getSystemCacheDir() + "/TabAUR";
}

string Config::getSystemConfigDir() {
  char *dir = std::getenv("XDG_CONFIG_HOME");
  if(dir != NULL && std::filesystem::exists(std::string(dir))) {
    std::string str_dir(dir);
    return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
  } else
    return std::string(std::getenv("HOME")) + "/.config";
}

<<<<<<< HEAD
string Config::getConfDir() {
	return this->getSystemConfDir() + "/TabAUR";
=======
string Config::getConfigDir() {
	return this->getSystemConfigDir() + "/TabAUR";
>>>>>>> 00e7f3a096073915e128a1355cd25d979f63c978
}

void Config::loadConfigFile(string filename) {
  toml::table tbl;
    try
    {
        tbl = toml::parse_file(filename);
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << "Parsing failed:\n" << err << "\n";
        exit(-1);
    }
}
