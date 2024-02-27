#include <string>
#define TOML_IMPLEMENTATION
//#include <iterator>
#include <cstdlib>
#include <iostream>
#include <config.hpp>
#include <strutil.hpp>
#include <filesystem>

Config::Config(){
	if (!std::filesystem::exists(this->getCacheDir())) {
		std::cout << "TabAUR Cache folder was not found, Creating folder at " << this->getCacheDir() << "!" << std::endl;
		std::filesystem::create_directory(this->getCacheDir());
	}
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
	// insert cache lookup here
	
	// if no custom cache dir found, make it up
	
	return this->getSystemCacheDir() + "/TabAUR";
}

string Config::getSystemConfDir() {
  char *dir = std::getenv("XDG_CONFIG_HOME");
  if(dir != NULL && std::filesystem::exists(std::string(dir))) {
    std::string str_dir(dir);
    return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
  } else
    return std::string(std::getenv("HOME")) + "/.config";
}

string Config::getConfDir() {
	return this->getSystemConfDir() + "/TabAUR";
}

void Config::parseConf() {
  string config = "config.toml";
  toml::table tbl;
  try {
        tbl = toml::parse_file(config);
  }
  catch (const toml::parse_error& err) {
        std::cerr << "Parsing failed:\n" << err << "\n";
  }
}

int main_conf(void) {
  Config config;
  std::string dir = config.getCacheDir(); 
  std::cout << dir << std::endl;
  return 0;
}
