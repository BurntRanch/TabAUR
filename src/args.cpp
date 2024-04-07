// Why re-invent the wheel when there is already one working well?
// All credits goes to the pacman devs
#include "args.hpp"
#include "util.hpp"
#include "config.hpp"

Operation_t op;

alpm_list_smart_deleter taur_targets(nullptr, free_list_and_internals);

void invalid_opt(int used, string opt1, string opt2) {
    if (used)
        log_printf(LOG_ERROR, "invalid option: '{}' and '{}' may not be used together", opt1, opt2);
}

/** Helper function for parsing operation from command-line arguments.
 * @param opt Keycode returned by getopt_long
 * @param dryrun If nonzero, application state is NOT changed
 * @return 0 if opt was handled, 1 if it was not handled
 */
int parsearg_op(int opt, int dryrun) {
    switch (opt) {
        /* operations */
        case 'D':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_PACMAN); break;
        case 'F':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_PACMAN); break;
        case 'Q':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_QUERY); break;
        case 'R':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_REM);
            op.requires_root = true;
            break;
        case 'S':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_SYNC); break;
        case 'T':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_PACMAN); break;
        case 'U':
            if(dryrun) break;
            op.op = (op.op != OP_MAIN ? 0 : OP_PACMAN); break;
        case 'V':
            if(dryrun) break;
            op.version = 1; break;
        case 'h':
            if(dryrun) break;
            op.help = 1; break;
        case 't':
            if(dryrun) break;
            op.test_colors = 1; break;
        default:
            return 1;
    }
    return 0;
}

/** Helper functions for parsing command-line arguments.
 * @param opt Keycode returned by getopt_long
 * @return 0 on success, 1 on unkown option, 2 on invalid argument
 */
int parsearg_global(int opt) {
    switch (opt) {
        case OP_CACHEDIR:
            config->cacheDir = strdup(optarg); break;
        case OP_NOCOLOR:
            config->colors = false; break;
        case OP_COLOR:
            config->colors = true; break;
        case OP_DEBUG:
            config->debug = true; break;
        case OP_AURONLY:
        case 'a':
            config->aurOnly = true; break;
        case OP_SUDO:
            config->sudo = strdup(optarg); break;
        case OP_USEGIT:
        case 'g':
            config->useGit = true; break;
        default:
            return 1;
    }
    return 0;
}

int parsearg_sync(int opt) {
    switch (opt) {
        case OP_SYSUPGRADE:
        case 'u':
            (op.op_s_upgrade)++; break;
        case OP_REFRESH:
        case 'y':
            (op.op_s_sync)++; break;
        case OP_SEARCH:
        case 's':
            op.op_s_search = 1; break;
        default:
            return 1;
    }
    return 0;
}
