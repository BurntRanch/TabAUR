#include <cstdio>
#include "util.hpp"
#include "taur.hpp"

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(string const& fullString, std::string const& ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

bool hasStart(string const& fullString, std::string const& start) {
    if (start.length() > fullString.length())
        return false;
    return (0 == fullString.compare(0, start.length(), start));
}

void log_printf(int log, string fmt, ...) {
    va_list args;
    va_start(args, fmt);
    switch(log){
        case LOG_ERROR:
            std::cerr << BOLDRED << "====[ ERROR ]==== " << std::endl << BOLD; break;
        case LOG_WARN:
            std::cout << BOLDYELLOW << "Warning: " << BOLD; break;
        case LOG_INFO:
            std::cout << BOLDBLUE << "Info: " << BOLD; break;
        case LOG_DEBUG:
            if (config.debug)
                std::cout << BOLDMAGENTA << "[DEBUG]: " << BOLD;
            else
                return;
    }
    vprintf(fmt.c_str(), args);
    va_end(args);
    std::cout << NOCOLOR;
}

string sanitizeStr(string& str) {
    str.erase(sanitize(str.begin(), str.end()), str.end());
    return str;
}

std::string expandVar(std::string& str) {
    const char* env;
    if (str[0] == '~') {
        env = getenv("HOME");   // it's likely impossible for not having the $HOME env var setup
        str.replace(0, 1, env); // replace ~ with the $HOME value
    } else if (str[0] == '$') {
        str.erase(0, 1); // erase from str[0] to str[1]
        env = getenv(str.c_str());
        if (env == nullptr) {
            log_printf(LOG_ERROR, "No such enviroment variable: %s\n", str.c_str());
            exit(-1);
        }
        str = std::string(env);
    }

    return str;
}

// http://stackoverflow.com/questions/478898/ddg#478960
string execGet(string cmd) {
    std::array<char, 128> buffer;
    string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

// https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c#4654718
bool is_number(const string& s) {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

bool taur_exec(vector<const char*> cmd) {
    cmd.push_back(nullptr);
    int pid = fork();

    if (pid < 0) {
        log_printf(LOG_ERROR, "fork() failed: %s\n", strerror(errno));
        exit(127);
    }
    if (pid == 0) {
        execvp(cmd[0], const_cast<char* const*>(cmd.data()));
        log_printf(LOG_ERROR, "An error as occured: %s\n", strerror(errno));
        exit(127);
    }
    if (pid > 0) { // we wait for the command to finish then start executing the rest
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
    }
    return false;
}

std::vector<string> split(std::string text, char delim) {
    string              line;
    std::vector<string> vec;
    std::stringstream   ss(text);
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}
