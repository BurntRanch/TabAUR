#ifndef UTIL_HPP
#define UTIL_HPP

#include <alpm.h>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

using std::string;
// taken from pacman
#define _(str) (char*)str

enum log_level {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG
};

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c#874160
bool hasEnding(string const& fullString, string const& ending);
bool hasStart(string const& fullString, string const& start);
void log_printf(int log, string fmt, ...);
string expandVar(string& str);
bool is_number(const string& s);
bool taur_read_exec(std::vector<const char*> cmd, string *output);
void interruptHandler(int);
bool taur_exec(std::vector<const char*> cmd);
void sanitizeStr(string& str);
bool is_package_from_syncdb(alpm_pkg_t *pkg, alpm_list_t *syncdbs);
bool commitTransactionAndRelease(alpm_handle_t *handle, bool soft = false);
string shell_exec(string cmd);
std::vector<string> split(string text, char delim);

template <typename T>
T sanitize(T beg, T end) {
    T dest = beg;
    for (T itr = beg; itr != end; ++itr)
        // if its:
        // 1. a digit
        // 2. an uppercase letter
        // 3. a lowercase letter
        if (!(((*itr) >= 48 && (*itr) <= 57) && ((*itr) >= 65 && (*itr) <= 90) && ((*itr) >= 97 && (*itr) <= 122)))
            *(dest++) = *itr;
    return dest;
}

static inline std::vector<string> secret = {
    {"Ingredients:"},
    {R"#(
    3/4 cup milk
    1/2 cup vegetable oil
    1 large egg
    2 cups all-purpose flour
    1/2 cup white sugar
    2 teaspoons baking powder
    1/2 teaspoon salt
    3/4 cup mini semi-sweet chocolate chips
    1 and 1/2 tablespoons white sugar
    1 tablespoon brown sugar
    )#"},
    {R"#(Step 1:
    Preheat the oven to 400 degrees F (200 degrees C).
    Grease a 12-cup muffin tin or line cups with paper liners.
    )#"},
    {R"#(Step 2:
    Combine milk, oil, and egg in a small bowl until well blended. 
    Combine flour, 1/2 cup sugar, baking powder, and salt together in a large bowl, making a well in the center. 
    Pour milk mixture into well and stir until batter is just combined; fold in chocolate chips.
    )#"},
    {R"#(Step 3:
    Spoon batter into the prepared muffin cups, filling each 2/3 full. 
    Combine 1 and 1/2 tablespoons white sugar and 1 tablespoon brown sugar in a small bowl; sprinkle on tops of muffins.
    )#"},
    {R"#(Step 4:
    Bake in the preheated oven until tops spring back when lightly pressed, about 18 to 20 minutes. 
    Cool in the tin briefly, then transfer to a wire rack.
            
            Serve warm or cool completely.
    )#"},
    {"Credits to: https://www.allrecipes.com/recipe/7906/chocolate-chip-muffins/"}
};

#endif
