/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
 
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

const char *toku_patent_string = "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it.";
const char *toku_copyright_string = "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved.";

#include "toku_portability.h"
#include <toku_pthread.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <db.h>
#include "toku_assert.h"
#include "ydb.h"
#include "ydb-internal.h"
#include "brt-internal.h"
#include "cachetable.h"
#include "log.h"
#include "memory.h"
#include "dlmalloc.h"
#include "checkpoint.h"
#include "key.h"

#ifdef TOKUTRACE
 #define DB_ENV_CREATE_FUN db_env_create_toku10
 #define DB_CREATE_FUN db_create_toku10
#else
 #define DB_ENV_CREATE_FUN db_env_create
 #define DB_CREATE_FUN db_create
 int toku_set_trace_file (char *fname __attribute__((__unused__))) { return 0; }
 int toku_close_trace_file (void) { return 0; } 
#endif

/** The default maximum number of persistent locks in a lock tree  */
const u_int32_t __toku_env_default_max_locks = 1000;

static inline DBT*
init_dbt_realloc(DBT *dbt) {
    memset(dbt, 0, sizeof(*dbt));
    dbt->flags = DB_DBT_REALLOC;
    return dbt;
}

static void
toku_ydb_init_malloc(void) {
#if defined(TOKU_WINDOWS) && TOKU_WINDOWS
    //Set the heap (malloc/free/realloc) to use the low fragmentation mode.
    ULONG  HeapFragValue = 2;

    int r;
    r = HeapSetInformation(GetProcessHeap(),
                           HeapCompatibilityInformation,
                           &HeapFragValue,
                           sizeof(HeapFragValue));
    //if (r==0) //Do some error output if necessary.
    assert(r!=0);
#endif
}

void toku_ydb_init(void) {
    toku_ydb_init_malloc();
    toku_brt_init(toku_ydb_lock, toku_ydb_unlock);
    toku_ydb_lock_init();
}

void toku_ydb_destroy(void) {
    toku_brt_destroy();
    toku_ydb_lock_destroy();
}

static int
ydb_getf_do_nothing(DBT const* UU(key), DBT const* UU(val), void* UU(extra)) {
    return 0;
}

/* the ydb reference is used to cleanup the library when there are no more references to it */
static int toku_ydb_refs = 0;

static inline void ydb_add_ref() {
    ++toku_ydb_refs;
}

static inline void ydb_unref() {
    assert(toku_ydb_refs > 0);
    if (--toku_ydb_refs == 0) {
        /* call global destructors */
        toku_malloc_cleanup();
    }
}

/* env methods */
static int toku_env_close(DB_ENV *env, u_int32_t flags);
static int toku_env_set_data_dir(DB_ENV * env, const char *dir);
static int toku_env_set_lg_dir(DB_ENV * env, const char *dir);
static int toku_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir);

static inline void env_add_ref(DB_ENV *env) {
    ++env->i->ref_count;
}

static inline void env_unref(DB_ENV *env) {
    assert(env->i->ref_count > 0);
    if (--env->i->ref_count == 0)
        toku_env_close(env, 0);
}

static inline int env_opened(DB_ENV *env) {
    return env->i->cachetable != 0;
}


/* db methods */
static inline int db_opened(DB *db) {
    return db->i->full_fname != 0;
}

static int toku_db_put(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags);
static int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags);
static int toku_db_cursor(DB *db, DB_TXN * txn, DBC **c, u_int32_t flags, int is_temporary_cursor);

/* txn methods */

