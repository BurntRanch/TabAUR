#include <db.hpp>

#define PADDING '\1'

using std::ofstream;
using std::ios;

DB::DB(string name) {
    if (!std::filesystem::exists(name)) {
        ofstream touch(name);
        touch.close();
    }

    ifstream file(name, ios::binary | ios::ate);
    if (!file.is_open())
        throw std::invalid_argument("DB File (" + name + ") could not be opened as ifstream");

    ifstream::pos_type pos = file.tellg();
    
    if (pos < 0)
        throw std::invalid_argument("Failed to read length from DB File (" + name + ").");

    this->fileName = name;

    vector<char> fileContent = vector<char>((int)pos + 1);

    file.seekg(0, ios::beg);
    file.read(&fileContent[0], pos);

    file.close();

    this->dbRecords = split(string(fileContent.begin(), fileContent.end()), '\n');
}

DB::~DB() {
    ofstream file(this->fileName, ios::trunc);

    if (!file.is_open()) {
        std::cout << ("DB File (" + this->fileName + ") could not be opened as ofstream") << std::endl;
        return;
    }

    for (size_t i = 0; i < this->dbRecords.size(); i++) {
        // empty
        if (this->dbRecords[i].length() <= 5)
            continue;
        file.write((this->dbRecords[i] + '\n').c_str(), this->dbRecords[i].size() + 1);
    }

    file.close();
}

optional<int> DB::find_pkg(string name) {
    for (size_t i = 0; i < this->dbRecords.size(); i++) {
        vector<string> recordDetails = split(this->dbRecords[i], PADDING);
        if (recordDetails[0] == name)
            return i;
    }

    return {};
}

TaurPkg_t parseDBRecord(string record) {
    vector<string> recordDetails = split(record, PADDING);
    return (TaurPkg_t) { recordDetails[0], recordDetails[1], recordDetails[2] };
}

optional<TaurPkg_t> DB::get_pkg(string name) {
    optional<int> oi = this->find_pkg(name);
    if (oi) {
        int i = oi.value();
        return parseDBRecord(this->dbRecords[i]);
    }

    return {};
}

// Adds/Updates packages
void DB::add_pkg(TaurPkg_t pkg) {
    if (this->get_pkg(pkg.name)) {
        int i = this->find_pkg(pkg.name).value();
        this->dbRecords[i] = pkg.name + PADDING + pkg.version + PADDING + pkg.url;

        return;
    }

    this->dbRecords.push_back(pkg.name + PADDING + pkg.version + PADDING + pkg.url);
}

void DB::remove_pkg(TaurPkg_t pkg) {
    optional<int> oe = this->find_pkg(pkg.name);

    if (oe) {
        int e = oe.value();

        this->dbRecords.erase(this->dbRecords.begin() + e);
    }
}

vector<TaurPkg_t> DB::get_all_pkgs() {
    vector<TaurPkg_t> out;
    for (size_t i = 0; i < this->dbRecords.size(); i++) {
        if (this->dbRecords[i].length() <= 5)
            continue;
        out.push_back(parseDBRecord(this->dbRecords[i]));
    }

    return out;
}
