/*
 * Portions of this code are derived from Pacman
 * Copyright (c) 2006-2025 Pacman Development Team <pacman-dev@lists.archlinux.org>
 * Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *
 * Licensed under the GNU General Public License, version 2 or later.
 */

#ifndef ARGS_HPP
#define ARGS_HPP

#include <alpm.h>
#include <getopt.h>

#include "util.hpp"

enum
{
    OP_MAIN = 1,
    OP_SYNC,
    OP_REM,
    OP_QUERY,
    OP_UPGRADE,
    OP_PACMAN,  // when it's different from -S,R,Q we gonna use pacman
};

enum
{
    OP_ASK = 1000,
    OP_CACHEDIR,
    OP_AURONLY,
    OP_SUDO,
    OP_USEGIT,
    OP_REFRESH,
    OP_SYSUPGRADE,
    OP_CLEANBUILD,
    OP_INFO,
    OP_COLORS,
    OP_DEBUG,
    OP_CONFIG,
    OP_THEME,
    OP_SEARCH,
    OP_NOCONFIRM,
    OP_QUIET,
    OP_TEST_COLORS,
    OP_RECURSIVE,
    OP_NOSAVE,
};

struct Operation_t
{
    u_short op;
    u_short op_s_sync;
    u_short op_s_upgrade;
    u_short op_s_search;
    u_short op_s_pacman;
    u_short op_s_cleanbuild;

    u_short op_q_search;
    u_short op_q_info;

    bool    requires_root = false;
    u_short help;
    u_short version;
    u_short test_colors;
    u_short show_recipe;
};

inline struct Operation_t      op;
extern alpm_list_smart_deleter taur_targets;

int  parsearg_op(const int opt, const int dryrun);
void invalid_opt();
int  parsearg_global(const int opt);
int  parsearg_query(const int opt);
int  parsearg_sync(const int opt);
int  parsearg_remove(const int opt);
#endif
