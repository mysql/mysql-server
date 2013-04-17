#ifndef TOKURECOVER_H
#define TOKURECOVER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include "../include/db.h"
#include "brttypes.h"
#include "memory.h"
#include "x1764.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// Run tokudb recovery from the log
// Returns 0 if success
int tokudb_recover (const char *env_dir, const char *log_dir,
                    brt_compare_func bt_compare,
                    brt_compare_func dup_compare,
                    generate_row_for_put_func       generate_row_for_put,
                    generate_row_for_del_func       generate_row_for_del,
                    size_t cachetable_size);

// Effect: Check the tokudb logs to determine whether or not we need to run recovery.
// If the log is empty or if there is a clean shutdown at the end of the log, then we
// dont need to run recovery.
// Returns: TRUE if we need recovery, otherwise FALSE.
int tokudb_needs_recovery(const char *logdir, BOOL ignore_empty_log);

// Return 0 if recovery log exists, ENOENT if log is missing
int tokudb_recover_log_exists(const char * log_dir);

// For test only - set callbacks for recovery testing
void toku_recover_set_callback (void (*)(void*), void*);
void toku_recover_set_callback2 (void (*)(void*), void*);

extern int tokudb_recovery_trace;

int toku_recover_lock (const char *lock_dir, int *lockfd);

int toku_recover_unlock(int lockfd);


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif // TOKURECOVER_H
