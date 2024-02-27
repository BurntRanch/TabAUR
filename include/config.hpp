#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#define TOML_HEADER_ONLY 1
#include <toml.hpp>

class Config {
public:
  Config();
  std::string getConfigDir();
  std::string getSystemCacheDir();
  std::string getCacheDir();
private:

};

#endif
