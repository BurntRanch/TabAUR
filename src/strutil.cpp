#include <strutil.hpp>

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (ending.length() > fullString.length())
	    return false;
    return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
}
