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

optional<int> DB::findPkg(string name) {
    for (size_t i = 0; i < this->dbRecords.size(); i++) {
        vector<string> recordDetails = split(this->dbRecords[i], PADDING);
        if (recordDetails[0] == name)
            return i;
    }

    return {};
}

optional<TaurPkg_t> DB::getPkg(string name) {
    optional<int> oi = findPkg(name);
    if (oi) {
        int i = oi.value();
        vector<string> recordDetails = split(this->dbRecords[i], PADDING);
        return (TaurPkg_t) {recordDetails[0], recordDetails[1], recordDetails[2]};
    }

    return {};
}

// Adds/Updates packages
void DB::addPkg(TaurPkg_t pkg) {
    if (getPkg(pkg.name)) {
        int i = findPkg(pkg.name).value();
        this->dbRecords[i] = pkg.name + PADDING + pkg.version + PADDING + pkg.url;

        return;
    }

    this->dbRecords.push_back(pkg.name + PADDING + pkg.version + PADDING + pkg.url);
}

void DB::removePkg(TaurPkg_t pkg) {
    optional<int> oe = findPkg(pkg.name);

    if (oe) {
        int e = oe.value();

        this->dbRecords.erase(this->dbRecords.begin() + e);
    }
}
