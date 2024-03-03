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

struct TaurPkg_t {
	string name;
	string version;
	string url;
};

class DB {
    public:
        DB(string name);
        ~DB();
        optional<int> findPkg(string name);
        optional<TaurPkg_t> getPkg(string name);
        void addPkg(TaurPkg_t pkg);
    private:
        string fileName;
        vector<string> dbRecords;
};

#endif