/* lightweight cursor methods. */
static int toku_c_getf_first(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_last(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_next(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_next_nodup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_next_dup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_prev(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_prev_nodup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_prev_dup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_current(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_current_binding(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_set(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_set_range(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_get_both(DBC *c, u_int32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra);
static int toku_c_getf_get_both_range(DBC *c, u_int32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra);

static int toku_c_getf_heaviside(DBC *c, u_int32_t flags,
                                 YDB_HEAVISIDE_CALLBACK_FUNCTION f, void *extra_f,
                                 YDB_HEAVISIDE_FUNCTION h, void *extra_h, int direction); 
// There is a total order on all key/value pairs in the database.
// In a DB_DUPSORT db, let V_i = (Key,Value) refer to the ith element (0 based indexing).
// In a NODUP      db, let V_i = (Key)       refer to the ith element (0 based indexing).
// We define V_{-1}             = -\infty and
//           V_{|V|}            =  \infty and
//           h(-\infty,extra_h) = -1 by definition and
//           h( \infty,extra_h) =  1 by definition
// Requires: Direction != 0
// Effect: 
//    if direction >0 then find the smallest i such that h(V_i,extra_h)>=0.
//    if direction <0 then find the largest  i such that h(V_i,extra_h)<=0.
//    Let signus(r_h) = signus(h(V_i, extra_h)) 
//    If flags&(DB_PRELOCKED|DB_PRELOCKED_WRITE) then skip locking
//      That is, we already own the locks
//    else 
//      if direction >0 then readlock [V_{i-1}, V_i]
//      if direction <0 then readlock [V_i,     V_{i+1}]
//      That is, If we search from the right, lock the element we found, up to the
//           next element to the right.
//      If locking fails, return the locking error code
//    
//    If (0<=i<|V|) then
//      call f(V_i.Key, V_i.Value, extra_f, r_h)
//      Note: The lifetime of V_i.Key and V_i.Value is limited: they may only
//            be referenced until f returns
//      and return 0
//    else
//      return DB_NOTFOUND
// Rationale: Locking
//      If we approach from the left (direction<0) we need to prevent anyone
//      from inserting anything to our right that could change our answer,
//      so we lock the range from the element found, to the next element to the right.
//      The inverse argument applies for approaching from the right.
// Rationale: passing r_h to f
//      We want to save the performance hit of requiring f to call h again to
//      find out what h's return value was.
// Rationale: separate extra_f, extra_h parameters
//      If the same extra parameter is sent to both f and h, then you need a
//      special struct for each tuple (f_i, h_i) you use instead of a struct for each
//      f_i and each h_i.
// Requires: The signum of h is monotically increasing.
//  Requires: f does not create references to key, value, or data within once f
//           exits
// Returns
//      0                   success
//      DB_NOTFOUND         i is not in [0,|V|)
//      DB_LOCK_NOTGRANTED  Failed to obtain a lock.
//  On nonzero return, what c points to becomes undefined, That is, c becomes uninitialized
// Performance: ... TODO
// Implementation Notes:
//      How do we get the extra locking information efficiently?
//        After finding the target, we can copy the cursor, do a DB_NEXT,
//        or do a DB_NEXT+DB_PREV (vice versa for direction<0).
//        Can we have the BRT provide two key/value pairs instead of one?
//        That is, brt_cursor_c_getf_heavi_and_next for direction >0
//        and  brt_cursor_c_getf_heavi_and_prev for direction <0
//      Current suggestion is to make a copy of the cursor, and use the
//        copy to find the next(prev) element by using DB_NEXT(DB_PREV).
//        This has the overhead of needing to make a copy of the cursor,
//        which probably has a memcpy involved.
//        The argument against returning two key/value pairs is that
//        we should not have to pay to retreive both when we're doing something
//        simple like DB_NEXT.
//        This could be mitigated by having two BRT functions (or one with a
//        BOOL parameter) such that it only returns two values when necessary.
// Parameters
//  c           The cursor
//  flags       Additional bool parameters. The current allowed flags are
//              DB_PRELOCKED and DB_PRELOCKED_WRITE (bitwise or'd to use both)
//  h           A heaviside function that, along with direction, defines the query.
//              extra_h is passed to h
//              For additional information on heaviside functions, see omt.h
//              NOTE: In a DB_DUPSORT database, both key and value will be
//              passed to h.  In a NODUP database, only key will be passed to h.
//  f           A callback function (i.e. smart dbts) to provide the result of the
//              query.  key and value are the key/value pair found, extra_f is
//              passed to f, r_h is the return value for h for the key and value returned.
//              This is used as output. That is, we call f with the outputs of the
//              function.
//  direction   Which direction to search in on the heaviside function.  >0
//              means from the right, <0 means from the left.
//  extra_f     Any extra information required for f
//  extra_h     Any extra information required for h
//
// Example:
//  Find the smallest V_i = (key_i,val_i) such that key_i > key_x, assume
//   key.data and val.data are c strings, and print them out.
//      Create a struct to hold key_x, that is extra_h
//      Direction = 1 (We approach from the right, and want the smallest such
//          element).
//      Construct a heaviside function that returns >=0 if the
//      given key > key_x, and -1 otherwise
//          That is, call the comparison function on (key, key_x)
//      Create a struct to hold key_x, that is extra_f
//      construct f to call printf on key_x.data, key_i.data, val_i.data.
//  Find the least upper bound (greatest lower bound)
//      In this case, h can just return the comparison function's answer.
//      direction >0 means upper bound, direction <0 means lower bound.
//      (If you want upper/lower bound of the keyvalue pair, you need
//      to call the comparison function on the values if the key comparison
//      returns 0).
// Handlerton implications:
//  The handlerton needs at most one heaviside function per special query type (where a
//  special query is one that is not directly supported by the bdb api excluding
//  this function).
//  It is possible that more than query type can use the same heaviside function
//  if the extra_h parameter can be used to change its behavior sufficiently.
//
//  That is, part of extra_h can be a boolean strictly_greater
//  You can construct a single heaviside function that converts 0 to -1
//  (strictly greater) from the comparison function, or one that just returns
//  the results of the comparison function (greater or equal).
//
// Implementation Notes:
//  The BRT search function supports the following searches:
//      SEARCH_LEFT(h(V_i))
//          Given a step function b, that goes from 0 to 1
//          find the greatest i such that h_b(V_i) == 1
//          If it does not exist, return not found
//      SEARCH_RIGHT(h(V_i))
//          Given a step function b, that goes from 1 to 0
//          find the smallest i such that h_b(V_i) == 1
//          If it does not exist, return not found
//  We can implement c_getf_heavi using these BRT search functions.
//  A query of direction<0:
//      Create wrapper function B
//          return h(V_i) <=0 ? 1 : 0;
//      SEARCH_RIGHT(B)
//  A query of direction>0:
//      Create wrapper function B
//          return h(V_i) >=0 ? 1 : 0;
//      SEARCH_LEFT(B)

// Effect: Lightweight cursor get

/* cursor methods */
static int toku_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag);
static int toku_c_del(DBC *c, u_int32_t flags);
static int toku_c_count(DBC *cursor, db_recno_t *count, u_int32_t flags);
static int toku_c_close(DBC * c);

/* misc */
static char *construct_full_name(const char *dir, const char *fname);
    
static int do_recovery (DB_ENV *env) {
    const char *datadir=env->i->dir;
    char *logdir;
    if (env->i->lg_dir) {
	logdir = construct_full_name(env->i->dir, env->i->lg_dir);
    } else {
	logdir = toku_strdup(env->i->dir);
    }
    
#if 0
    // want to do recovery in its own process
    pid_t pid;
    if ((pid=fork())==0) {
	int r=tokudb_recover(datadir, logdir);
	assert(r==0);
	toku_free(logdir); // the child must also free.
	exit(0);
    }
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)!=0)  {
	toku_free(logdir);
	return toku_ydb_do_error(env, -1, "Recovery failed\n");
    }
    toku_free(logdir);
    return 0;
#else
    int r = tokudb_recover(datadir, logdir);
    toku_free(logdir);
    return r;
#endif
}

static int toku_env_open(DB_ENV * env, const char *home, u_int32_t flags, int mode) {
    HANDLE_PANICKED_ENV(env);
    int r;
    u_int32_t unused_flags=flags;

    if (env_opened(env)) {
	return toku_ydb_do_error(env, EINVAL, "The environment is already open\n");
    }

    if ((flags & DB_USE_ENVIRON) && (flags & DB_USE_ENVIRON_ROOT)) {
	return toku_ydb_do_error(env, EINVAL, "DB_USE_ENVIRON and DB_USE_ENVIRON_ROOT are incompatible flags\n");
    }

    if (home) {
        if ((flags & DB_USE_ENVIRON) || (flags & DB_USE_ENVIRON_ROOT)) {
	    return toku_ydb_do_error(env, EINVAL, "DB_USE_ENVIRON and DB_USE_ENVIRON_ROOT are incompatible with specifying a home\n");
	}
    }
    else if ((flags & DB_USE_ENVIRON)) home = getenv("DB_HOME");
#if !TOKU_WINDOWS
    else if ((flags & DB_USE_ENVIRON_ROOT) && geteuid() == 0) home = getenv("DB_HOME");
#endif
    unused_flags &= ~DB_USE_ENVIRON & ~DB_USE_ENVIRON_ROOT; 

    if (!home) home = ".";

	// Verify that the home exists.
	{
	    BOOL made_new_home = FALSE;
        char* new_home = NULL;
    	toku_struct_stat buf;
        if (home[strlen(home)-1] == '\\') {
            new_home = toku_malloc(strlen(home));
            memcpy(new_home, home, strlen(home));
            new_home[strlen(home) - 1] = 0;
            made_new_home = TRUE;
        }
    	r = toku_stat(made_new_home? new_home : home, &buf);
        if (made_new_home) {
            toku_free(new_home);
        }
    	if (r!=0) {
    	    return toku_ydb_do_error(env, errno, "Error from toku_stat(\"%s\",...)\n", home);
    	}
    }
    unused_flags &= ~DB_PRIVATE;

    if (env->i->dir)
        toku_free(env->i->dir);
    env->i->dir = toku_strdup(home);
    if (env->i->dir == 0) {
	return toku_ydb_do_error(env, ENOMEM, "Out of memory\n");
    }
    if (0) {
        died1:
        toku_free(env->i->dir);
        env->i->dir = NULL;
        return r;
    }
    env->i->open_flags = flags;
    env->i->open_mode = mode;

    unused_flags &= ~DB_INIT_TXN & ~DB_INIT_LOG; 

    if (flags&DB_RECOVER) {
	r=do_recovery(env);
	if (r!=0) return r;
    }

    if (flags & (DB_INIT_TXN | DB_INIT_LOG)) {
        char* full_dir = NULL;
        if (env->i->lg_dir) full_dir = construct_full_name(env->i->dir, env->i->lg_dir);
	assert(env->i->logger);
        toku_logger_write_log_files(env->i->logger, (flags & DB_INIT_LOG) != 0);
        r = toku_logger_open(full_dir ? full_dir : env->i->dir, env->i->logger);
        if (full_dir) toku_free(full_dir);
	if (r!=0) {
	    toku_ydb_do_error(env, r, "Could not open logger\n");
	died2:
	    toku_logger_close(&env->i->logger);
	    goto died1;
	}
    } else {
	r = toku_logger_close(&env->i->logger); // if no logging system, then kill the logger
	assert(r==0);
    }

    unused_flags &= ~DB_INIT_MPOOL; // we always init an mpool.
    unused_flags &= ~DB_CREATE;     // we always do DB_CREATE
    unused_flags &= ~DB_INIT_LOCK;  // we check this later (e.g. in db->open)
    unused_flags &= ~DB_RECOVER;

// This is probably correct, but it will be pain...
//    if ((flags & DB_THREAD)==0) {
//	return toku_ydb_do_error(env, EINVAL, "TokuDB requires DB_THREAD");
//    }
    unused_flags &= ~DB_THREAD;

    if (unused_flags!=0) {
	return toku_ydb_do_error(env, EINVAL, "Extra flags not understood by tokudb: %u\n", unused_flags);
    }

    r = toku_brt_create_cachetable(&env->i->cachetable, env->i->cachetable_size, ZERO_LSN, env->i->logger);
    if (r!=0) goto died2;

    if (env->i->logger) toku_logger_set_cachetable(env->i->logger, env->i->cachetable);

    return 0;
}

static int toku_env_close(DB_ENV * env, u_int32_t flags) {
    int is_panicked = toku_env_is_panicked(env);
    char *panic_string = env->i->panic_string;
    env->i->panic_string = 0;

    // Even if the env is panicked, try to close as much as we can.
    int r0=0,r1=0;
    if (env->i->cachetable) {
	toku_ydb_unlock();  // ydb lock must not be held when shutting down minicron
	toku_cachetable_minicron_shutdown(env->i->cachetable);
	toku_ydb_lock();
        r0=toku_cachetable_close(&env->i->cachetable);
	if (r0) {
	    toku_ydb_do_error(env, r0, "Cannot close environment (cachetable close error)\n");
	}
    }
    if (env->i->logger) {
        r1=toku_logger_close(&env->i->logger);
	if (r0==0 && r1) {
	    toku_ydb_do_error(env, r0, "Cannot close environment (logger close error)\n");
	}
    }
    // Even if nothing else went wrong, but we were panicked, then raise an error.
    // But if something else went wrong then raise that error (above)
    if (is_panicked) {
	if (r0==0 && r1==0) {
	    toku_ydb_do_error(env, is_panicked, "Cannot close environment due to previous error: %s\n", panic_string);
	}
	if (panic_string) toku_free(panic_string);
    } else {
	assert(panic_string==0);
    }

    if (env->i->data_dirs) {
        u_int32_t i;
        assert(env->i->n_data_dirs > 0);
        for (i = 0; i < env->i->n_data_dirs; i++) {
            toku_free(env->i->data_dirs[i]);
        }
        toku_free(env->i->data_dirs);
    }
    if (env->i->lg_dir)
        toku_free(env->i->lg_dir);
    if (env->i->tmp_dir)
        toku_free(env->i->tmp_dir);
    toku_free(env->i->dir);
    toku_ltm_close(env->i->ltm);
    toku_free(env->i);
    toku_free(env);
    ydb_unref();
    if (flags!=0) return EINVAL;
    if (r0) return r0;
    if (r1) return r1;
    return is_panicked;
}

static int toku_env_log_archive(DB_ENV * env, char **list[], u_int32_t flags) {
    return toku_logger_log_archive(env->i->logger, list, flags);
}

static int toku_env_log_flush(DB_ENV * env, const DB_LSN * lsn __attribute__((__unused__))) {
    HANDLE_PANICKED_ENV(env);
    // We just flush everything.  MySQL uses lsn==0 which means flush everything.  For anyone else using the log, it is correct to flush too much, so we are OK.
    return toku_logger_fsync(env->i->logger);
}

static int toku_env_set_cachesize(DB_ENV * env, u_int32_t gbytes, u_int32_t bytes, int ncache) {
    HANDLE_PANICKED_ENV(env);
    if (ncache != 1)
        return EINVAL;
    u_int64_t cs64 = ((u_int64_t) gbytes << 30) + bytes;
    unsigned long cs = cs64;
    if (cs64 > cs)
        return EINVAL;
    env->i->cachetable_size = cs;
    return 0;
}

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3

static int toku_env_get_cachesize(DB_ENV * env, u_int32_t *gbytes, u_int32_t *bytes, int *ncache) {
    HANDLE_PANICKED_ENV(env);
    *gbytes = env->i->cachetable_size >> 30;
    *bytes = env->i->cachetable_size & ((1<<30)-1);
    *ncache = 1;
    return 0;
}

static int locked_env_get_cachesize(DB_ENV *env, u_int32_t *gbytes, u_int32_t *bytes, int *ncache) {
    toku_ydb_lock(); int r = toku_env_get_cachesize(env, gbytes, bytes, ncache); toku_ydb_unlock(); return r;
}
#endif

static int toku_env_set_data_dir(DB_ENV * env, const char *dir) {
    HANDLE_PANICKED_ENV(env);
    u_int32_t i;
    int r;
    char** temp;
    char* new_dir;
    
    if (env_opened(env) || !dir) {
	return toku_ydb_do_error(env, EINVAL, "You cannot set the data dir after opening the env\n");
    }
    
    if (env->i->data_dirs) {
        assert(env->i->n_data_dirs > 0);
        for (i = 0; i < env->i->n_data_dirs; i++) {
            if (!strcmp(dir, env->i->data_dirs[i])) {
                //It is already in the list.  We're done.
                return 0;
            }
        }
    }
    else assert(env->i->n_data_dirs == 0);
    new_dir = toku_strdup(dir);
    if (0) {
        died1:
        toku_free(new_dir);
        return r;
    }
    if (new_dir==NULL) {
	assert(errno == ENOMEM);
	return toku_ydb_do_error(env, errno, "Out of memory\n");
    }
    temp = (char**) toku_realloc(env->i->data_dirs, (1 + env->i->n_data_dirs) * sizeof(char*));
    if (temp==NULL) {assert(errno == ENOMEM); r = ENOMEM; goto died1;}
    else env->i->data_dirs = temp;
    env->i->data_dirs[env->i->n_data_dirs] = new_dir;
    env->i->n_data_dirs++;
    return 0;
}

static void toku_env_set_errcall(DB_ENV * env, toku_env_errcall_t errcall) {
    env->i->errcall = errcall;
}

static void toku_env_set_errfile(DB_ENV*env, FILE*errfile) {
    env->i->errfile = errfile;
}

static void toku_env_set_errpfx(DB_ENV * env, const char *errpfx) {
    env->i->errpfx = errpfx;
}

static int toku_env_set_flags(DB_ENV * env, u_int32_t flags, int onoff) {
    HANDLE_PANICKED_ENV(env);

    u_int32_t change = 0;
    if (flags & DB_AUTO_COMMIT) {
        change |=  DB_AUTO_COMMIT;
        flags  &= ~DB_AUTO_COMMIT;
    }
    if (flags != 0 && onoff) {
	return toku_ydb_do_error(env, EINVAL, "TokuDB does not (yet) support any nonzero ENV flags other than DB_AUTO_COMMIT\n");
    }
    if   (onoff) env->i->open_flags |=  change;
    else         env->i->open_flags &= ~change;
    return 0;
}

static int toku_env_set_lg_bsize(DB_ENV * env, u_int32_t bsize) {
    HANDLE_PANICKED_ENV(env);
    return toku_logger_set_lg_bsize(env->i->logger, bsize);
}

static int toku_env_set_lg_dir(DB_ENV * env, const char *dir) {
    HANDLE_PANICKED_ENV(env);
    if (env_opened(env)) {
	return toku_ydb_do_error(env, EINVAL, "Cannot set log dir after opening the env\n");
    }

    if (env->i->lg_dir) toku_free(env->i->lg_dir);
    if (dir) {
        env->i->lg_dir = toku_strdup(dir);
        if (!env->i->lg_dir) {
	    return toku_ydb_do_error(env, ENOMEM, "Out of memory\n");
	}
    }
    else env->i->lg_dir = NULL;
    return 0;
}

static int toku_env_set_lg_max(DB_ENV * env, u_int32_t lg_max) {
    HANDLE_PANICKED_ENV(env);
    return toku_logger_set_lg_max(env->i->logger, lg_max);
}

static int toku_env_get_lg_max(DB_ENV * env, u_int32_t *lg_maxp) {
    HANDLE_PANICKED_ENV(env);
    return toku_logger_get_lg_max(env->i->logger, lg_maxp);
}

static int toku_env_set_lk_detect(DB_ENV * env, u_int32_t detect) {
    HANDLE_PANICKED_ENV(env);
    detect=detect;
    return toku_ydb_do_error(env, EINVAL, "TokuDB does not (yet) support set_lk_detect\n");
}

static int toku_env_set_lk_max_locks(DB_ENV *dbenv, u_int32_t max) {
    int r = ENOSYS;
    HANDLE_PANICKED_ENV(dbenv);
    if (env_opened(dbenv))         { return EINVAL; }
    r = toku_ltm_set_max_locks_per_db(dbenv->i->ltm, max);
    return r;
}

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
static int toku_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    return toku_env_set_lk_max_locks(env, lk_max);
}

static int locked_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    toku_ydb_lock(); int r = toku_env_set_lk_max(env, lk_max); toku_ydb_unlock(); return r;
}
#endif

static int toku_env_get_lk_max_locks(DB_ENV *dbenv, u_int32_t *lk_maxp) {
    HANDLE_PANICKED_ENV(dbenv);
    return toku_ltm_get_max_locks_per_db(dbenv->i->ltm, lk_maxp);
}

static int locked_env_set_lk_max_locks(DB_ENV *dbenv, u_int32_t max) {
    toku_ydb_lock(); int r = toku_env_set_lk_max_locks(dbenv, max); toku_ydb_unlock(); return r;
}

static int __attribute__((unused)) locked_env_get_lk_max_locks(DB_ENV *dbenv, u_int32_t *lk_maxp) {
    toku_ydb_lock(); int r = toku_env_get_lk_max_locks(dbenv, lk_maxp); toku_ydb_unlock(); return r;
}

//void toku__env_set_noticecall (DB_ENV *env, void (*noticecall)(DB_ENV *, db_notices)) {
//    env->i->noticecall = noticecall;
//}

static int toku_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir) {
    HANDLE_PANICKED_ENV(env);
    if (env_opened(env)) {
	return toku_ydb_do_error(env, EINVAL, "Cannot set the tmp dir after opening an env\n");
    }
    if (!tmp_dir) {
	return toku_ydb_do_error(env, EINVAL, "Tmp dir bust be non-null\n");
    }
    if (env->i->tmp_dir)
        toku_free(env->i->tmp_dir);
    env->i->tmp_dir = toku_strdup(tmp_dir);
    return env->i->tmp_dir ? 0 : ENOMEM;
}

static int toku_env_set_verbose(DB_ENV * env, u_int32_t which, int onoff) {
    HANDLE_PANICKED_ENV(env);
    which=which; onoff=onoff;
    return 1;
}

// For test purposes only.
static void (*checkpoint_callback_f)(void*) = NULL;
static void * checkpoint_callback_extra     = NULL;

static int toku_env_txn_checkpoint(DB_ENV * env, u_int32_t kbyte __attribute__((__unused__)), u_int32_t min __attribute__((__unused__)), u_int32_t flags __attribute__((__unused__))) {
    char *error_string = NULL;
    int r = toku_checkpoint(env->i->cachetable, env->i->logger, &error_string, checkpoint_callback_f, checkpoint_callback_extra);
    if (r) {
	env->i->is_panicked = r; // Panicking the whole environment may be overkill, but I'm not sure what else to do.
	env->i->panic_string = error_string;
	if (error_string) {
	    toku_ydb_do_error(env, r, "%s\n", error_string);
	} else {
	    toku_ydb_do_error(env, r, "Checkpoint\n");
	}
	error_string=NULL;
    }
    return r;
}

static int toku_env_txn_stat(DB_ENV * env, DB_TXN_STAT ** statp, u_int32_t flags) {
    HANDLE_PANICKED_ENV(env);
    statp=statp;flags=flags;
    return 1;
}

#if 0
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
static void toku_default_errcall(const char *errpfx, char *msg) {
    fprintf(stderr, "YDB: %s: %s", errpfx, msg);
}
#else
static void toku_default_errcall(const DB_ENV *env, const char *errpfx, const char *msg) {
    env = env;
    fprintf(stderr, "YDB: %s: %s", errpfx, msg);
}
#endif
#endif

static int locked_env_open(DB_ENV * env, const char *home, u_int32_t flags, int mode) {
    toku_ydb_lock(); int r = toku_env_open(env, home, flags, mode); toku_ydb_unlock(); return r;
}

static int locked_env_close(DB_ENV * env, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_close(env, flags); toku_ydb_unlock(); return r;
}

static int locked_env_log_archive(DB_ENV * env, char **list[], u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_log_archive(env, list, flags); toku_ydb_unlock(); return r;
}

static int locked_env_log_flush(DB_ENV * env, const DB_LSN * lsn) {
    toku_ydb_lock(); int r = toku_env_log_flush(env, lsn); toku_ydb_unlock(); return r;
}

static int locked_env_set_cachesize(DB_ENV *env, u_int32_t gbytes, u_int32_t bytes, int ncache) {
    toku_ydb_lock(); int r = toku_env_set_cachesize(env, gbytes, bytes, ncache); toku_ydb_unlock(); return r;
}

static int locked_env_set_data_dir(DB_ENV * env, const char *dir) {
    toku_ydb_lock(); int r = toku_env_set_data_dir(env, dir); toku_ydb_unlock(); return r;
}

static int locked_env_set_flags(DB_ENV * env, u_int32_t flags, int onoff) {
    toku_ydb_lock(); int r = toku_env_set_flags(env, flags, onoff); toku_ydb_unlock(); return r;
}

static int locked_env_set_lg_bsize(DB_ENV * env, u_int32_t bsize) {
    toku_ydb_lock(); int r = toku_env_set_lg_bsize(env, bsize); toku_ydb_unlock(); return r;
}

static int locked_env_set_lg_dir(DB_ENV * env, const char *dir) {
    toku_ydb_lock(); int r = toku_env_set_lg_dir(env, dir); toku_ydb_unlock(); return r;
}

static int locked_env_set_lg_max(DB_ENV * env, u_int32_t lg_max) {
    toku_ydb_lock(); int r = toku_env_set_lg_max(env, lg_max); toku_ydb_unlock(); return r;
}

static int locked_env_get_lg_max(DB_ENV * env, u_int32_t *lg_maxp) {
    toku_ydb_lock(); int r = toku_env_get_lg_max(env, lg_maxp); toku_ydb_unlock(); return r;
}

static int locked_env_set_lk_detect(DB_ENV * env, u_int32_t detect) {
    toku_ydb_lock(); int r = toku_env_set_lk_detect(env, detect); toku_ydb_unlock(); return r;
}

static int locked_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir) {
    toku_ydb_lock(); int r = toku_env_set_tmp_dir(env, tmp_dir); toku_ydb_unlock(); return r;
}

static int locked_env_set_verbose(DB_ENV * env, u_int32_t which, int onoff) {
    toku_ydb_lock(); int r = toku_env_set_verbose(env, which, onoff); toku_ydb_unlock(); return r;
}

static int locked_env_txn_stat(DB_ENV * env, DB_TXN_STAT ** statp, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_txn_stat(env, statp, flags); toku_ydb_unlock(); return r;
}

static int
env_checkpointing_set_period(DB_ENV * env, u_int32_t seconds) {
    HANDLE_PANICKED_ENV(env);
    int r;
    if (!env_opened(env)) r = EINVAL;
    else
        r = toku_set_checkpoint_period(env->i->cachetable, seconds);
    return r;
}

static int
locked_env_checkpointing_set_period(DB_ENV * env, u_int32_t seconds) {
    toku_ydb_lock(); int r = env_checkpointing_set_period(env, seconds); toku_ydb_unlock(); return r;
}

static int
env_checkpointing_get_period(DB_ENV * env, u_int32_t *seconds) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else 
        *seconds = toku_get_checkpoint_period(env->i->cachetable);
    return r;
}

static int
locked_env_checkpointing_get_period(DB_ENV * env, u_int32_t *seconds) {
    toku_ydb_lock(); int r = env_checkpointing_get_period(env, seconds); toku_ydb_unlock(); return r;
}

