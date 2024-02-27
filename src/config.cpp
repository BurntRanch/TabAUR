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

std::string Config::getSystemCacheDir() {
  char *dir = std::getenv("XDG_CACHE_HOME");
  if(dir != NULL && std::filesystem::exists(std::string(dir))) {
    std::string str_dir(dir);
    return hasEnding(str_dir, "/") ? str_dir.substr(0, str_dir.rfind('/')) : str_dir;
  } else
    return std::string(std::getenv("HOME")) + "/.cache";
}

std::string Config::getCacheDir() {
	// insert config lookup here
	
	// if no custom cache dir found, make it up
	
	return this->getSystemCacheDir() + "/TabAUR";
}

int main_conf(void) {
  Config config;
  std::string dir = config.getCacheDir(); 
  std::cout << dir << std::endl;
  return 0;
}
