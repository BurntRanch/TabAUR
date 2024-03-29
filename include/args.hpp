#ifndef ARGS_HPP
#define ARGS_HPP

#include <alpm_list.h>
#include <string>
#include <sys/types.h>
#include <vector>
#include <getopt.h>

enum {
    OP_MAIN  = 1,
    OP_SYNC, // when you run taur -S[args]
    OP_REM,       // when you run taur -R[args]
    OP_QUERY,
    OP_PACMAN, // when it's different from -S,R,Q we gonna use pacman
};

enum {
    OP_ASK = 100,
    OP_CACHEDIR,
    OP_AURONLY,
    OP_REFRESH,
    OP_SYSUPGRADE,
    OP_NOCOLOR,
    OP_COLOR,
    OP_DEBUG
};

struct Operation_t {
    u_short op;
    u_short op_s_sync;
    u_short op_s_upgrade;
    u_short op_s_pacman;

    bool    requires_root = false;
    u_short help;
    u_short version;

    std::vector<std::string> args;
};

extern struct Operation_t op;
extern alpm_list_t *taur_targets;

int parsearg_op(int opt, int dryrun);
void invalid_opt();
int parsearg_global(int opt);
int parsearg_sync(int opt);
#endif
