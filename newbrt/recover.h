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
// Returns: 0 if successfull
int tokudb_recover(const char *datadir, const char *logdir, brt_compare_func bt_compare, brt_compare_func dup_compare);

// Effect: Check the tokudb logs to determine whether or not we need to run recovery.
// If the log is empty or if there is a clean shutdown at the end of the log, then we
// dont need to run recovery.
// Returns: TRUE if we need recovery, otherwise FALSE.
int tokudb_needs_recovery(const char *logdir, BOOL ignore_empty_log);

#endif // TOKURECOVER_H
