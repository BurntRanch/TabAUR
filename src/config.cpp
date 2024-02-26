#define TOML_IMPLEMENTATION
//#include <iterator>
#include <cstdlib>
#include <iostream>
#include <config.hpp>

Config::Config(){
  std::cout << "Initialized Config testing\n"; 
}

char* Config::getCacheDir() {
  char* dir = std::getenv("XDG_CACHE_HOME");
  if(!strcmp(dir, ""))
    return dir;
  else
    return (char *)"$HOME/.cache";
}

int main_conf(void) {
  Config config;
  std::string dir = config.getCacheDir(); 
  std::cout << dir << std::endl;
  return 0;
}