static int
env_checkpointing_postpone(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_checkpoint_safe_client_lock();
    return r;
}

static int
env_checkpointing_resume(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_checkpoint_safe_client_unlock();
    return r;
}

static int
env_checkpointing_begin_atomic_operation(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_multi_operation_client_lock();
    return r;
}

static int
env_checkpointing_end_atomic_operation(DB_ENV * env) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (!env_opened(env)) r = EINVAL;
    else toku_multi_operation_client_unlock();
    return r;
}

static int
env_set_default_dup_compare(DB_ENV * env, int (*dup_compare) (DB *, const DBT *, const DBT *)) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (env_opened(env)) r = EINVAL;
    else {
        env->i->dup_compare = dup_compare;
    }
    return r;
}

static int
locked_env_set_default_dup_compare(DB_ENV * env, int (*dup_compare) (DB *, const DBT *, const DBT *)) {
    toku_ydb_lock();
    int r = env_set_default_dup_compare(env, dup_compare);
    toku_ydb_unlock();
    return r;
}

static int
env_set_default_bt_compare(DB_ENV * env, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    HANDLE_PANICKED_ENV(env);
    int r = 0;
    if (env_opened(env)) r = EINVAL;
    else {
        env->i->bt_compare = bt_compare;
    }
    return r;
}

static int
locked_env_set_default_bt_compare(DB_ENV * env, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    toku_ydb_lock();
    int r = env_set_default_bt_compare(env, bt_compare);
    toku_ydb_unlock();
    return r;
}

static int locked_txn_begin(DB_ENV * env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags);

static int toku_db_lt_panic(DB* db, int r);

static toku_dbt_cmp toku_db_get_compare_fun(DB* db);

static toku_dbt_cmp toku_db_get_dup_compare(DB* db);

static int toku_env_create(DB_ENV ** envp, u_int32_t flags) {
    int r = ENOSYS;
    DB_ENV* result = NULL;

    if (flags!=0)    { r = EINVAL; goto cleanup; }
    MALLOC(result);
    if (result == 0) { r = ENOMEM; goto cleanup; }
    memset(result, 0, sizeof *result);
    result->err = (void (*)(const DB_ENV * env, int error, const char *fmt, ...)) toku_locked_env_err;
    result->set_default_bt_compare = locked_env_set_default_bt_compare;
    result->set_default_dup_compare = locked_env_set_default_dup_compare;
    result->checkpointing_set_period = locked_env_checkpointing_set_period;
    result->checkpointing_get_period = locked_env_checkpointing_get_period;
    result->checkpointing_postpone = env_checkpointing_postpone;
    result->checkpointing_resume = env_checkpointing_resume;
    result->checkpointing_begin_atomic_operation = env_checkpointing_begin_atomic_operation;
    result->checkpointing_end_atomic_operation = env_checkpointing_end_atomic_operation;
    result->open = locked_env_open;
    result->close = locked_env_close;
    result->txn_checkpoint = toku_env_txn_checkpoint;
    result->log_flush = locked_env_log_flush;
    result->set_errcall = toku_env_set_errcall;
    result->set_errfile = toku_env_set_errfile;
    result->set_errpfx = toku_env_set_errpfx;
    //result->set_noticecall = locked_env_set_noticecall;
    result->set_flags = locked_env_set_flags;
    result->set_data_dir = locked_env_set_data_dir;
    result->set_tmp_dir = locked_env_set_tmp_dir;
    result->set_verbose = locked_env_set_verbose;
    result->set_lg_bsize = locked_env_set_lg_bsize;
    result->set_lg_dir = locked_env_set_lg_dir;
    result->set_lg_max = locked_env_set_lg_max;
    result->get_lg_max = locked_env_get_lg_max;
    result->set_lk_max_locks = locked_env_set_lk_max_locks;
    result->get_lk_max_locks = locked_env_get_lk_max_locks;
    result->set_cachesize = locked_env_set_cachesize;
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
    result->get_cachesize = locked_env_get_cachesize;
#endif
    result->set_lk_detect = locked_env_set_lk_detect;
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    result->set_lk_max = locked_env_set_lk_max;
#endif
    result->log_archive = locked_env_log_archive;
    result->txn_stat = locked_env_txn_stat;
    result->txn_begin = locked_txn_begin;

    MALLOC(result->i);
    if (result->i == 0) { r = ENOMEM; goto cleanup; }
    memset(result->i, 0, sizeof *result->i);
    result->i->bt_compare  = toku_default_compare_fun;
    result->i->dup_compare = toku_default_compare_fun;
    result->i->is_panicked=0;
    result->i->panic_string = 0;
    result->i->ref_count = 1;
    result->i->errcall = 0;
    result->i->errpfx = 0;
    result->i->errfile = 0;

    r = toku_ltm_create(&result->i->ltm, __toku_env_default_max_locks,
                         toku_db_lt_panic, 
                         toku_db_get_compare_fun, toku_db_get_dup_compare, 
                         toku_malloc, toku_free, toku_realloc);
    if (r!=0) { goto cleanup; }

    {
	r = toku_logger_create(&result->i->logger);
	if (r!=0) { goto cleanup; }
	assert(result->i->logger);
    }

    ydb_add_ref();
    *envp = result;
    r = 0;
cleanup:
    if (r!=0) {
        if (result) {
            if (result->i) {
                if (result->i->ltm) {
                    toku_ltm_close(result->i->ltm);
                }
                toku_free(result->i);
            }
            toku_free(result);
        }
    }
    return r;
}

int DB_ENV_CREATE_FUN (DB_ENV ** envp, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_env_create(envp, flags); toku_ydb_unlock(); return r;
}

static int toku_txn_release_locks(DB_TXN* txn) {
    assert(txn);
    toku_lth* lth = db_txn_struct_i(txn)->lth;

    int r = ENOSYS;
    int first_error = 0;
    if (lth) {
        toku_lth_start_scan(lth);
        toku_lock_tree* next = toku_lth_next(lth);
        while (next) {
            r = toku_lt_unlock(next, toku_txn_get_txnid(db_txn_struct_i(txn)->tokutxn));
            if (!first_error && r!=0) { first_error = r; }
            if (r == 0) {
                r = toku_lt_remove_ref(next);
                if (!first_error && r!=0) { first_error = r; }
            }
            next = toku_lth_next(lth);
        }
        toku_lth_close(lth);
        db_txn_struct_i(txn)->lth = NULL;
    }
    r = first_error;

    return r;
}

// Yield the lock so someone else can work, and then reacquire the lock.
// Useful while processing commit or rollback logs, to allow others to access the system.
static void ydb_yield (voidfp f, void *UU(v)) {
    toku_ydb_unlock(); 
    if (f) f();
    toku_ydb_lock();
}

static int toku_txn_commit(DB_TXN * txn, u_int32_t flags) {
    if (!txn) return EINVAL;
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children
    int r_child_first = 0;
    while (db_txn_struct_i(txn)->child) {
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, flags);
        if (!r_child_first) r_child_first = r_child;
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    //Remove from parent
    if (txn->parent) {
        if (db_txn_struct_i(txn->parent)->child==txn) db_txn_struct_i(txn->parent)->child=db_txn_struct_i(txn)->next;
        if (db_txn_struct_i(txn->parent)->child==txn) {
            db_txn_struct_i(txn->parent)->child=NULL;
        }
        else {
	    db_txn_struct_i(db_txn_struct_i(txn)->next)->prev = db_txn_struct_i(txn)->prev;
            db_txn_struct_i(db_txn_struct_i(txn)->prev)->next = db_txn_struct_i(txn)->next;
        }
    }
    //toku_ydb_notef("flags=%d\n", flags);
    int nosync = (flags & DB_TXN_NOSYNC)!=0 || (db_txn_struct_i(txn)->flags&DB_TXN_NOSYNC);
    flags &= ~DB_TXN_NOSYNC;

    int r;
    if (r_child_first || flags!=0)
	// frees the tokutxn
	// Calls ydb_yield(NULL) occasionally
        r = toku_logger_abort(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL);
    else
	// frees the tokutxn
	// Calls ydb_yield(NULL) occasionally
        r = toku_logger_commit(db_txn_struct_i(txn)->tokutxn, nosync, ydb_yield, NULL);

    // Close the logger after releasing the locks
    int r2 = toku_txn_release_locks(txn);
    toku_logger_txn_close(db_txn_struct_i(txn)->tokutxn);
    // the toxutxn is freed, and we must free the rest. */

    // The txn is no good after the commit even if the commit fails, so free it up.
#if !TOKUDB_NATIVE_H
    toku_free(db_txn_struct_i(txn));
#endif
    toku_free(txn);
    if (flags!=0) return EINVAL;
    return r ? r : (r2 ? r2 : r_child_first);
}

static u_int32_t toku_txn_id(DB_TXN * txn) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    toku_ydb_barf();
    abort();
    return -1;
}

static int toku_txn_abort(DB_TXN * txn) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children
    int r_child_first = 0;
    while (db_txn_struct_i(txn)->child) {
        int r_child = toku_txn_abort(db_txn_struct_i(txn)->child);
        if (!r_child_first) r_child_first = r_child;
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    //Remove from parent
    if (txn->parent) {
        if (db_txn_struct_i(txn->parent)->child==txn) db_txn_struct_i(txn->parent)->child=db_txn_struct_i(txn)->next;
        if (db_txn_struct_i(txn->parent)->child==txn) {
            db_txn_struct_i(txn->parent)->child=NULL;
        }
        else {
            db_txn_struct_i(db_txn_struct_i(txn)->next)->prev = db_txn_struct_i(txn)->prev;
            db_txn_struct_i(db_txn_struct_i(txn)->prev)->next = db_txn_struct_i(txn)->next;
        }
    }
    int r = toku_logger_abort(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL);
    int r2 = toku_txn_release_locks(txn);
    toku_logger_txn_close(db_txn_struct_i(txn)->tokutxn);

#if !TOKUDB_NATIVE_H
    toku_free(db_txn_struct_i(txn));
#endif
    toku_free(txn);
    return r ? r : (r2 ? r2 : r_child_first);
}

static int toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags);

static int locked_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_txn_begin(env, stxn, txn, flags); toku_ydb_unlock(); return r;
}

static u_int32_t locked_txn_id(DB_TXN *txn) {
    toku_ydb_lock(); u_int32_t r = toku_txn_id(txn); toku_ydb_unlock(); return r;
}

static int toku_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    XMALLOC(*txn_stat);
    return toku_logger_txn_rolltmp_raw_count(db_txn_struct_i(txn)->tokutxn, &(*txn_stat)->rolltmp_raw_count);
}

static int locked_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    toku_ydb_lock(); u_int32_t r = toku_txn_stat(txn, txn_stat); toku_ydb_unlock(); return r;
}

static int locked_txn_commit(DB_TXN *txn, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_txn_commit(txn, flags); toku_ydb_unlock(); return r;
}

static int locked_txn_abort(DB_TXN *txn) {
    toku_ydb_lock(); int r = toku_txn_abort(txn); toku_ydb_unlock(); return r;
}

static int toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags) {
    HANDLE_PANICKED_ENV(env);
    if (!toku_logger_is_open(env->i->logger)) return toku_ydb_do_error(env, EINVAL, "Environment does not have logging enabled\n");
    if (!(env->i->open_flags & DB_INIT_TXN))  return toku_ydb_do_error(env, EINVAL, "Environment does not have transactions enabled\n");
    u_int32_t txn_flags = 0;
    txn_flags |= DB_TXN_NOWAIT; //We do not support blocking locks.
    if (flags&DB_READ_UNCOMMITTED) {
        txn_flags |=  DB_READ_UNCOMMITTED;
        flags     &= ~DB_READ_UNCOMMITTED;
    }
    if (flags&DB_TXN_NOWAIT) {
        txn_flags |=  DB_TXN_NOWAIT;
        flags     &= ~DB_TXN_NOWAIT;
    }
    if (flags&DB_TXN_NOSYNC) {
        txn_flags |=  DB_TXN_NOSYNC;
        flags     &= ~DB_TXN_NOSYNC;
    }
    if (flags!=0) return toku_ydb_do_error(env, EINVAL, "Invalid flags passed to DB_ENV->txn_begin\n");

    DB_TXN *MALLOC(result);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, sizeof *result);
    //toku_ydb_notef("parent=%p flags=0x%x\n", stxn, flags);
    result->mgrp = env;
    result->abort = locked_txn_abort;
    result->commit = locked_txn_commit;
    result->id = locked_txn_id;
    result->parent = stxn;
    result->txn_stat = locked_txn_stat;
#if !TOKUDB_NATIVE_H
    MALLOC(db_txn_struct_i(result));
    if (!db_txn_struct_i(result)) {
        toku_free(result);
        return ENOMEM;
    }
#endif
    memset(db_txn_struct_i(result), 0, sizeof *db_txn_struct_i(result));
    db_txn_struct_i(result)->flags = txn_flags;

    int r;
    if (env->i->open_flags & DB_INIT_LOCK && !stxn) {
        r = toku_lth_create(&db_txn_struct_i(result)->lth,
                            toku_malloc, toku_free, toku_realloc);
        if (r!=0) {
#if !TOKUDB_NATIVE_H
            toku_free(db_txn_struct_i(result));
#endif
            toku_free(result);
            return r;
        }
    }
    
    r = toku_logger_txn_begin(stxn ? db_txn_struct_i(stxn)->tokutxn : 0, &db_txn_struct_i(result)->tokutxn, env->i->logger);
    if (r != 0)
        return r;
    //Add to the list of children for the parent.
    if (result->parent) {
        if (!db_txn_struct_i(result->parent)->child) {
            db_txn_struct_i(result->parent)->child = result;
            db_txn_struct_i(result)->next = result;
            db_txn_struct_i(result)->prev = result;
        }
        else {
            db_txn_struct_i(result)->prev = db_txn_struct_i(db_txn_struct_i(result->parent)->child)->prev;
            db_txn_struct_i(result)->next = db_txn_struct_i(result->parent)->child;
            db_txn_struct_i(db_txn_struct_i(db_txn_struct_i(result->parent)->child)->prev)->next = result;
            db_txn_struct_i(db_txn_struct_i(result->parent)->child)->prev = result;
        }
    }
    *txn = result;
    return 0;
}

#if 0
int txn_commit(DB_TXN * txn, u_int32_t flags) {
    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
    return toku_logger_log_commit(db_txn_struct_i(txn)->tokutxn);
}
#endif

int log_compare(const DB_LSN * a, const DB_LSN * b) {
    toku_ydb_lock();
    fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
    assert(0);
    toku_ydb_unlock();
    return 0;
}

static int
db_close_before_brt(DB *db, u_int32_t UU(flags)) {
    char *error_string = 0;
    int r1 = toku_close_brt(db->i->brt, db->dbenv->i->logger, &error_string);
    if (r1) {
	db->dbenv->i->is_panicked = r1; // Panicking the whole environment may be overkill, but I'm not sure what else to do.
	db->dbenv->i->panic_string = error_string;
	if (error_string) {
	    toku_ydb_do_error(db->dbenv, r1, "%s\n", error_string);
	} else {
	    toku_ydb_do_error(db->dbenv, r1, "Closing file\n");
	}
	error_string=0;
    }
    assert(error_string==0);
    int r2 = 0;
    if (db->i->db_id) { toku_db_id_remove_ref(&db->i->db_id); }
    if (db->i->lt) {
        r2 = toku_lt_remove_ref(db->i->lt);
	if (r2) {
	    db->dbenv->i->is_panicked = r2; // Panicking the whole environment may be overkill, but I'm not sure what else to do.
	    db->dbenv->i->panic_string = 0;
	}
    }
    // printf("%s:%d %d=__toku_db_close(%p)\n", __FILE__, __LINE__, r, db);
    // Even if panicked, let's close as much as we can.
    int is_panicked = toku_env_is_panicked(db->dbenv); 
    env_unref(db->dbenv);
    toku_free(db->i->fname);
    toku_free(db->i->full_fname);
    toku_sdbt_cleanup(&db->i->skey);
    toku_sdbt_cleanup(&db->i->sval);
    toku_free(db->i);
    toku_free(db);
    ydb_unref();
    if (r1) return r1;
    if (r2) return r2;
    if (is_panicked) return EINVAL;
    return 0;
}

