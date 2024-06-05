/*
 *
 *  Copyright (c) 2006-2022 Pacman Development Team <pacman-dev@lists.archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


//  general functions taken from pacman source code
//  putted it on a separeted file for keeping the codebase a bit more clean

#include "pacman.hpp"
#include "util.hpp"
#include <cstdlib>
#include <unistd.h>

static int cached_columns = -1;

int getcols_fd(int fd)
{
    int width = -1;

    if (!isatty(fd)) {
        return 0;
    }

#if defined(TIOCGSIZE)
    struct ttysize win;
    if (ioctl(fd, TIOCGSIZE, &win) == 0) {
        width = win.ts_cols;
    }
#elif defined(TIOCGWINSZ)
    struct winsize win;
    if (ioctl(fd, TIOCGWINSZ, &win) == 0) {
        width = win.ws_col;
    }
#endif

    if (width <= 0) {
        return -EIO;
    }

    return width;
}

unsigned short getcols(void) {
    const char *e;
    int         c = -1;

    if (cached_columns >= 0) {
        return cached_columns;
    }

    e = getenv("COLUMNS");
    if (e && e[0] != '\0') {
        char *p = NULL;
        c       = strtol(e, &p, 10);
        if (*p != '\0') {
            c = -1;
        }
    }

    if (c < 0) {
        c = getcols_fd(STDOUT_FILENO);
    }

    if (c < 0) {
        c = 80;
    }

    cached_columns = c;
    return c;
}

static size_t string_length(const char *s) {
    int      len;
    wchar_t *wcstr;

    if (!s || s[0] == '\0') {
        return 0;
    }
    if (strstr(s, "\033")) {
        char *replaced = (char *)malloc(sizeof(char) * strlen(s));
        int   iter     = 0;
        for (; *s; s++) {
            if (*s == '\033') {
                while (*s != 'm') {
                    s++;
                }
            } else {
                replaced[iter] = *s;
                iter++;
            }
        }
        replaced[iter] = '\0';
        len            = iter;
        wcstr          = (wchar_t *)calloc(len, sizeof(wchar_t));
        len            = mbstowcs(wcstr, replaced, len);
        len            = wcswidth(wcstr, len);
        free(wcstr);
        free(replaced);
    } else {
        /* len goes from # bytes -> # chars -> # cols */
        len   = strlen(s) + 1;
        wcstr = (wchar_t *)calloc(len, sizeof(wchar_t));
        len   = mbstowcs(wcstr, s, len);
        len   = wcswidth(wcstr, len);
        free(wcstr);
    }

    return len;
}

void list_display_linebreak(const char *title, const alpm_list_t *list, unsigned short maxcols) {
    unsigned short len = 0;

    if (title) {
        len = (unsigned short)string_length(title) + 1;
        fmt::print(BOLD, "{}{} ", title, NOCOLOR);
    }

    if (!list) {
        printf("%s\n", _("None"));
    } else {
        const alpm_list_t *i;
        /* Print the first element */
        indentprint((const char *)list->data, len, maxcols);
        printf("\n");
        /* Print the rest */
        for (i = alpm_list_next(list); i; i = alpm_list_next(i)) {
            size_t j;
            for (j = 1; j <= len; j++) {
                printf(" ");
            }
            indentprint((const char *)i->data, len, maxcols);
            printf("\n");
        }
    }
}

/* output a string, but wrap words properly with a specified indentation
 */
void indentprint(const char *str, unsigned short indent, unsigned short cols) {
    wchar_t       *wcstr;
    const wchar_t *p;
    size_t         len, cidx;

    if (!str) {
        return;
    }

    /* if we're not a tty, or our tty is not wide enough that wrapping even makes
	 * sense, print without indenting */
    if (cols == 0 || indent > cols) {
        fputs(str, stdout);
        return;
    }

    len   = strlen(str) + 1;
    wcstr = (wchar_t *)calloc(len, sizeof(wchar_t));
    len   = mbstowcs(wcstr, str, len);
    p     = wcstr;
    cidx  = indent;

    if (!p || !len) {
        free(wcstr);
        return;
    }

    while (*p) {
        if (*p == L' ') {
            const wchar_t *q, *next;
            p++;
            if (p == NULL || *p == L' ')
                continue;
            next = wcschr(p, L' ');
            if (next == NULL) {
                next = p + wcslen(p);
            }
            /* len captures # cols */
            len = 0;
            q   = p;
            while (q < next) {
                len += wcwidth(*q++);
            }
            if ((len + 1) > (cols - cidx)) {
                /* wrap to a newline and reindent */
                printf("\n%-*s", (int)indent, "");
                cidx = indent;
            } else {
                printf(" ");
                cidx++;
            }
            continue;
        }
        printf("%lc", (wint_t)*p);
        cidx += wcwidth(*p);
        p++;
    }
    free(wcstr);
}

void string_display(string_view title, string_view string, unsigned short cols) {
    if (title.length()) {
        fmt::print(BOLD, "{}{} ", title, NOCOLOR);
    }
    if (string.empty()) {
        printf(_("None"));
    } else {
        /* compute the length of title + a space */
        size_t len = string_length(title.data()) + 1;
        indentprint(string.data(), (unsigned short)len, cols);
    }
    fmt::print("\n");
}

void list_display(const char *title, const alpm_list_t *list, unsigned short maxcols) {
    const alpm_list_t *i;
    size_t             len = 0;

    if (title) {
        len = string_length(title) + 1;
        fmt::print(BOLD, "{}{} ", title, NOCOLOR);
    }

    if (!list) {
        printf("%s\n", _("None"));
    } else {
        size_t      cols = len;
        const char *str  = (const char *)list->data;
        fputs(str, stdout);
        cols += string_length(str);
        for (i = alpm_list_next(list); i; i = alpm_list_next(i)) {
            str      = (const char *)i->data;
            size_t s = string_length(str);
            /* wrap only if we have enough usable column space */
            if (maxcols > len && cols + s + 2 >= maxcols) {
                size_t j;
                cols = len;
                printf("\n");
                for (j = 1; j <= len; j++) {
                    printf(" ");
                }
            } else if (cols != len) {
                /* 2 spaces are added if this is not the first element on a line. */
                printf("  ");
                cols += 2;
            }
            fputs(str, stdout);
            cols += s;
        }
        putchar('\n');
    }
}

/** Turn a depends list into a text list.
 * @param deps a list with items of type alpm_depend_t
 */
void deplist_display(const char *title, alpm_list_t *deps, unsigned short cols) {
    alpm_list_t *i, *text = NULL;
    for (i = deps; i; i = alpm_list_next(i)) {
        alpm_depend_t *dep = (alpm_depend_t *)i->data;
        text               = alpm_list_add(text, alpm_dep_compute_string(dep));
    }
    list_display(title, text, cols);
    FREELIST(text);
}
