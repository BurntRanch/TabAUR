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
        optional<int> find_pkg(string name);
        optional<TaurPkg_t> get_pkg(string name);
        void add_pkg(TaurPkg_t pkg);
        void remove_pkg(TaurPkg_t pkg);
        vector<TaurPkg_t> get_all_pkgs();
    private:
        string fileName;
        vector<string> dbRecords;
};

#endif