static int toku_db_close(DB * db, u_int32_t flags) {
    int r = toku_brt_db_delay_closed(db->i->brt, db, db_close_before_brt, flags);
    return r;
}


//Get the main portion of a cursor flag (excluding the bitwise or'd components).
static int get_main_cursor_flag(u_int32_t flags) {
    return flags & DB_OPFLAGS_MASK;
}

static int get_nonmain_cursor_flags(u_int32_t flags) {
    return flags & ~(DB_OPFLAGS_MASK);
}

static inline BOOL toku_c_uninitialized(DBC* c) {
    return toku_brt_cursor_uninitialized(dbc_struct_i(c)->c);
}            

typedef struct query_context_wrapped_t {
    DBT               *key;
    DBT               *val;
    struct simple_dbt *skey;
    struct simple_dbt *sval;
} *QUERY_CONTEXT_WRAPPED, QUERY_CONTEXT_WRAPPED_S;

static inline void
query_context_wrapped_init(QUERY_CONTEXT_WRAPPED context, DBC *c, DBT *key, DBT *val) {
    context->key  = key;
    context->val  = val;
    context->skey = dbc_struct_i(c)->skey;
    context->sval = dbc_struct_i(c)->sval;
}

static int
c_get_wrapper_callback(DBT const *key, DBT const *val, void *extra) {
    QUERY_CONTEXT_WRAPPED context = extra;
    int r;
              r = toku_dbt_set(key->size, key->data, context->key, context->skey);
    if (r==0) r = toku_dbt_set(val->size, val->data, context->val, context->sval);
    return r;
}

static TOKULOGGER
c_get_logger(DBC *c) {
    TOKUTXN txn = dbc_struct_i(c)->txn ? db_txn_struct_i(dbc_struct_i(c)->txn)->tokutxn : NULL;
    TOKULOGGER logger = toku_txn_logger(txn);
    return logger;
}

static int toku_c_get_current_unconditional(DBC* c, u_int32_t flags, DBT* key, DBT* val) {
    int r;
    QUERY_CONTEXT_WRAPPED_S context; 
    query_context_wrapped_init(&context, c, key, val);
    r = toku_c_getf_current_binding(c, flags, c_get_wrapper_callback, &context);
    return r;
}

static inline void toku_swap_flag(u_int32_t* flag, u_int32_t* get_flag,
                                  u_int32_t new_flag) {
    *flag    -= *get_flag;
    *get_flag =  new_flag;
    *flag    += *get_flag;
}

/*
    Used for partial implementation of nested transactions.
    Work is done by children as normal, but all locking is done by the
    root of the nested txn tree.
    This may hold extra locks, and will not work as expected when
    a node has two non-completed txns at any time.
*/
static inline DB_TXN* toku_txn_ancestor(DB_TXN* txn) {
    while (txn && txn->parent) txn = txn->parent;

    return txn;
}

static int toku_txn_add_lt(DB_TXN* txn, toku_lock_tree* lt);

/* c_get has many subfunctions with lots of parameters
 * this structure exists to simplify it. */
typedef struct {
    DBC*        c;                  // The cursor
    DB*         db;                 // db the cursor is iterating over
    DB_TXN*     txn_anc;            // The (root) ancestor of the transaction
    TXNID       id_anc;
    DBT         tmp_key;            // Temporary key to protect out param
    DBT         tmp_val;            // Temporary val to protect out param
    u_int32_t   flag;               // The c_get flag
    u_int32_t   op;                 // The operation portion of the c_get flag
    u_int32_t   lock_flags;         // The prelock flags.
    BOOL        cursor_is_write;    // Whether op can change position of cursor
    BOOL        key_is_read;        
    BOOL        key_is_write;
    BOOL        val_is_read;
    BOOL        val_is_write;
    BOOL        duplicates;
    BOOL        tmp_key_malloced;
    BOOL        tmp_val_malloced;
} C_GET_VARS;

static inline u_int32_t get_prelocked_flags(u_int32_t flags, DB_TXN* txn) {
    u_int32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);

    //DB_READ_UNCOMMITTED transactions 'own' all read locks.
    if (txn && db_txn_struct_i(txn)->flags&DB_READ_UNCOMMITTED) lock_flags |= DB_PRELOCKED;
    return lock_flags;
}

//Return true for NODUP database, false for DUPSORT
static BOOL
db_is_nodup(DB *db) {
    unsigned int brtflags;

    int r = toku_brt_get_flags(db->i->brt, &brtflags);
    assert(r==0);
    BOOL rval = (BOOL)(!(brtflags&TOKU_DB_DUPSORT));
    return rval;
}

static BOOL
c_db_is_nodup(DBC *c) {
    BOOL rval = db_is_nodup(c->dbp);
    return rval;
}

