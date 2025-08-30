#include "catch2/catch_amalgamated.hpp"
#include "config.hpp"
#include "pacman.hpp"
#include "taur.hpp"
#include "util.hpp"
using Catch::Matchers::Equals;

#include <memory>

#include "config.hpp"
#include "util.hpp"

const std::string& configDir  = getConfigDir();
std::string        configfile = (configDir + "/config.toml");
std::string        themefile  = (configDir + "/theme.toml");

std::unique_ptr<Config> config = std::make_unique<Config>(configfile, themefile, configDir);

TEST_CASE("util.cpp test suitcase", "[Util]")
{
    alpm_errno_t err;
    config->handle = alpm_initialize("/", "/var/lib/pacman", &err);

    SECTION("String asserts")
    {
        string env  = std::getenv("HOME");
        string path = "~/.config/rule34";
        REQUIRE(hasEnding("I want the end", "end"));
        REQUIRE(hasStart("And now the begin, then  I want the end", "And"));
        REQUIRE(expandVar(path) == env + "/.config/rule34");
        REQUIRE(shell_exec("echo hello") == "hello");
    }
}
