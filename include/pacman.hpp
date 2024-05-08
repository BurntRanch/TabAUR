// code taken from pacman utils
// putted it on a separeted file for keeping the codebase a bit more clean
// args.cpp and args.hpp has part of pacman code too

#include <alpm.h>
#include <alpm_list.h>
#include <string>
#include <string_view>
#include <vector>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

using std::string;
using std::string_view;

#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))
#define CLBUF_SIZE   4096

/* As of 2015/10/20, the longest title (all locales considered) was less than 30
 * characters long. We set the title maximum length to 50 to allow for some
 * potential growth.
 */
#define TITLE_MAXLEN 50

/* The term "title" refers to the first field of each line in the package
 * information displayed by pacman. Titles are stored in the `titles` array and
 * referenced by the following indices.
 */
enum {
    T_ARCHITECTURE = 0,
    T_BACKUP_FILES,
    T_BUILD_DATE,
    T_COMPRESSED_SIZE,
    T_CONFLICTS_WITH,
    T_DEPENDS_ON,
    T_DESCRIPTION,
    T_DOWNLOAD_SIZE,
    T_GROUPS,
    T_INSTALL_DATE,
    T_INSTALL_REASON,
    T_INSTALL_SCRIPT,
    T_INSTALLED_SIZE,
    T_LICENSES,
    T_MD5_SUM,
    T_NAME,
    T_OPTIONAL_DEPS,
    T_OPTIONAL_FOR,
    T_PACKAGER,
    T_PROVIDES,
    T_REPLACES,
    T_REPOSITORY,
    T_REQUIRED_BY,
    T_SHA_256_SUM,
    T_SIGNATURES,
    T_URL,
    T_VALIDATED_BY,
    T_VERSION,
    /* the following is a sentinel and should remain in last position */
    _T_MAX
};

static char titles[_T_MAX][TITLE_MAXLEN * sizeof(wchar_t)];

int getcols_fd(int fd);
unsigned short getcols(void);
void string_display(string_view title, string_view string, unsigned short cols = getcols());
void list_display(const char *title, const alpm_list_t *list, unsigned short maxcols = getcols());
void deplist_display(const char *title, alpm_list_t *deps, unsigned short cols = getcols());
void indentprint(const char *str, unsigned short indent, unsigned short cols = getcols());
void list_display_linebreak(const char *title, const alpm_list_t *list, unsigned short maxcols = getcols());
