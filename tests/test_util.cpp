#include "util.hpp"
#include "config.hpp"
#include "pacman.hpp"
#include "taur.hpp"

#include "catch2/catch_amalgamated.hpp"
using Catch::Matchers::Equals;

std::unique_ptr<Config> config = std::make_unique<Config>();

TEST_CASE( "util.cpp test suitcase", "[Util]" ) {
    alpm_errno_t err;
    config->handle = alpm_initialize("/", "/var/lib/pacman", &err);

    SECTION( "String asserts" ) {
        string env = std::getenv("HOME");
        string path = "~/.config/rule34";
        REQUIRE(hasEnding("I want the end", "end"));
        REQUIRE(hasStart("And now the begin, then  I want the end", "A"));
        REQUIRE(expandVar(path) == env + "/.config/rule34");
        REQUIRE(shell_exec("echo hello") == "hello");
    }
    
    SECTION("ALPM asserts") {
        alpm_list_t *syncdbs = alpm_get_syncdbs(config->handle);
        REQUIRE(is_package_from_syncdb("linux", syncdbs));
    }
}
