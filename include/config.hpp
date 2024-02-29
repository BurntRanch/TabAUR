#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#define TOML_HEADER_ONLY 1
#include <toml.hpp>

using std::string;

class Config {
public:
  Config();
  string getHomeCacheDir();
  string getCacheDir();
  string getHomeConfigDir();
  string getConfigDir();
  void   loadConfigFile(string filename);
private:
	toml::table tbl;
};

#endif
