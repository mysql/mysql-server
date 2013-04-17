#ifndef TOKURECOVER_H
#define TOKURECOVER_H

#ident "$Id: recover.h 13182 2009-07-09 20:57:11Z dwells $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include "../include/db.h"
#include "brttypes.h"
#include "memory.h"
#include "bread.h"
#include "x1764.h"

// Run tokudb recovery from the log
// Returns 0 if success
int tokudb_recover(const char *envdir, const char *logdir, brt_compare_func bt_compare, brt_compare_func dup_compare);

// Effect: Check the tokudb logs to determine whether or not we need to run recovery.
// If the log is empty or if there is a clean shutdown at the end of the log, then we
// dont need to run recovery.
// Returns: TRUE if we need recovery, otherwise FALSE.
int tokudb_needs_recovery(const char *logdir, BOOL ignore_empty_log);

// Delete the rolltmp files
// Ruturns 0 if success
int tokudb_recover_delete_rolltmp_files(const char *datadir, const char *logdir);

// Return 0 if recovery log exists, ENOENT if log is missing
int tokudb_recover_log_exists(const char * log_dir);


extern int tokudb_recovery_trace;

#endif // TOKURECOVER_H
