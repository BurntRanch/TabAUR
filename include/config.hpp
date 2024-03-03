#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#define TOML_HEADER_ONLY 1
#include <toml.hpp>

using std::string;
using std::optional;

class Config {
public:
    Config();
    string getHomeCacheDir();
    string getCacheDir();
    string getHomeConfigDir();
    string getConfigDir();
  
    //stupid c++ that wants template functions in header
    template<typename T>
    T getConfigValue(string value, T fallback) {
      // .value<T>().value(), nice.
      return this->tbl.at_path(value).value<T>() ? this->tbl.at_path(value).value<T>().value() : fallback;
    }
  
    void   loadConfigFile(string filename);
private:
    toml::table tbl;
};

extern Config config;

#endif
