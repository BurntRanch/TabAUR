#ifndef STRUTIL_HPP
#define STRUTIL_HPP

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(std::string const& fullString, std::string const& ending);
bool hasStart(std::string const& fullString, std::string const& start);

std::vector<std::string> split(std::string text, char delim);

#endif
