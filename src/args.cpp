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

// Why re-invent the wheel when there is already one working well?
// All credits goes to the pacman devs

// clang-format off
#include "args.hpp"

#include <alpm.h>

#include "config.hpp"
#include "util.hpp"

alpm_list_smart_deleter taur_targets(nullptr, free_list_and_internals);

void invalid_opt(int used, string_view opt1, string_view opt2)
{
    if (used)
        log_println(ERROR, _("invalid option: '{}' and '{}' may not be used together"), opt1, opt2);
}

/** Helper function for parsing operation from command-line arguments.
 * @param opt Keycode returned by getopt_long
 * @param dryrun If nonzero, application state is NOT changed
 * @return 0 if opt was handled, 1 if it was not handled
 */
int parsearg_op(int opt, int dryrun)
{
    switch (opt)
    {
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
                op.op = (op.op != OP_MAIN ? 0 : OP_UPGRADE);
                op.requires_root = true;
                break;
        case 'V':
                if(dryrun) break;
                op.version = 1; break;
        case 'h':
                if(dryrun) break;
                op.help = 1; break;
        case 't':
                if(dryrun) break;
                op.test_colors = 1; break;
        case 'r':
                if(dryrun) break;
                op.show_recipe = 1; break;
        default:
                return 1;
    }
    return 0;
}

/** Helper functions for parsing command-line arguments.
 * @param opt Keycode returned by getopt_long
 * @return 0 on success, 1 on unkown option, 2 on invalid argument
 */
int parsearg_global(int opt)
{
    switch (opt)
    {
        case OP_CACHEDIR:
                config->cacheDir = strndup(optarg, PATH_MAX);
                break;
        
        case OP_COLORS:
                fmt::disable_colors = !((bool)std::atoi(optarg));
                config->colors = (bool)std::atoi(optarg);
                break;
        
        case OP_DEBUG:
                config->debug = true;
                break;
    
        case OP_AURONLY:
        case 'a':
                config->aurOnly = true;
                break;
        
        case OP_SUDO:
                config->sudo = strndup(optarg, PATH_MAX);
                break;
        
        case OP_NOCONFIRM:
                config->noconfirm = true;
                break;
        
        case OP_USEGIT:
        case 'g':
                config->useGit = true;
                break;
        
        case OP_CONFIG:
        case OP_THEME:
                break;
        
        default:
                return 1;
    }
    return 0;
}

int parsearg_query(int opt)
{
    switch(opt)
    {
        case OP_QUIET:
        case 'q':
            config->quiet = true;
            break;
    
        case OP_SEARCH:
        case 's':
            op.op_q_search = 1;
            break;
        
        case OP_INFO:
        case 'i':
            op.op_q_info = 1;
            break;
        
        default:
            return 1;
    }
    return 0;
}

int parsearg_sync(int opt)
{
    switch (opt)
    {
        case OP_SYSUPGRADE:
        case 'u':
            op.op_s_upgrade = 1; 
            break;
    
        case OP_REFRESH:
        case 'y':
            op.op_s_sync = 1; 
            break;
        
        case OP_SEARCH:
        case 's':
            op.op_s_search = 1; 
            break;
        
        case OP_CLEANBUILD:
            op.op_s_cleanbuild = 1;
            break;
        
        default:
            return 1;
    }
    return 0;
}

int parsearg_remove(int opt)
{
    switch (opt)
    {
        case OP_NOSAVE:
        case 'n':
            config->flags |= ALPM_TRANS_FLAG_NOSAVE;
            break;
    
        case OP_RECURSIVE:
        case 's':
            if (config->flags & ALPM_TRANS_FLAG_RECURSE)
                config->flags |= ALPM_TRANS_FLAG_RECURSEALL;
            else
                config->flags |= ALPM_TRANS_FLAG_RECURSE;
            break;
        
        default: return 1;
    }
    return 0;
}