static int
toku_c_get(DBC* c, DBT* key, DBT* val, u_int32_t flag) {
    //OW!! SCALDING!

    u_int32_t main_flag       = get_main_cursor_flag(flag);
    u_int32_t remaining_flags = get_nonmain_cursor_flags(flag);
    int r;
    QUERY_CONTEXT_WRAPPED_S context;
    //Passing in NULL for a key or val means that it is NOT an output.
    //    Both key and val are output:
    //        query_context_wrapped_init(&context, c, key,  val);
    //    Val is output, key is not:
    //            query_context_wrapped_init(&context, c, NULL, val);
    //    Neither key nor val are output:
    //	    query_context_wrapped_init(&context, c, NULL, NULL); // Used for DB_GET_BOTH
    switch (main_flag) {
        case (DB_FIRST):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_first(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_LAST):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_last(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_NEXT):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_next(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_NEXT_DUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_next_dup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_NEXT_NODUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_next_nodup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_PREV):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#ifdef DB_PREV_DUP
        case (DB_PREV_DUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev_dup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#endif
        case (DB_PREV_NODUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev_nodup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_CURRENT):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_current(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_CURRENT_BINDING):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_current_binding(c, remaining_flags, c_get_wrapper_callback, &context);
            break;

        case (DB_SET):
            query_context_wrapped_init(&context, c, NULL, val);
            r = toku_c_getf_set(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_set_range(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_GET_BOTH):
            query_context_wrapped_init(&context, c, NULL, NULL);
            r = toku_c_getf_get_both(c, remaining_flags, key, val, c_get_wrapper_callback, &context);
            break;
        case (DB_GET_BOTH_RANGE):
            //For a nodup database, DB_GET_BOTH_RANGE is an alias for DB_GET_BOTH.
            //DB_GET_BOTH(_RANGE) require different contexts (see case(DB_GET_BOTH)).
            if (c_db_is_nodup(c)) query_context_wrapped_init(&context, c, NULL, NULL);
            else                  query_context_wrapped_init(&context, c, NULL, val);
            r = toku_c_getf_get_both_range(c, remaining_flags, key, val, c_get_wrapper_callback, &context);
            break;
        default:
            r = EINVAL;
            break;
    }
    return r;
}

static int locked_c_getf_first(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_first(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_last(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_last(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_next(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_next(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_next_nodup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_next_nodup(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_next_dup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_next_dup(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_prev(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_prev(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_prev_nodup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_prev_nodup(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_prev_dup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_prev_dup(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_current(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_current(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_current_binding(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_current_binding(c, flag, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_set(DBC *c, u_int32_t flag, DBT * key, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_set(c, flag, key, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_set_range(DBC *c, u_int32_t flag, DBT * key, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_set_range(c, flag, key, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_get_both(DBC *c, u_int32_t flag, DBT * key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_get_both(c, flag, key, val, f, extra); toku_ydb_unlock(); return r;
}

static int locked_c_getf_get_both_range(DBC *c, u_int32_t flag, DBT * key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    toku_ydb_lock();  int r = toku_c_getf_get_both_range(c, flag, key, val, f, extra); toku_ydb_unlock(); return r;
}

typedef struct {
    BOOL            is_read_lock;
    DB_TXN         *txn;
    DB             *db;
    toku_lock_tree *lt;
    DBT const      *left_key;
    DBT const      *left_val;
    DBT const      *right_key;
    DBT const      *right_val;
} *RANGE_LOCK_REQUEST, RANGE_LOCK_REQUEST_S;

static void
range_lock_request_init(RANGE_LOCK_REQUEST request,
                        BOOL       is_read_lock,
                        DB_TXN    *txn,
                        DB        *db,
                        DBT const *left_key,
                        DBT const *left_val,
                        DBT const *right_key,
                        DBT const *right_val) {
    request->is_read_lock = is_read_lock;
    request->txn = txn;
    request->db = db;
    request->lt = db->i->lt;
    request->left_key = left_key;
    request->left_val = left_val;
    request->right_key = right_key;
    request->right_val = right_val;
}


static void
read_lock_request_init(RANGE_LOCK_REQUEST request,
                       DB_TXN    *txn,
                       DB        *db,
                       DBT const *left_key,
                       DBT const *left_val,
                       DBT const *right_key,
                       DBT const *right_val) {
    range_lock_request_init(request, TRUE, txn, db,
                            left_key,  left_val,
                            right_key, right_val);
}

static void
write_lock_request_init(RANGE_LOCK_REQUEST request,
                        DB_TXN    *txn,
                        DB        *db,
                        DBT const *left_key,
                        DBT const *left_val,
                        DBT const *right_key,
                        DBT const *right_val) {
    range_lock_request_init(request, FALSE, txn, db,
                            left_key,  left_val,
                            right_key, right_val);
}

static int
grab_range_lock(RANGE_LOCK_REQUEST request) {
    int r;
    //TODO: (Multithreading) Grab lock protecting lock tree
    DB_TXN *txn_anc = toku_txn_ancestor(request->txn);
    r = toku_txn_add_lt(txn_anc, request->lt);
    if (r==0) {
        TXNID txn_anc_id = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);
        if (request->is_read_lock)
            r = toku_lt_acquire_range_read_lock(request->lt, request->db, txn_anc_id,
                                                request->left_key,  request->left_val,
                                                request->right_key, request->right_val);
        else 
            r = toku_lt_acquire_range_write_lock(request->lt, request->db, txn_anc_id,
                                                 request->left_key,  request->left_val,
                                                 request->right_key, request->right_val);
    }
    //TODO: (Multithreading) Release lock protecting lock tree
    return r;
}

//This is the user level callback function given to ydb layer functions like
//toku_c_getf_first

typedef struct query_context_base_t {
    BRT_CURSOR  c;
    DB_TXN     *txn;
    DB         *db;
    void       *f_extra;
    BOOL        do_locking;
    int         r_user_callback;
} *QUERY_CONTEXT_BASE, QUERY_CONTEXT_BASE_S;

typedef struct query_context_t {
    QUERY_CONTEXT_BASE_S  base;
    YDB_CALLBACK_FUNCTION f;
} *QUERY_CONTEXT, QUERY_CONTEXT_S;

typedef struct query_context_with_input_t {
    QUERY_CONTEXT_BASE_S  base;
    YDB_CALLBACK_FUNCTION f;
    DBT                  *input_key;
    DBT                  *input_val;
} *QUERY_CONTEXT_WITH_INPUT, QUERY_CONTEXT_WITH_INPUT_S;


static void
query_context_base_init(QUERY_CONTEXT_BASE context, DBC *c, u_int32_t flag, void *extra) {
    context->c       = dbc_struct_i(c)->c;
    context->txn     = dbc_struct_i(c)->txn;
    context->db      = c->dbp;
    context->f_extra = extra;
    u_int32_t lock_flags = get_prelocked_flags(flag, dbc_struct_i(c)->txn);
    flag &= ~lock_flags;
    assert(flag==0);
    context->do_locking = (BOOL)(context->db->i->lt!=NULL && !lock_flags);
    context->r_user_callback = 0;
}

static void
query_context_init(QUERY_CONTEXT context, DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    query_context_base_init(&context->base, c, flag, extra);
    context->f = f;
}

static void
query_context_with_input_init(QUERY_CONTEXT_WITH_INPUT context, DBC *c, u_int32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    query_context_base_init(&context->base, c, flag, extra);
    context->f         = f;
    context->input_key = key;
    context->input_val = val;
}

static int c_del_callback(DBT const *key, DBT const *val, void *extra);

//Delete whatever the cursor is pointing at.
static int
toku_c_del(DBC * c, u_int32_t flags) {
    HANDLE_PANICKED_DB(c->dbp);

    u_int32_t unchecked_flags = flags;
    //DB_DELETE_ANY means delete regardless of whether it exists in the db.
    u_int32_t flag_for_brt = flags&DB_DELETE_ANY;
    unchecked_flags &= ~flag_for_brt;
    u_int32_t lock_flags = get_prelocked_flags(flags, dbc_struct_i(c)->txn);
    unchecked_flags &= ~lock_flags;
    BOOL do_locking = (BOOL)(c->dbp->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));

    int r = 0;
    if (unchecked_flags!=0) r = EINVAL;
    else {
        if (do_locking) {
            QUERY_CONTEXT_S context;
            query_context_init(&context, c, lock_flags, NULL, NULL);
            //We do not need a read lock, we must already have it.
            r = toku_c_getf_current_binding(c, DB_PRELOCKED, c_del_callback, &context);
        }
        if (r==0) {
            //Do the actual delete.
            TOKUTXN txn = dbc_struct_i(c)->txn ? db_txn_struct_i(dbc_struct_i(c)->txn)->tokutxn : 0;
            r = toku_brt_cursor_delete(dbc_struct_i(c)->c, flag_for_brt, txn);
        }
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_del_callback(DBT const *key, DBT const *val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    assert(context->do_locking);
    assert(key!=NULL);
    assert(val!=NULL);
    //Lock:
    //  left(key,val)==right(key,val) == (key, val);
    RANGE_LOCK_REQUEST_S request;
    write_lock_request_init(&request, context->txn, context->db,
                            key, val,
                            key, val);
    r = grab_range_lock(&request);

    //Give brt-layer an error (if any) to return from toku_c_getf_current_binding
    return r;
}

static int c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_first(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_first will call c_getf_first_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_first(dbc_struct_i(c)->c, c_getf_first_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity, toku_lt_neg_infinity,
                                   &found_key,           &found_val);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity, toku_lt_neg_infinity,
                                   toku_lt_infinity,     toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_first
    return r;
}

static int c_getf_last_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_last(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_last will call c_getf_last_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_last(dbc_struct_i(c)->c, c_getf_last_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_last_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   &found_key,           &found_val,
                                   toku_lt_infinity,     toku_lt_infinity);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   toku_lt_neg_infinity, toku_lt_neg_infinity,
                                   toku_lt_infinity,     toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_last
    return r;
}

static int c_getf_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_next(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    if (c_db_is_nodup(c))             r = toku_c_getf_next_nodup(c, flag, f, extra);
    else if (toku_c_uninitialized(c)) r = toku_c_getf_first(c, flag, f, extra);
    else {
        QUERY_CONTEXT_S context; //Describes the context of this query.
        query_context_init(&context, c, flag, f, extra); 
        TOKULOGGER logger = c_get_logger(c);
        //toku_brt_cursor_next will call c_getf_next_callback(..., context) (if query is successful)
        r = toku_brt_cursor_next(dbc_struct_i(c)->c, c_getf_next_callback, &context, logger);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        const DBT *prevkey;
        const DBT *prevval;
        const DBT *right_key = key==NULL ? toku_lt_infinity : &found_key;
        const DBT *right_val = key==NULL ? toku_lt_infinity : &found_val;

        toku_brt_cursor_peek(context->c, &prevkey, &prevval);
        read_lock_request_init(&request, context->txn, context->db,
                               prevkey,   prevval,
                               right_key, right_val);
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_next
    return r;
}

static int
toku_c_getf_next_nodup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    if (toku_c_uninitialized(c)) r = toku_c_getf_first(c, flag, f, extra);
    else {
        QUERY_CONTEXT_S context; //Describes the context of this query.
        query_context_init(&context, c, flag, f, extra); 
        TOKULOGGER logger = c_get_logger(c);
        //toku_brt_cursor_next will call c_getf_next_callback(..., context) (if query is successful)
        r = toku_brt_cursor_next_nodup(dbc_struct_i(c)->c, c_getf_next_callback, &context, logger);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

static int c_getf_next_dup_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_next_dup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    if (toku_c_uninitialized(c)) return EINVAL;

    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_next_dup will call c_getf_next_dup_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_next_dup(dbc_struct_i(c)->c, c_getf_next_dup_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_next_dup_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        const DBT *prevkey;
        const DBT *prevval;
        const DBT *right_val = key==NULL ? toku_lt_infinity : &found_val;

        toku_brt_cursor_peek(context->c, &prevkey, &prevval);
        read_lock_request_init(&request, context->txn, context->db,
                               prevkey,  prevval,
                               prevkey,  right_val); //found_key is same as prevkey for this case
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_next_dup
    return r;
}

static int c_getf_prev_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_prev(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    if (c_db_is_nodup(c))             r = toku_c_getf_prev_nodup(c, flag, f, extra);
    else if (toku_c_uninitialized(c)) r = toku_c_getf_last(c, flag, f, extra);
    else {
        QUERY_CONTEXT_S context; //Describes the context of this query.
        query_context_init(&context, c, flag, f, extra); 
        TOKULOGGER logger = c_get_logger(c);
        //toku_brt_cursor_prev will call c_getf_prev_callback(..., context) (if query is successful)
        r = toku_brt_cursor_prev(dbc_struct_i(c)->c, c_getf_prev_callback, &context, logger);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_prev_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        const DBT *prevkey;
        const DBT *prevval;
        const DBT *left_key = key==NULL ? toku_lt_neg_infinity : &found_key;
        const DBT *left_val = key==NULL ? toku_lt_neg_infinity : &found_val;

        toku_brt_cursor_peek(context->c, &prevkey, &prevval);
        read_lock_request_init(&request, context->txn, context->db,
                               left_key, left_val,
                               prevkey,  prevval);
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_prev
    return r;
}

static int
toku_c_getf_prev_nodup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    if (toku_c_uninitialized(c)) r = toku_c_getf_last(c, flag, f, extra);
    else {
        QUERY_CONTEXT_S context; //Describes the context of this query.
        query_context_init(&context, c, flag, f, extra); 
        TOKULOGGER logger = c_get_logger(c);
        //toku_brt_cursor_prev will call c_getf_prev_callback(..., context) (if query is successful)
        r = toku_brt_cursor_prev_nodup(dbc_struct_i(c)->c, c_getf_prev_callback, &context, logger);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

static int c_getf_prev_dup_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_prev_dup(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    if (toku_c_uninitialized(c)) return EINVAL;

    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_prev_dup will call c_getf_prev_dup_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_prev_dup(dbc_struct_i(c)->c, c_getf_prev_dup_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_prev_dup_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        const DBT *prevkey;
        const DBT *prevval;
        const DBT *left_val = key==NULL ? toku_lt_neg_infinity : &found_val;

        toku_brt_cursor_peek(context->c, &prevkey, &prevval);
        read_lock_request_init(&request, context->txn, context->db,
                               prevkey,  left_val, //found_key is same as prevkey for this case
                               prevkey,  prevval);
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_prev_dup
    return r;
}

static int c_getf_current_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_current(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_current(dbc_struct_i(c)->c, DB_CURRENT, c_getf_current_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_current_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT      super_context = extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    int r=0;
    //Call application-layer callback if found.
    if (key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_current
    return r;
}

static int
toku_c_getf_current_binding(DBC *c, u_int32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    query_context_init(&context, c, flag, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_current(dbc_struct_i(c)->c, DB_CURRENT_BINDING, c_getf_current_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

static int c_getf_set_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_set(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_set will call c_getf_set_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_set(dbc_struct_i(c)->c, key, NULL, c_getf_set_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key,val)  = (input_key, -infinity)
    //  right(key,val) = (input_key, found ? found_val : infinity)
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key, toku_lt_neg_infinity,
                                   super_context->input_key, &found_val);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key, toku_lt_neg_infinity,
                                   super_context->input_key, toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_set
    return r;
}

static int c_getf_set_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_set_range(DBC *c, u_int32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_set_range will call c_getf_set_range_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_set_range(dbc_struct_i(c)->c, key, c_getf_set_range_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key,val)  = (input_key, -infinity)
    //  right(key) = found ? found_key : infinity
    //  right(val) = found ? found_val : infinity
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key, toku_lt_neg_infinity,
                                   &found_key,               &found_val);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key, toku_lt_neg_infinity,
                                   toku_lt_infinity,         toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_set_range
    return r;
}

static int c_getf_get_both_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_get_both(DBC *c, u_int32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);

    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, val, f, extra); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_get_both will call c_getf_get_both_callback(..., context) (if query is successful)
    int r = toku_brt_cursor_set(dbc_struct_i(c)->c, key, val, c_getf_get_both_callback, &context, logger);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_get_both_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key,val)  = (input_key, input_val)
    //  right==left
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        read_lock_request_init(&request, context->txn, context->db,
                               super_context->input_key, super_context->input_val,
                               super_context->input_key, super_context->input_val);
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_get_both
    return r;
}

static int c_getf_get_both_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra);

static int
toku_c_getf_get_both_range(DBC *c, u_int32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    int r;
    if (c_db_is_nodup(c)) r = toku_c_getf_get_both(c, flag, key, val, f, extra);
    else {
        QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
        query_context_with_input_init(&context, c, flag, key, val, f, extra); 
        TOKULOGGER logger = c_get_logger(c);
        //toku_brt_cursor_get_both_range will call c_getf_get_both_range_callback(..., context) (if query is successful)
        r = toku_brt_cursor_get_both_range(dbc_struct_i(c)->c, key, val, c_getf_get_both_range_callback, &context, logger);
        if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_get_both_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra) {
    QUERY_CONTEXT_WITH_INPUT super_context = extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, key, keylen);
    toku_fill_dbt(&found_val, val, vallen);

    //Lock:
    //  left(key,val)  = (input_key, input_val)
    //  right(key,val) = (input_key, found ? found_val : infinity)
    if (context->do_locking) {
        RANGE_LOCK_REQUEST_S request;
        if (key!=NULL) {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key, super_context->input_val,
                                   super_context->input_key, &found_val);
        }
        else {
            read_lock_request_init(&request, context->txn, context->db,
                                   super_context->input_key, super_context->input_val,
                                   super_context->input_key, toku_lt_infinity);
        }
        r = grab_range_lock(&request);
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

    //Give brt-layer an error (if any) to return from toku_brt_cursor_get_both_range
    return r;
}


static int locked_c_getf_heaviside(DBC *c, u_int32_t flags,
                               YDB_HEAVISIDE_CALLBACK_FUNCTION f, void *extra_f,
                               YDB_HEAVISIDE_FUNCTION h, void *extra_h, int direction) {
    toku_ydb_lock();  int r = toku_c_getf_heaviside(c, flags, f, extra_f, h, extra_h, direction); toku_ydb_unlock(); return r;
}

typedef struct {
    QUERY_CONTEXT_BASE_S            base;
    YDB_HEAVISIDE_CALLBACK_FUNCTION f;
    HEAVI_WRAPPER                   wrapper;
} *QUERY_CONTEXT_HEAVISIDE, QUERY_CONTEXT_HEAVISIDE_S;

static void
query_context_heaviside_init(QUERY_CONTEXT_HEAVISIDE context, DBC *c, u_int32_t flag, YDB_HEAVISIDE_CALLBACK_FUNCTION f, void *extra, HEAVI_WRAPPER wrapper) {
    query_context_base_init(&context->base, c, flag, extra);
    context->f       = f;
    context->wrapper = wrapper;
}

static void
heavi_wrapper_init(HEAVI_WRAPPER wrapper, int (*h)(const DBT *key, const DBT *value, void *extra_h), void *extra_h, int direction) {
    wrapper->h         = h;
    wrapper->extra_h   = extra_h;
    wrapper->r_h       = direction; //Default value of r_h (may be set to 0 later)->
    wrapper->direction = direction;
}

static int c_getf_heaviside_callback(ITEMLEN found_keylen, bytevec found_key, ITEMLEN found_vallen, bytevec found_val,
                                     ITEMLEN next_keylen,  bytevec next_key,  ITEMLEN next_vallen,  bytevec next_val,
                                     void *extra);

static int
toku_c_getf_heaviside(DBC *c, u_int32_t flag,
                      YDB_HEAVISIDE_CALLBACK_FUNCTION f, void *extra_f,
                      YDB_HEAVISIDE_FUNCTION h, void *extra_h,
                      int direction) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HEAVI_WRAPPER_S wrapper;
    heavi_wrapper_init(&wrapper, h, extra_h, direction);
    QUERY_CONTEXT_HEAVISIDE_S context; //Describes the context of this query.
    query_context_heaviside_init(&context, c, flag, f, extra_f, &wrapper); 
    TOKULOGGER logger = c_get_logger(c);
    //toku_brt_cursor_heaviside will call c_getf_heaviside_callback(..., context) (if query is successful)
    r = toku_brt_cursor_heaviside(dbc_struct_i(c)->c, c_getf_heaviside_callback, &context, logger, &wrapper);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
//bytevec==NULL means not found.
static int c_getf_heaviside_callback(ITEMLEN found_keylen, bytevec found_keyvec, ITEMLEN found_vallen, bytevec found_valvec,
                                 ITEMLEN next_keylen,  bytevec next_keyvec,  ITEMLEN next_vallen,  bytevec next_valvec,
                                 void *extra) {
    QUERY_CONTEXT_HEAVISIDE super_context = extra;
    QUERY_CONTEXT_BASE      context       = &super_context->base;

    int r;
    int r2 = 0;

    DBT found_key;
    DBT found_val;
    toku_fill_dbt(&found_key, found_keyvec, found_keylen);
    toku_fill_dbt(&found_val, found_valvec, found_vallen);

    if (context->do_locking) {
        const DBT *left_key  = toku_lt_neg_infinity;
        const DBT *left_val  = toku_lt_neg_infinity;
        const DBT *right_key = toku_lt_infinity;
        const DBT *right_val = toku_lt_infinity;
        RANGE_LOCK_REQUEST_S request;
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
        //Have cursor (base->c)
        //Have txn    (base->txn)
        //Have db     (base->db)
        BOOL found = (BOOL)(found_keyvec != NULL);
        DBC *tmp_cursor; //Temporary cursor to find 'next_key/next_val'
        DBT tmp_key;
        DBT tmp_val;
        toku_init_dbt(&tmp_key);
        toku_init_dbt(&tmp_val);
        r = toku_db_cursor(context->db, context->txn, &tmp_cursor, 0, 0);
        if (r!=0) goto tmp_cleanup;
        //Find the 'next key and next val'
        //We will do all relevent range locking, so there is no need for any sub-queries to do locking.
        //Pass in DB_PRELOCKED.
        if (super_context->wrapper->direction<0) {
            if (found) {
                //do an 'after'
                //call DB_GET_BOTH to set the temp cursor to the 'found' values
                //then call 'DB_NEXT' to advance it to the values we want
                r = toku_c_getf_get_both(tmp_cursor, DB_PRELOCKED, &found_key, &found_val, ydb_getf_do_nothing, NULL);
                if (r==0) {
                    r = toku_c_get(tmp_cursor, &tmp_key, &tmp_val, DB_NEXT|DB_PRELOCKED);
                    if (r==DB_NOTFOUND) r = 0;
                }
            }
            else {
                //do a 'first'
                r = toku_c_get(tmp_cursor, &tmp_key, &tmp_val, DB_FIRST|DB_PRELOCKED);
                if (r==DB_NOTFOUND) r = 0;
            }
        }
        else {
            if (found) {
                //do a 'before'
                //call DB_GET_BOTH to set the temp cursor to the 'found' values
                //then call 'DB_PREV' to advance it to the values we want
                r = toku_c_getf_get_both(tmp_cursor, DB_PRELOCKED, &found_key, &found_val, ydb_getf_do_nothing, NULL);
                if (r==0) {
                    r = toku_c_get(tmp_cursor, &tmp_key, &tmp_val, DB_PREV|DB_PRELOCKED);
                    if (r==DB_NOTFOUND) r = 0;
                }
            }
            else {
                //do a 'last'
                r = toku_c_get(tmp_cursor, &tmp_key, &tmp_val, DB_LAST|DB_PRELOCKED);
                if (r==DB_NOTFOUND) r = 0;
            }
        }
        if (r==0) {
            next_keyvec = tmp_key.data;
            next_keylen = tmp_key.size;
            next_valvec = tmp_val.data;
            next_vallen = tmp_val.size;
        }
        else goto temp_cursor_cleanup;
#endif
        DBT next_key;
        DBT next_val;
        toku_fill_dbt(&next_key, next_keyvec, next_keylen);
        toku_fill_dbt(&next_val, next_valvec, next_vallen);
        if (super_context->wrapper->direction<0) {
            if (found_keyvec!=NULL) {
                left_key  = &found_key; 
                left_val  = &found_val; 
            }
            if (next_keyvec!=NULL) {
                right_key = &next_key; 
                right_val = &next_val; 
            }
        }
        else {
            if (next_keyvec!=NULL) {
                left_key  = &next_key; 
                left_val  = &next_val; 
            }
            if (found_keyvec!=NULL) {
                right_key = &found_key; 
                right_val = &found_val; 
            }
        }
        read_lock_request_init(&request, context->txn, context->db,
                               left_key,   left_val,
                               right_key,  right_val);
        r = grab_range_lock(&request);
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
temp_cursor_cleanup:
        r2 = toku_c_close(tmp_cursor);
        //cleanup cursor
#endif
    }
    else r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && found_keyvec!=NULL) {
        context->r_user_callback = super_context->f(&found_key, &found_val, context->f_extra, super_context->wrapper->r_h);
        if (context->r_user_callback) r = TOKUDB_USER_CALLBACK_ERROR;
    }

#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
tmp_cleanup:
#endif
    //Give brt-layer an error (if any) to return from toku_brt_cursor_heavi
    return r ? r : r2;
}

static int toku_c_close(DBC * c) {
    int r = toku_brt_cursor_close(dbc_struct_i(c)->c);
    toku_sdbt_cleanup(&dbc_struct_i(c)->skey_s);
    toku_sdbt_cleanup(&dbc_struct_i(c)->sval_s);
#if !TOKUDB_NATIVE_H
    toku_free(dbc_struct_i(c));
#endif
    toku_free(c);
    return r;
}

static inline int keyeq(DBC *c, DBT *a, DBT *b) {
    DB *db = c->dbp;
    return db->i->brt->compare_fun(db, a, b) == 0;
}

// Return the number of entries whose key matches the key currently 
// pointed to by the brt cursor.  
static int 
toku_c_count(DBC *cursor, db_recno_t *count, u_int32_t flags) {
    int r;
    DBC *count_cursor = 0;
    DBT currentkey;

    init_dbt_realloc(&currentkey);
    u_int32_t lock_flags = get_prelocked_flags(flags, dbc_struct_i(cursor)->txn);
    flags &= ~lock_flags;
    if (flags != 0) {
        r = EINVAL; goto finish;
    }

    r = toku_c_get_current_unconditional(cursor, lock_flags, &currentkey, NULL);
    if (r != 0) goto finish;

    //TODO: Optimization
    //if (do_locking) {
    //   do a lock from currentkey,-infinity to currentkey,infinity
    //   lock_flags |= DB_PRELOCKED
    //}
    
    r = toku_db_cursor(cursor->dbp, dbc_struct_i(cursor)->txn, &count_cursor, 0, 0);
    if (r != 0) goto finish;

    *count = 0;
    r = toku_c_getf_set(count_cursor, lock_flags, &currentkey, ydb_getf_do_nothing, NULL);
    if (r != 0) {
        r = 0; goto finish; /* success, the current key must be deleted and there are no more */
    }

    for (;;) {
        *count += 1;
        r = toku_c_getf_next_dup(count_cursor, lock_flags, ydb_getf_do_nothing, NULL);
        if (r != 0) break;
    }
    r = 0; /* success, we found at least one before the end */
finish:
    if (currentkey.data) toku_free(currentkey.data);
    if (count_cursor) {
        int rr = toku_c_close(count_cursor); assert(rr == 0);
    }
    return r;
}


///////////
//db_getf_XXX is equivalent to c_getf_XXX, without a persistent cursor

static int
db_getf_set(DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    DBC *c;
    int r = toku_db_cursor(db, txn, &c, 0, 1);
    if (r==0) {
        r = toku_c_getf_set(c, flags, key, f, extra);
        int r2 = toku_c_close(c);
        if (r==0) r = r2;
    }
    return r;
}

static int
db_getf_get_both(DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    DBC *c;
    int r = toku_db_cursor(db, txn, &c, 0, 1);
    if (r==0) {
        r = toku_c_getf_get_both(c, flags, key, val, f, extra);
        int r2 = toku_c_close(c);
        if (r==0) r = r2;
    }
    return r;
}
////////////

static int
toku_db_del(DB *db, DB_TXN *txn, DBT *key, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    u_int32_t unchecked_flags = flags;
    //DB_DELETE_ANY means delete regardless of whether it exists in the db.
    BOOL error_if_missing = (BOOL)(!(flags&DB_DELETE_ANY));
    unchecked_flags &= ~DB_DELETE_ANY;
    u_int32_t lock_flags = get_prelocked_flags(flags, txn);
    unchecked_flags &= ~lock_flags;
    BOOL do_locking = (BOOL)(db->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));
    int r = 0;
    if (unchecked_flags!=0) r = EINVAL;
    if (r==0 && error_if_missing) {
        //Check if the key exists in the db.
        r = db_getf_set(db, txn, lock_flags, key, ydb_getf_do_nothing, NULL);
    }
    if (r==0 && do_locking) {
        //Do locking if necessary.
        RANGE_LOCK_REQUEST_S request;
        //Left end of range == right end of range (point lock)
        write_lock_request_init(&request, txn, db,
                                key, toku_lt_neg_infinity,
                                key, toku_lt_infinity);
        r = grab_range_lock(&request);
    }
    if (r==0) {
        //Do the actual deleting.
        r = toku_brt_delete(db->i->brt, key, txn ? db_txn_struct_i(txn)->tokutxn : 0);
    }
    return r;
}

static int locked_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag) {
    //{ unsigned int i; printf("cget flags=%d keylen=%d key={", flag, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", data->size); for(i=0; i<data->size; i++) printf("%d,", ((char*)data->data)[i]); printf("}\n"); }
    toku_ydb_lock(); int r = toku_c_get(c, key, data, flag); toku_ydb_unlock();
    //{ unsigned int i; printf("cgot r=%d keylen=%d key={", r, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", data->size); for(i=0; i<data->size; i++) printf("%d,", ((char*)data->data)[i]); printf("}\n"); }
    return r;
}

static int locked_c_close(DBC * c) {
    toku_ydb_lock(); int r = toku_c_close(c); toku_ydb_unlock(); return r;
}

static int locked_c_count(DBC *cursor, db_recno_t *count, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_c_count(cursor, count, flags); toku_ydb_unlock(); return r;
}

static int locked_c_del(DBC * c, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_c_del(c, flags); toku_ydb_unlock(); return r;
}

static int toku_db_cursor(DB * db, DB_TXN * txn, DBC ** c, u_int32_t flags, int is_temporary_cursor) {
    HANDLE_PANICKED_DB(db);
    if (flags != 0)
        return EINVAL;
    DBC *MALLOC(result);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, sizeof *result);
#define SCRS(name) result->name = locked_ ## name
    SCRS(c_get);
    SCRS(c_close);
    SCRS(c_del);
    SCRS(c_count);
    SCRS(c_getf_first);
    SCRS(c_getf_last);
    SCRS(c_getf_next);
    SCRS(c_getf_next_nodup);
    SCRS(c_getf_next_dup);
    SCRS(c_getf_prev);
    SCRS(c_getf_prev_nodup);
    SCRS(c_getf_prev_dup);
    SCRS(c_getf_current);
    SCRS(c_getf_current_binding);
    SCRS(c_getf_heaviside);
    SCRS(c_getf_set);
    SCRS(c_getf_set_range);
    SCRS(c_getf_get_both);
    SCRS(c_getf_get_both_range);
#undef SCRS

#if !TOKUDB_NATIVE_H
    MALLOC(result->i); // otherwise it is allocated as part of result->ii
    assert(result->i);
#endif
    result->dbp = db;
    dbc_struct_i(result)->txn = txn;
    dbc_struct_i(result)->skey_s = (struct simple_dbt){0,0};
    dbc_struct_i(result)->sval_s = (struct simple_dbt){0,0};
    if (is_temporary_cursor) {
	dbc_struct_i(result)->skey = &db->i->skey;
	dbc_struct_i(result)->sval = &db->i->sval;
    } else {
	dbc_struct_i(result)->skey = &dbc_struct_i(result)->skey_s;
	dbc_struct_i(result)->sval = &dbc_struct_i(result)->sval_s;
    }
    int r = toku_brt_cursor(db->i->brt, &dbc_struct_i(result)->c);
    assert(r == 0);
    *c = result;
    return 0;
}

static int
toku_db_delboth(DB *db, DB_TXN *txn, DBT *key, DBT *val, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    u_int32_t unchecked_flags = flags;
    //DB_DELETE_ANY means delete regardless of whether it exists in the db.
    BOOL error_if_missing = (BOOL)(!(flags&DB_DELETE_ANY));
    unchecked_flags &= ~DB_DELETE_ANY;
    u_int32_t lock_flags = get_prelocked_flags(flags, txn);
    unchecked_flags &= ~lock_flags;
    BOOL do_locking = (BOOL)(db->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));
    int r = 0;
    if (unchecked_flags!=0) r = EINVAL;
    if (r==0 && error_if_missing) {
        //Check if the key exists in the db.
        r = db_getf_get_both(db, txn, lock_flags, key, val, ydb_getf_do_nothing, NULL);
    }
    if (r==0 && do_locking) {
        //Do locking if necessary.
        RANGE_LOCK_REQUEST_S request;
        //Left end of range == right end of range (point lock)
        write_lock_request_init(&request, txn, db,
                                key, val,
                                key, val);
        r = grab_range_lock(&request);
    }
    if (r==0) {
        //Do the actual deleting.
        r = toku_brt_delete_both(db->i->brt, key, val, txn ? db_txn_struct_i(txn)->tokutxn : NULL);
    }
    return r;
}

static inline int db_thread_need_flags(DBT *dbt) {
    return (dbt->flags & (DB_DBT_MALLOC+DB_DBT_REALLOC+DB_DBT_USERMEM)) == 0;
}

static int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    int r;

    if ((db->i->open_flags & DB_THREAD) && db_thread_need_flags(data))
        return EINVAL;

    u_int32_t lock_flags = get_prelocked_flags(flags, txn);
    flags &= ~lock_flags;
    if (flags != 0 && flags != DB_GET_BOTH) return EINVAL;
    // We aren't ready to handle flags such as DB_READ_COMMITTED or DB_READ_UNCOMMITTED or DB_RMW

    DBC *dbc;
    r = toku_db_cursor(db, txn, &dbc, 0, 1);
    if (r!=0) return r;
    u_int32_t c_get_flags = (flags == 0) ? DB_SET : DB_GET_BOTH;
    r = toku_c_get(dbc, key, data, c_get_flags | lock_flags);
    int r2 = toku_c_close(dbc);
    return r ? r : r2;
}

#if 0
static int toku_db_key_range(DB * db, DB_TXN * txn, DBT * dbt, DB_KEY_RANGE * kr, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    txn=txn; dbt=dbt; kr=kr; flags=flags;
    toku_ydb_barf();
    abort();
}
#endif

static int construct_full_name_in_buf(const char *dir, const char *fname, char* full, int length) {
    int l;

    if (!full) return EINVAL;
    if (toku_os_is_absolute_name(fname)) {
        l = 0;
        full[0] = '\0';
    }
    else {
        l = snprintf(full, length, "%s", dir);
        if (l >= length) return ENAMETOOLONG;
        if (l == 0 || full[l - 1] != '/') {
            if (l + 1 == length) return ENAMETOOLONG;
                
            /* Didn't put a slash down. */
            if (fname[0] != '/') {
                full[l++] = '/';
                full[l] = 0;
            }
        }
    }
    l += snprintf(full + l, length - l, "%s", fname);
    if (l >= length) return ENAMETOOLONG;
    return 0;
}

static char *construct_full_name(const char *dir, const char *fname) {
    if (toku_os_is_absolute_name(fname))
        dir = "";
    {
        int dirlen = strlen(dir);
        int fnamelen = strlen(fname);
        int len = dirlen + fnamelen + 2;        // One for the / between (which may not be there).  One for the trailing null.
        char *result = toku_malloc(len);
        // printf("%s:%d len(%d)=%d+%d+2\n", __FILE__, __LINE__, len, dirlen, fnamelen);
        if (construct_full_name_in_buf(dir, fname, result, len) != 0) {
            toku_free(result);
            result = NULL;
        }
        return result;
    }
}

static int find_db_file(DB_ENV* dbenv, const char *fname, char** full_name_out) {
    u_int32_t i;
    int r;
    toku_struct_stat statbuf;
    char* full_name;
    
    assert(full_name_out);    
    if (dbenv->i->data_dirs!=NULL) {
        assert(dbenv->i->n_data_dirs > 0);
        for (i = 0; i < dbenv->i->n_data_dirs; i++) {
            full_name = construct_full_name(dbenv->i->data_dirs[0], fname);
            if (!full_name) return ENOMEM;
            r = toku_stat(full_name, &statbuf);
            if (r == 0) goto finish;
            else {
                toku_free(full_name);
                r = errno;
                if (r != ENOENT) return r;
            }
        }
        //Did not find it at all.  Return the first data dir.
        full_name = construct_full_name(dbenv->i->data_dirs[0], fname);
        goto finish;
    }
    //Default without data_dirs is the environment directory.
    full_name = construct_full_name(dbenv->i->dir, fname);
    goto finish;

finish:
    if (!full_name) return ENOMEM;
    *full_name_out = full_name;
    return 0;    
}

static int toku_db_lt_panic(DB* db, int r) {
    assert(r!=0);
    assert(db && db->i && db->dbenv && db->dbenv->i);
    DB_ENV* env = db->dbenv;
    env->i->is_panicked = r;

    if (r < 0) env->i->panic_string = toku_strdup(toku_lt_strerror((TOKU_LT_ERROR)r));
    else       env->i->panic_string = toku_strdup("Error in locktree.\n");

    return toku_ydb_do_error(env, r, "%s", env->i->panic_string);
}

static int toku_txn_add_lt(DB_TXN* txn, toku_lock_tree* lt) {
    int r = ENOSYS;
    assert(txn && lt);
    toku_lth* lth = db_txn_struct_i(txn)->lth;
    assert(lth);

    toku_lock_tree* find = toku_lth_find(lth, lt);
    if (find) {
        assert(find == lt);
        r = 0;
        goto cleanup;
    }
    r = toku_lth_insert(lth, lt);
    if (r != 0) { goto cleanup; }
    
    toku_lt_add_ref(lt);
    r = 0;
cleanup:
    return r;
}

static toku_dbt_cmp toku_db_get_compare_fun(DB* db) {
    return db->i->brt->compare_fun;
}

static toku_dbt_cmp toku_db_get_dup_compare(DB* db) {
    return db->i->brt->dup_compare;
}

static int toku_db_fd(DB *db, int *fdp) {
    HANDLE_PANICKED_DB(db);
    if (!db_opened(db)) return EINVAL;
    return toku_brt_get_fd(db->i->brt, fdp);
}

static int toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode);

static int multiple_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    //Only flag we look at is create.  If create is there, we create directory if necessary.
    //If create is not set and directory not exists, we quit out with errors.
    int is_db_create  = flags & DB_CREATE;

    char *directory_name = NULL;
    BOOL created_directory = FALSE;
    int r = find_db_file(db->dbenv, fname, &directory_name);
    if (r!=0) goto cleanup;
    toku_struct_stat statbuf;
    r = toku_stat(directory_name, &statbuf);
    if (r!=0) { r = errno; assert(r!=0); }
    if (r==0 && !S_ISDIR(statbuf.st_mode)) { r = ENOTDIR; goto cleanup; } //File exists, but is not a directory.
    if (r!=0 && r!=ENOENT) goto cleanup;
    if (r==ENOENT && is_db_create) {
        //Try to create the directory.
        r = toku_os_mkdir(directory_name, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
        if (r==0) created_directory = TRUE;
        else { r = errno; assert(r!=0); }
    }
    if (r!=0) goto cleanup;
    {
        char subdb_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        int bytes = snprintf(subdb_full_name, sizeof(subdb_full_name), "%s/%s", fname, dbname);
        assert(bytes==(int)sizeof(subdb_full_name)-1);
        const char *null_subdbname = NULL;
        r = toku_db_open(db, txn, subdb_full_name, null_subdbname, dbtype, flags, mode);
    }

cleanup:
    if (r!=0) {
        if (created_directory) {
            //Failure on create, and directory did not previously exist.
            //Lets delete the directory.
            //TODO: Since we failed the db create, perhaps we should delete
            //the directory (it was only made for this).
            //Possible race conditions could exist if we do this.
            //Files may still be there.
        }
    }
    if (directory_name) toku_free(directory_name);
    return r;
}

static int toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    HANDLE_PANICKED_DB(db);
    if (dbname!=NULL) 
        return multiple_db_open(db, txn, fname, dbname, dbtype, flags, mode);

    //This code ONLY supports single-db files.
    assert(dbname==NULL);
    // Warning.  Should check arguments.  Should check return codes on malloc and open and so forth.
    BOOL need_locktree = (BOOL)((db->dbenv->i->open_flags & DB_INIT_LOCK) &&
                                (db->dbenv->i->open_flags & DB_INIT_TXN));

    int openflags = 0;
    int r;
    if (dbtype!=DB_BTREE && dbtype!=DB_UNKNOWN) return EINVAL;
    int is_db_excl    = flags & DB_EXCL;    flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  flags&=~DB_CREATE;
    int is_db_rdonly  = flags & DB_RDONLY;  flags&=~DB_RDONLY;
    //We support READ_UNCOMMITTED whether or not the flag is provided.
                                            flags&=~DB_READ_UNCOMMITTED;
    if (dbtype != DB_UNKNOWN && dbtype != DB_BTREE) return EINVAL;
    if (flags & ~DB_THREAD) return EINVAL; // unknown flags

    if (is_db_excl && !is_db_create) return EINVAL;
    if (dbtype==DB_UNKNOWN && is_db_excl) return EINVAL;

    /* tokudb supports no duplicates and sorted duplicates only */
    unsigned int tflags;
    r = toku_brt_get_flags(db->i->brt, &tflags);
    if (r != 0) 
        return r;
    if ((tflags & TOKU_DB_DUP) && !(tflags & TOKU_DB_DUPSORT))
        return EINVAL;

    if (db_opened(db))
        return EINVAL;              /* It was already open. */
    
    r = find_db_file(db->dbenv, fname, &db->i->full_fname);
    if (r != 0) goto error_cleanup;
    // printf("Full name = %s\n", db->i->full_fname);
    assert(db->i->fname == 0);
    db->i->fname = toku_strdup(fname);
    if (db->i->fname == 0) { 
        r = ENOMEM; goto error_cleanup;
    }
    if (is_db_rdonly)
        openflags |= O_RDONLY;
    else
        openflags |= O_RDWR;
    
    {
        toku_struct_stat statbuf;
        if (toku_stat(db->i->full_fname, &statbuf) == 0) {
            /* If the database exists at the file level, and we specified no db_name, then complain here. */
            if (dbname == 0 && is_db_create) {
                if (is_db_excl) {
                    r = EEXIST;
                    goto error_cleanup;
                }
		is_db_create = 0; // It's not a create after all, since the file exists.
            }
        } else {
            if (!is_db_create) {
                r = ENOENT;
                goto error_cleanup;
            }
        }
    }
    if (is_db_create) openflags |= O_CREAT;

    db->i->open_flags = flags;
    db->i->open_mode = mode;


    r = toku_brt_open(db->i->brt, db->i->full_fname, fname,
		      is_db_create, is_db_excl,
		      db->dbenv->i->cachetable,
		      txn ? db_txn_struct_i(txn)->tokutxn : NULL_TXN,
		      db);
    if (r != 0)
        goto error_cleanup;

    if (need_locktree) {
        unsigned int brtflags;
        BOOL dups;
        toku_brt_get_flags(db->i->brt, &brtflags);
        dups = (BOOL)((brtflags & TOKU_DB_DUPSORT || brtflags & TOKU_DB_DUP));

        int db_fd;
        r = toku_db_fd(db, &db_fd);
        if (r!=0) goto error_cleanup;
        assert(db_fd>=0);
        r = toku_db_id_create(&db->i->db_id, db_fd);
        if (r!=0) { goto error_cleanup; }
        r = toku_ltm_get_lt(db->dbenv->i->ltm, &db->i->lt, dups, db->i->db_id);
        if (r!=0) { goto error_cleanup; }
    }

    return 0;
 
error_cleanup:
    if (db->i->db_id) {
        toku_db_id_remove_ref(&db->i->db_id);
        db->i->db_id = NULL;
    }
    if (db->i->full_fname) {
        toku_free(db->i->full_fname);
        db->i->full_fname = NULL;
    }
    if(db->i->fname) {
        toku_free(db->i->fname);
        db->i->fname = NULL;
    }
    if (db->i->lt) {
        toku_lt_remove_ref(db->i->lt);
        db->i->lt = NULL;
    }
    return r;
}

//Return 0 if proposed pair do not violate size constraints of DB
//(insertion is legal)
//Return non zero otherwise.
static int
db_put_check_size_constraints(DB *db, DBT *key, DBT *val) {
    int r;

    BOOL dupsort = (BOOL)(!db_is_nodup(db));
    //Check limits on size of key and val.
    unsigned int nodesize;
    r = toku_brt_get_nodesize(db->i->brt, &nodesize); assert(r == 0);
    u_int32_t limit;

    if (dupsort) {
        limit = nodesize / (2*BRT_FANOUT-1);
        if (key->size + val->size >= limit)
            r = toku_ydb_do_error(db->dbenv, EINVAL, "The largest (key + val) item allowed is %u bytes", limit-1);
    } else {
        limit = nodesize / (3*BRT_FANOUT-1);
        if (key->size >= limit || val->size >= limit)
            r = toku_ydb_do_error(db->dbenv, EINVAL, "The largest key or val item allowed is %u bytes", limit-1);
    }
    return r;
}

//Return 0 if supported.
//Return ERANGE if out of range.
static int
db_row_size_supported(DB *db, u_int32_t size) {
    DBT key, val;

    toku_fill_dbt(&key, NULL, size);
    toku_fill_dbt(&val, NULL, 0);
    int r = db_put_check_size_constraints(db, &key, &val);
    if (r!=0) r = ERANGE;
    return r;
}

static int
locked_db_row_size_supported(DB *db, u_int32_t size) {
    toku_ydb_lock();
    int r = db_row_size_supported(db, size);
    toku_ydb_unlock();
    return r;
}

//Return 0 if insert is legal
static int
db_put_check_overwrite_constraint(DB *db, DB_TXN *txn, DBT *key, DBT *UU(val),
                                  u_int32_t lock_flags, u_int32_t overwrite_flag) {
    int r;

    //DB_YESOVERWRITE does not impose constraints.
    if (overwrite_flag==DB_YESOVERWRITE) r = 0;
    else if (overwrite_flag==DB_NOOVERWRITE) {
        //Check if (key,anything) exists in dictionary.
        //If exists, fail.  Otherwise, do insert.
        r = db_getf_set(db, txn, lock_flags, key, ydb_getf_do_nothing, NULL);
        if (r==DB_NOTFOUND) r = 0;
        else if (r==0)      r = DB_KEYEXIST;
        //Any other error is passed through.
    }
    else if (overwrite_flag==0) {
        //in a nodup db:   overwrite_flag==0 is an alias for DB_YESOVERWRITE
        //in a dupsort db: overwrite_flag==0 is an error
        if (db_is_nodup(db)) r = 0;
        else {
            r = toku_ydb_do_error(db->dbenv, EINVAL, "Tokudb requires that db->put specify DB_YESOVERWRITE or DB_NOOVERWRITE on DB_DUPSORT databases");
        }
    }
    else {
        //Other flags are not (yet) supported.
        r = EINVAL;
    }
    return r;
}

static int
toku_db_put(DB *db, DB_TXN *txn, DBT *key, DBT *val, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    int r;

    u_int32_t lock_flags = get_prelocked_flags(flags, txn);
    flags &= ~lock_flags;
    BOOL do_locking = (BOOL)(db->i->lt && !(lock_flags&DB_PRELOCKED_WRITE));

    r = db_put_check_size_constraints(db, key, val);
    if (r==0) {
        //Do any checking required by the flags.
        r = db_put_check_overwrite_constraint(db, txn, key, val, lock_flags, flags);
    }
    if (r==0 && do_locking) {
        //Do locking if necessary.
        RANGE_LOCK_REQUEST_S request;
        //Left end of range == right end of range (point lock)
        write_lock_request_init(&request, txn, db,
                                key, val,
                                key, val);
        r = grab_range_lock(&request);
    }
    if (r==0) {
        //Insert into the brt.
        r = toku_brt_insert(db->i->brt, key, val, txn ? db_txn_struct_i(txn)->tokutxn : 0);
    }
    return r;
}

static int toku_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags);

static int
toku_db_remove_subdb(DB * db, const char *fname, const char *dbname, u_int32_t flags) {
    BOOL need_close = TRUE;
    char *directory_name = NULL;
    int r = find_db_file(db->dbenv, fname, &directory_name);
    if (r!=0) goto cleanup;
    toku_struct_stat statbuf;
    r = toku_stat(directory_name, &statbuf);
    if (r==0 && !S_ISDIR(statbuf.st_mode)) { r = ENOTDIR; goto cleanup; } //File exists, but is not a directory.
    if (r!=0) { r = errno; assert(r!=0); goto cleanup; }
    {
        char subdb_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        int bytes = snprintf(subdb_full_name, sizeof(subdb_full_name), "%s/%s", fname, dbname);
        assert(bytes==(int)sizeof(subdb_full_name)-1);
        const char *null_subdbname = NULL;
        need_close = FALSE;
        r = toku_db_remove(db, subdb_full_name, null_subdbname, flags);
    }

cleanup:
    if (need_close) {
        int r2 = toku_db_close(db, 0);
        if (r==0) r = r2;
    }
    if (directory_name) toku_free(directory_name);
    return r;
}

//TODO: Maybe delete directory when last 'subdb' is deleted.
static int toku_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    if (dbname)
        return toku_db_remove_subdb(db, fname, dbname, flags);
    int r = ENOSYS;
    int r2 = 0;
    toku_db_id* db_id = NULL;
    BOOL need_close   = TRUE;
    char* full_name   = NULL;
    toku_ltm* ltm     = NULL;

    //TODO: Verify DB* db not yet opened
    //TODO: Verify db file not in use. (all dbs in the file must be unused)
    r = toku_db_open(db, NULL, fname, dbname, DB_UNKNOWN, 0, S_IRWXU|S_IRWXG|S_IRWXO);
    if (r==TOKUDB_DICTIONARY_TOO_OLD || r==TOKUDB_DICTIONARY_TOO_NEW || r==TOKUDB_DICTIONARY_NO_HEADER) {
        need_close = FALSE;
        goto delete_db_file;
    }
    if (r!=0) { goto cleanup; }
    if (db->i->lt) {
        /* Lock tree exists, therefore:
           * Non-private environment (since we are using transactions)
           * Environment will exist after db->close */
        if (db->i->db_id) {
            /* 'copy' the db_id */
            db_id = db->i->db_id;
            toku_db_id_add_ref(db_id);
        }
        if (db->dbenv->i->ltm) { ltm = db->dbenv->i->ltm; }
    }
    
delete_db_file:
    r = find_db_file(db->dbenv, fname, &full_name);
    if (r!=0) { goto cleanup; }
    assert(full_name);
    r = toku_db_close(db, 0);
    need_close = FALSE;
    if (r!=0) { goto cleanup; }
    if (unlink(full_name) != 0) { r = errno; goto cleanup; }

    if (ltm && db_id) { toku_ltm_invalidate_lt(ltm, db_id); }

    r = 0;
cleanup:
    if (need_close) { r2 = toku_db_close(db, 0); }
    if (full_name)  { toku_free(full_name); }
    if (db_id)      { toku_db_id_remove_ref(&db_id); }
    return r ? r : r2;
}

/* TODO: Either
    -find a way for the DB_ID to survive this rename (i.e. be
     the same before and after
    or
    -Go through all DB_IDs in the ltm, and rename them so we
     have the correct unique ids.
   TODO: Verify the DB file is not in use (either a single db file or
         a file with multi-databases).
   TODO: Check the other directories in the environment for the file
TODO: Alert the BRT layer (for logging/recovery purposes)
*/
static int toku_db_rename(DB * db, const char *namea, const char *nameb, const char *namec, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    if (flags!=0) return EINVAL;
    char afull[PATH_MAX], cfull[PATH_MAX];
    int r;
    if (nameb)
        r = snprintf(afull, PATH_MAX, "%s%s/%s", db->dbenv->i->dir, namea, nameb);
    else
        r = snprintf(afull, PATH_MAX, "%s%s", db->dbenv->i->dir, namea);
    assert(r < PATH_MAX);
    r = snprintf(cfull, PATH_MAX, "%s%s", db->dbenv->i->dir, namec);
    assert(r < PATH_MAX);
    return rename(afull, cfull);
}

static int toku_db_set_bt_compare(DB * db, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    HANDLE_PANICKED_DB(db);
    int r = toku_brt_set_bt_compare(db->i->brt, bt_compare);
    return r;
}

static int toku_db_set_dup_compare(DB *db, int (*dup_compare)(DB *, const DBT *, const DBT *)) {
    HANDLE_PANICKED_DB(db);
    int r = toku_brt_set_dup_compare(db->i->brt, dup_compare);
    return r;
}

static int toku_db_set_descriptor(DB *db, u_int32_t version, const DBT* descriptor, toku_dbt_upgradef dbt_userformat_upgrade) {
    HANDLE_PANICKED_DB(db);
    int r;
    if (db_opened(db)) return EINVAL;
    else if (!descriptor) r = EINVAL;
    else if (descriptor->size>0 && !descriptor->data) r = EINVAL;
    else r = toku_brt_set_descriptor(db->i->brt, version, descriptor, dbt_userformat_upgrade);
    return r;
}

static int toku_db_set_flags(DB *db, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);

    /* the following matches BDB */
    if (db_opened(db) && flags != 0) return EINVAL;

    u_int32_t tflags;
    int r = toku_brt_get_flags(db->i->brt, &tflags);
    if (r!=0) return r;
    
    if (flags & DB_DUP)
        tflags += TOKU_DB_DUP;
    if (flags & DB_DUPSORT)
        tflags += TOKU_DB_DUPSORT;
    r = toku_brt_set_flags(db->i->brt, tflags);
    return r;
}

static int toku_db_get_flags(DB *db, u_int32_t *pflags) {
    HANDLE_PANICKED_DB(db);
    if (!pflags) return EINVAL;
    u_int32_t tflags;
    u_int32_t flags = 0;
    int r = toku_brt_get_flags(db->i->brt, &tflags);
    if (r!=0) return r;
    if (tflags & TOKU_DB_DUP) {
        tflags &= ~TOKU_DB_DUP;
        flags  |= DB_DUP;
    }
    if (tflags & TOKU_DB_DUPSORT) {
        tflags &= ~TOKU_DB_DUPSORT;
        flags  |= DB_DUPSORT;
    }
    assert(tflags == 0);
    *pflags = flags;
    return 0;
}

static int toku_db_set_pagesize(DB *db, u_int32_t pagesize) {
    HANDLE_PANICKED_DB(db);
    int r = toku_brt_set_nodesize(db->i->brt, pagesize);
    return r;
}

static int toku_db_stat64(DB * db, DB_TXN *txn, DB_BTREE_STAT64 *s) {
    HANDLE_PANICKED_DB(db);
    return toku_brt_stat64(db->i->brt, db_txn_struct_i(txn)->tokutxn, &s->bt_nkeys, &s->bt_ndata, &s->bt_dsize, &s->bt_fsize);
}
static int locked_db_stat64 (DB *db, DB_TXN *txn, DB_BTREE_STAT64 *s) {
    toku_ydb_lock();
    int r = toku_db_stat64(db, txn, s);
    toku_ydb_unlock();
    return r;

}

static int toku_db_key_range64(DB* db, DB_TXN* txn __attribute__((__unused__)), DBT* key, u_int64_t* less, u_int64_t* equal, u_int64_t* greater, int* is_exact) {
    HANDLE_PANICKED_DB(db);

    // note that toku_brt_keyrange does not have a txn param
    // this will be fixed later
    // temporarily, because the caller, locked_db_keyrange, 
    // has the ydb lock, we are ok
    int r = toku_brt_keyrange(db->i->brt, key, less, equal, greater);
    if (r != 0) { goto cleanup; }
    // temporarily set is_exact to 0 because brt_keyrange does not have this parameter
    *is_exact = 0;
cleanup:
    return r;
}

static int toku_db_pre_acquire_read_lock(DB *db, DB_TXN *txn, const DBT *key_left, const DBT *val_left, const DBT *key_right, const DBT *val_right) {
    HANDLE_PANICKED_DB(db);
    if (!db->i->lt || !txn) return EINVAL;
    //READ_UNCOMMITTED transactions do not need read locks.
    if (db_txn_struct_i(txn)->flags&DB_READ_UNCOMMITTED) return 0;

    DB_TXN* txn_anc = toku_txn_ancestor(txn);
    int r;
    if ((r=toku_txn_add_lt(txn_anc, db->i->lt))) return r;
    TXNID id_anc = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);

    r = toku_lt_acquire_range_read_lock(db->i->lt, db, id_anc,
                                        key_left,  val_left,
                                        key_right, val_right);
    return r;
}

static int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn) {
    HANDLE_PANICKED_DB(db);
    if (!db->i->lt || !txn) return EINVAL;

    DB_TXN* txn_anc = toku_txn_ancestor(txn);
    int r;
    if ((r=toku_txn_add_lt(txn_anc, db->i->lt))) return r;
    TXNID id_anc = toku_txn_get_txnid(db_txn_struct_i(txn_anc)->tokutxn);

    r = toku_lt_acquire_range_write_lock(db->i->lt, db, id_anc,
                                         toku_lt_neg_infinity, toku_lt_neg_infinity,
                                         toku_lt_infinity,     toku_lt_infinity);
    if (r==0) {
	r = toku_brt_note_table_lock(db->i->brt, db_txn_struct_i(txn)->tokutxn); // tell the BRT layer that the table is locked (so that it can reduce the amount of rollback (rolltmp) data.
    }

    return r;
}

//TODO: DB_AUTO_COMMIT.
//TODO: Nowait only conditionally?
//TODO: NOSYNC change to SYNC if DB_ENV has something in set_flags
static inline int toku_db_construct_autotxn(DB* db, DB_TXN **txn, BOOL* changed,
                                            BOOL force_auto_commit) {
    assert(db && txn && changed);
    DB_ENV* env = db->dbenv;
    if (*txn || !(env->i->open_flags & DB_INIT_TXN)) {
        *changed = FALSE;
        return 0;
    }
    BOOL nosync = (BOOL)(!force_auto_commit && !(env->i->open_flags & DB_AUTO_COMMIT));
    u_int32_t txn_flags = DB_TXN_NOWAIT | (nosync ? DB_TXN_NOSYNC : 0);
    int r = toku_txn_begin(env, NULL, txn, txn_flags);
    if (r!=0) return r;
    *changed = TRUE;
    return 0;
}

static inline int toku_db_destruct_autotxn(DB_TXN *txn, int r, BOOL changed) {
    if (!changed) return r;
    if (r==0) return toku_txn_commit(txn, 0);
    toku_txn_abort(txn);
    return r; 
}

static int locked_db_close(DB * db, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_db_close(db, flags); toku_ydb_unlock(); return r;
}

static inline int autotxn_db_cursor(DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
    if (!txn && (db->dbenv->i->open_flags & DB_INIT_TXN)) {
        return toku_ydb_do_error(db->dbenv, EINVAL,
              "Cursors in a transaction environment must have transactions.\n");
    }
    return toku_db_cursor(db, txn, c, flags, 0);
}

static int locked_db_cursor(DB *db, DB_TXN *txn, DBC **c, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_cursor(db, txn, c, flags); toku_ydb_unlock(); return r;
}

static inline int autotxn_db_del(DB* db, DB_TXN* txn, DBT* key,
                                 u_int32_t flags) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_del(db, txn, key, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int locked_db_del(DB * db, DB_TXN * txn, DBT * key, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_del(db, txn, key, flags); toku_ydb_unlock(); return r;
}

static inline int autotxn_db_delboth(DB* db, DB_TXN* txn, DBT* key, DBT* val,
                                 u_int32_t flags) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_delboth(db, txn, key, val, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int locked_db_delboth(DB *db, DB_TXN *txn, DBT *key,  DBT *val, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_delboth(db, txn, key, val, flags); toku_ydb_unlock(); return r;
}

static inline int autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data,
                                 u_int32_t flags) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_get(db, txn, key, data, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int locked_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_get(db, txn, key, data, flags); toku_ydb_unlock(); return r;
}

static int locked_db_pre_acquire_read_lock(DB *db, DB_TXN *txn, const DBT *key_left, const DBT *val_left, const DBT *key_right, const DBT *val_right) {
    toku_ydb_lock();
    int r = toku_db_pre_acquire_read_lock(db, txn, key_left, val_left, key_right, val_right);
    toku_ydb_unlock();
    return r;
}

static int locked_db_pre_acquire_table_lock(DB *db, DB_TXN *txn) {
    toku_ydb_lock();
    int r = toku_db_pre_acquire_table_lock(db, txn);
    toku_ydb_unlock();
    return r;
}

// truncate a database
// effect: remove all of the rows from a database
static int toku_db_truncate(DB *db, DB_TXN *txn, u_int32_t *row_count, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    int r;

    // dont support flags (yet)
    if (flags)
        return EINVAL;
    // dont support cursors 
    if (toku_brt_get_cursor_count(db->i->brt) > 0)
        return EINVAL;

    // acquire a table lock
    if (txn) {
        r = toku_db_pre_acquire_table_lock(db, txn);
        if (r != 0)
            return r;
    }

    *row_count = 0;

    r = toku_brt_truncate(db->i->brt);

    return r;
}

static inline int autotxn_db_open(DB* db, DB_TXN* txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, (BOOL)((flags & DB_AUTO_COMMIT) != 0));
    if (r!=0) return r;
    r = toku_db_open(db, txn, fname, dbname, dbtype, flags & ~DB_AUTO_COMMIT, mode);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int locked_db_open(DB *db, DB_TXN *txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    toku_ydb_lock(); int r = autotxn_db_open(db, txn, fname, dbname, dbtype, flags, mode); toku_ydb_unlock(); return r;
}

static inline int autotxn_db_put(DB* db, DB_TXN* txn, DBT* key, DBT* data,
                                 u_int32_t flags) {
    //{ unsigned i; printf("put %p keylen=%d key={", db, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", data->size); for(i=0; i<data->size; i++) printf("%d,", ((char*)data->data)[i]); printf("}\n"); }
    BOOL changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE);
    if (r!=0) return r;
    r = toku_db_put(db, txn, key, data, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int locked_db_put(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    toku_ydb_lock(); int r = autotxn_db_put(db, txn, key, data, flags); toku_ydb_unlock(); return r;
}

static int locked_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags) {
    toku_checkpoint_safe_client_lock();
    toku_ydb_lock();
    int r = toku_db_remove(db, fname, dbname, flags);
    toku_ydb_unlock();
    toku_checkpoint_safe_client_unlock();
    return r;
}

static int locked_db_rename(DB * db, const char *namea, const char *nameb, const char *namec, u_int32_t flags) {
    toku_checkpoint_safe_client_lock();
    toku_ydb_lock();
    int r = toku_db_rename(db, namea, nameb, namec, flags);
    toku_ydb_unlock();
    toku_checkpoint_safe_client_unlock();
    return r;
}

static int locked_db_set_bt_compare(DB * db, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    toku_ydb_lock(); int r = toku_db_set_bt_compare(db, bt_compare); toku_ydb_unlock(); return r;
}

static int locked_db_set_dup_compare(DB * db, int (*dup_compare) (DB *, const DBT *, const DBT *)) {
    toku_ydb_lock(); int r = toku_db_set_dup_compare(db, dup_compare); toku_ydb_unlock(); return r;
}

static int locked_db_set_descriptor(DB *db, u_int32_t version, const DBT* descriptor, toku_dbt_upgradef dbt_userformat_upgrade) {
    toku_ydb_lock();
    int r = toku_db_set_descriptor(db, version, descriptor, dbt_userformat_upgrade);
    toku_ydb_unlock();
    return r;
}

static void locked_db_set_errfile (DB *db, FILE *errfile) {
    db->dbenv->set_errfile(db->dbenv, errfile);
}

static int locked_db_set_flags(DB *db, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_db_set_flags(db, flags); toku_ydb_unlock(); return r;
}

static int locked_db_get_flags(DB *db, u_int32_t *flags) {
    toku_ydb_lock(); int r = toku_db_get_flags(db, flags); toku_ydb_unlock(); return r;
}

static int locked_db_set_pagesize(DB *db, u_int32_t pagesize) {
    toku_ydb_lock(); int r = toku_db_set_pagesize(db, pagesize); toku_ydb_unlock(); return r;
}

static int locked_db_fd(DB *db, int *fdp) {
    toku_ydb_lock(); int r = toku_db_fd(db, fdp); toku_ydb_unlock(); return r;
}


static int locked_db_key_range64(DB* db, DB_TXN* txn, DBT* dbt, u_int64_t* less, u_int64_t* equal, u_int64_t* greater, int* is_exact) {
    toku_ydb_lock(); int r = toku_db_key_range64(db, txn, dbt, less, equal, greater, is_exact); toku_ydb_unlock(); return r;
}

static const DBT* toku_db_dbt_pos_infty(void) __attribute__((pure));
static const DBT* toku_db_dbt_pos_infty(void) {
    return toku_lt_infinity;
}

static const DBT* toku_db_dbt_neg_infty(void) __attribute__((pure));
static const DBT* toku_db_dbt_neg_infty(void) {
    return toku_lt_neg_infinity;
}

static int locked_db_truncate(DB *db, DB_TXN *txn, u_int32_t *row_count, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_db_truncate(db, txn, row_count, flags); toku_ydb_unlock(); return r;
}

static int toku_db_create(DB ** db, DB_ENV * env, u_int32_t flags) {
    int r;

    if (flags) return EINVAL;

    /* if the env already exists then add a ref to it
       otherwise create one */
    if (env) {
        if (!env_opened(env))
            return EINVAL;
        env_add_ref(env);
    } else {
        r = toku_env_create(&env, 0);
        if (r != 0)
            return r;
        r = toku_env_open(env, ".", DB_PRIVATE + DB_INIT_MPOOL, 0);
        if (r != 0) {
            env_unref(env);
            return r;
        }
        assert(env_opened(env));
    }
    
    DB *MALLOC(result);
    if (result == 0) {
        env_unref(env);
        return ENOMEM;
    }
    memset(result, 0, sizeof *result);
    result->dbenv = env;
#define SDB(name) result->name = locked_db_ ## name
    SDB(key_range64);
    SDB(close);
    SDB(cursor);
    SDB(del);
    SDB(delboth);
    SDB(get);
    //    SDB(key_range);
    SDB(open);
    SDB(put);
    SDB(remove);
    SDB(rename);
    SDB(set_bt_compare);
    SDB(set_dup_compare);
    SDB(set_descriptor);
    SDB(set_errfile);
    SDB(set_pagesize);
    SDB(set_flags);
    SDB(get_flags);
    SDB(stat64);
    SDB(fd);
    SDB(pre_acquire_read_lock);
    SDB(pre_acquire_table_lock);
    SDB(truncate);
    SDB(row_size_supported);
#undef SDB
    result->dbt_pos_infty = toku_db_dbt_pos_infty;
    result->dbt_neg_infty = toku_db_dbt_neg_infty;
    MALLOC(result->i);
    if (result->i == 0) {
        toku_free(result);
        env_unref(env);
        return ENOMEM;
    }
    memset(result->i, 0, sizeof *result->i);
    result->i->db = result;
    result->i->freed = 0;
    result->i->full_fname = 0;
    result->i->open_flags = 0;
    result->i->open_mode = 0;
    result->i->brt = 0;
    r = toku_brt_create(&result->i->brt);
    if (r != 0) {
        toku_free(result->i);
        toku_free(result);
        env_unref(env);
        return r;
    }
    r = toku_brt_set_bt_compare(result->i->brt, env->i->bt_compare);
    assert(r==0);
    r = toku_brt_set_dup_compare(result->i->brt, env->i->dup_compare);
    assert(r==0);
    ydb_add_ref();
    *db = result;
    return 0;
}

int DB_CREATE_FUN (DB ** db, DB_ENV * env, u_int32_t flags) {
    toku_ydb_lock(); int r = toku_db_create(db, env, flags); toku_ydb_unlock(); return r;
}

/* need db_strerror_r for multiple threads */

char *db_strerror(int error) {
    char *errorstr;
    if (error >= 0) {
        errorstr = strerror(error);
        if (errorstr)
            return errorstr;
    }
    
    if (error==DB_BADFORMAT) {
	return "Database Bad Format (probably a corrupted database)";
    }
    if (error==DB_NOTFOUND) {
	return "Not found";
    }

    static char unknown_result[100];    // Race condition if two threads call this at the same time. However even in a bad case, it should be some sort of null-terminated string.
    errorstr = unknown_result;
    snprintf(errorstr, sizeof unknown_result, "Unknown error code: %d", error);
    return errorstr;
}

const char *db_version(int *major, int *minor, int *patch) {
    if (major)
        *major = DB_VERSION_MAJOR;
    if (minor)
        *minor = DB_VERSION_MINOR;
    if (patch)
        *patch = DB_VERSION_PATCH;
#if defined(TOKUDB_REVISION)
#define xstr(X) str(X)
#define str(X) #X
    return "tokudb " xstr(DB_VERSION_MAJOR) "." xstr(DB_VERSION_MINOR) "." xstr(DB_VERSION_PATCH) " build " xstr(TOKUDB_REVISION);
#else
    return DB_VERSION_STRING;
#endif
}
 
int db_env_set_func_fsync (int (*fsync_function)(int)) {
    return toku_set_func_fsync(fsync_function);
}

int db_env_set_func_pwrite (ssize_t (*pwrite_function)(int, const void *, size_t, toku_off_t)) {
    return toku_set_func_pwrite(pwrite_function);
}
int db_env_set_func_write (ssize_t (*write_function)(int, const void *, size_t)) {
    return toku_set_func_write(write_function);
}

int db_env_set_func_malloc (void *(*f)(size_t)) {
    return toku_set_func_malloc(f);
}
int db_env_set_func_realloc (void *(*f)(void*, size_t)) {
    return toku_set_func_realloc(f);
}
int db_env_set_func_free (void (*f)(void*)) {
    return toku_set_func_free(f);
}
// Got to call dlmalloc, or else it won't get included.
void setup_dlmalloc (void) {
    db_env_set_func_malloc(dlmalloc);
    db_env_set_func_realloc(dlrealloc);
    db_env_set_func_free(dlfree);
}

// For test purposes only.
// With this interface, all checkpoint users get the same callback and the same extra.
void db_env_set_checkpoint_callback (void (*callback_f)(void*), void* extra) {
    toku_checkpoint_safe_client_lock();
    checkpoint_callback_f = callback_f;
    checkpoint_callback_extra = extra;
    toku_checkpoint_safe_client_unlock();
    //printf("set callback = %p, extra = %p\n", callback_f, extra);
}
