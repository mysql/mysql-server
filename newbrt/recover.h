#ifndef TOKURECOVER_H
#define TOKURECOVER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include <db.h>
#include "brttypes.h"
#include "memory.h"
#include "x1764.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

typedef void (*prepared_txn_callback_t)(DB_ENV*, TOKUTXN);
typedef void (*keep_zombie_callback_t)(DB_ENV*, BRT, char *iname, bool oplsn_valid, LSN oplsn);
typedef void (*keep_cachetable_callback_t)(DB_ENV*, CACHETABLE);
typedef int (*setup_db_callback_t)(DB **, DB_ENV *, u_int32_t db_create_flags, BRT, bool /*is_open*/)   __attribute__ ((warn_unused_result));
typedef int (*close_db_callback_t)(DB *, bool oplsn_valid, LSN oplsn) __attribute__ ((warn_unused_result));

// Run tokudb recovery from the log
// Returns 0 if success
int tokudb_recover (DB_ENV *env,
		    keep_zombie_callback_t     keep_zombie_callback,
		    prepared_txn_callback_t    prepared_txn_callback,
		    keep_cachetable_callback_t keep_cachetable_callback,
		    setup_db_callback_t        setup_db_callback,
		    close_db_callback_t        close_db_callback,
		    TOKULOGGER logger,
		    const char *env_dir, const char *log_dir,
                    brt_compare_func bt_compare,
                    brt_update_func update_function,
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

static const prepared_txn_callback_t NULL_prepared_txn_callback         __attribute__((__unused__)) = NULL;
static const keep_zombie_callback_t  NULL_keep_zombie_callback          __attribute__((__unused__)) = NULL;
static const keep_cachetable_callback_t  NULL_keep_cachetable_callback  __attribute__((__unused__)) = NULL;
static const setup_db_callback_t NULL_setup_db_callback                 __attribute__((__unused__)) = NULL;
static const close_db_callback_t NULL_close_db_callback                 __attribute__((__unused__)) = NULL;


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif // TOKURECOVER_H
