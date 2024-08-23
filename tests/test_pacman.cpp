#include <memory>
#include "config.hpp"
#include "util.hpp"

const std::string& configDir = getConfigDir();
std::string configfile = (configDir + "/config.toml");
std::string themefile  = (configDir + "/theme.toml");

std::unique_ptr<Config> config = std::make_unique<Config>(configfile, themefile, configDir);
