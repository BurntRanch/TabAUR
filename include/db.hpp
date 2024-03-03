#ifndef DB_HPP
#define DB_HPP

#include <strutil.hpp>
#include <vector>
#include <optional>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <exception>

using std::string;
using std::ifstream;
using std::vector;
using std::optional;

struct TAUR_PKG {
	std::string name;
	std::string version;
	std::string url;
};

class DB {
    public:
        DB(string name);
        ~DB();
        optional<int> findPkg(string name);
        optional<TAUR_PKG> getPkg(string name);
        void addPkg(TAUR_PKG pkg);
    private:
        string fileName;
        vector<string> dbRecords;
};

#endif
