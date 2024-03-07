#ifndef ARGS_HPP
#define ARGS_HPP

#include <string>
#include <sys/types.h>
#include <vector>
#include <getopt.h>

enum OperationType {
    OP_SYNC = 1, // when you run taur -S[args]
	OP_SYSUPGRADE,
    OP_PACMAN // when it's different from -S, we gonna use pacman
};

struct Operation_t {
    u_short op;
    u_short op_sync;
    u_short op_upgrade;

    std::vector<std::string> args;
};

extern struct Operation_t operation;

int parseargs(int argc, char* argv[]);
int parsearg_op(int opt);

#endif
