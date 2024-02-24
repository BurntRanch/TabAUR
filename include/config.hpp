#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#define TOML_HEADER_ONLY 0
#include <toml++/toml.hpp>

class Config {
public:
  Config();
  std::string getConfigDir();
  char* getCacheDir();
private:

};

#endif
