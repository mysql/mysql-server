/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

const char *toku_patent_string = "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it.";
const char *toku_copyright_string = "Copyright (c) 2007 Tokutek Inc.  All rights reserved.";

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <libgen.h>

#include "ydb-internal.h"

#include "brt-internal.h"
#include "cachetable.h"
#include "log.h"
#include "memory.h"

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    TOKUTXN tokutxn;
    DB_TXN *parent;
};

static char *construct_full_name(const char *dir, const char *fname);
static int do_associated_inserts (DB_TXN *txn, DBT *key, DBT *data, DB *secondary);
    
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
typedef void (*toku_env_errcall_t)(const char *, char *);
#elif DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
typedef void (*toku_env_errcall_t)(DB_ENV *, const char *, const char *);
#else
#error
#endif

struct __toku_db_env_internal {
    int ref_count;
    u_int32_t open_flags;
    int open_mode;
    toku_env_errcall_t errcall;
    void *errfile;
    char *errpfx;
    char *dir;                  /* A malloc'd copy of the directory. */
    char *tmp_dir;
    char *lg_dir;
    char **data_dirs;
    u_int32_t n_data_dirs;
    //void (*noticecall)(DB_ENV *, db_notices);
    long cachetable_size;
    CACHETABLE cachetable;
    TOKULOGGER logger;
};

// Probably this do_error (which is dumb and stupid) should do something consistent with do_env_err.
static void do_error (DB_ENV *dbenv, const char *string) {
    if (dbenv->i->errfile)
	fprintf(dbenv->i->errfile, "%s\n", string);
}

void toku_db_env_err_vararg(const DB_ENV * env, int error, const char *fmt, va_list ap) {
    FILE* ferr = env->i->errfile ? env->i->errfile : stderr;
    if (env->i->errpfx && env->i->errpfx[0] != '\0') fprintf(stderr, "%s: ", env->i->errpfx);
    fprintf(ferr, "YDB Error %d: ", error);
    vfprintf(ferr, fmt, ap);
}

static void toku_db_env_err(const DB_ENV * env, int error, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    toku_db_env_err_vararg(env, error, fmt, ap);
    va_end(ap);
}

#define barf() ({ fprintf(stderr, "YDB: BARF %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define barff(fmt,...) ({ fprintf(stderr, "YDB: BARF %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })
#define note() ({ fprintf(stderr, "YDB: Note %s:%d in %s\n", __FILE__, __LINE__, __func__); })
#define notef(fmt,...) ({ fprintf(stderr, "YDB: Note %s:%d in %s, ", __FILE__, __LINE__, __func__); fprintf(stderr, fmt, __VA_ARGS__); })

#if 0
static void print_flags(u_int32_t flags) {
    u_int32_t gotit = 0;
    int doneone = 0;
#define doit(flag) if (flag & flags) { if (doneone) fprintf(stderr, " | "); fprintf(stderr, "%s", #flag);  doneone=1; gotit|=flag; }
    doit(DB_INIT_LOCK);
    doit(DB_INIT_LOG);
    doit(DB_INIT_MPOOL);
    doit(DB_INIT_TXN);
    doit(DB_CREATE);
    doit(DB_THREAD);
    doit(DB_RECOVER);
    doit(DB_PRIVATE);
    if (gotit != flags)
        fprintf(stderr, "  flags 0x%x not accounted for", flags & ~gotit);
    fprintf(stderr, "\n");
}
#endif

/* TODO make these thread safe */

/* a count of the open env handles */
static int toku_ydb_refs = 0;

static void ydb_add_ref() {
    toku_ydb_refs += 1;
}

static void ydb_unref() {
    assert(toku_ydb_refs > 0);
    toku_ydb_refs -= 1;
    if (toku_ydb_refs == 0) {
        /* call global destructors */
        toku_malloc_cleanup();
    }
}

static void db_env_add_ref(DB_ENV *env) {
    env->i->ref_count += 1;
}

static void db_env_unref(DB_ENV *env) {
    env->i->ref_count -= 1;
    if (env->i->ref_count == 0)
        env->close(env, 0);
}

static inline int db_env_opened(DB_ENV *env) {
    return env->i->cachetable != 0;
}

static inline int db_opened(DB *db) {
    return db->i->full_fname != 0;
}

static int db_env_parse_config_line(DB_ENV* dbenv, char *command, char *value) {
    int r;
    
    if (!strcmp(command, "set_data_dir")) {
        r = dbenv->set_data_dir(dbenv, value);
    }
    else if (!strcmp(command, "set_tmp_dir")) {
        r = dbenv->set_tmp_dir(dbenv, value);
    }
    else if (!strcmp(command, "set_lg_dir")) {
        r = dbenv->set_lg_dir(dbenv, value);
    }
    else r = -1;
        
    return r;
}

static int db_env_read_config(DB_ENV *env) {
    const char* config_name = "DB_CONFIG";
    char* full_name = NULL;
    char* linebuffer = NULL;
    int buffersize;
    FILE* fp = NULL;
    int r = 0;
    int r2 = 0;
    char* command;
    char* value;
    
    full_name = construct_full_name(env->i->dir, config_name);
    if (full_name == 0) {
        r = ENOMEM;
        goto cleanup;
    }
    if ((fp = fopen(full_name, "r")) == NULL) {
        //Config file is optional.
        if (errno == ENOENT) {
            r = EXIT_SUCCESS;
            goto cleanup;
        }
        r = errno;
        goto cleanup;
    }
    //Read each line, applying configuration parameters.
    //After ignoring leading white space, skip any blank lines
    //or comments (starts with #)
    //Command contains no white space.  Value may contain whitespace.
    int linenumber;
    int ch = '\0';
    BOOL eof = FALSE;
    char* temp;
    char* end;
    int index;
    
    buffersize = 1<<10; //1KB
    linebuffer = toku_malloc(buffersize);
    if (!linebuffer) {
        r = ENOMEM;
        goto cleanup;
    }
    for (linenumber = 1; !eof; linenumber++) {
        /* Read a single line. */
        for (index = 0; TRUE; index++) {
            if ((ch = getc(fp)) == EOF) {
                eof = TRUE;
                if (ferror(fp)) {
                    /* Throw away current line and print warning. */
                    r = errno;
                    goto readerror;
                }
                break;
            }
            if (ch == '\n') break;
            if (index + 1 >= buffersize) {
                //Double the buffer.
                buffersize *= 2;
                linebuffer = toku_realloc(linebuffer, buffersize);
                if (!linebuffer) {
                    r = ENOMEM;
                    goto cleanup;
                }
            }
            linebuffer[index] = ch;
        }
        linebuffer[index] = '\0';
        end = &linebuffer[index];

        /* Separate the line into command/value */
        command = linebuffer;
        //Strip leading spaces.
        while (isspace(*command) && command < end) command++;
        //Find end of command.
        temp = command;
        while (!isspace(*temp) && temp < end) temp++;
        *temp++ = '\0'; //Null terminate command.
        value = temp;
        //Strip leading spaces.
        while (isspace(*value) && value < end) value++;
        if (value < end) {
            //Strip trailing spaces.
            temp = end;
            while (isspace(*(temp-1))) temp--;
            //Null terminate value.
            *temp = '\0';
        }
        //Parse the line.
        if (strlen(command) == 0 || command[0] == '#') continue; //Ignore Comments.
        r = db_env_parse_config_line(env, command, value < end ? value : "");
        if (r != 0) goto parseerror;
    }
    if (0) {
readerror:
        env->err(env, r, "Error reading from DB_CONFIG:%d.\n", linenumber);
    }
    if (0) {
parseerror:
        env->err(env, r, "Error parsing DB_CONFIG:%d.\n", linenumber);
    }
cleanup:
    if (full_name) toku_free(full_name);
    if (linebuffer) toku_free(linebuffer);
    if (fp) r2 = fclose(fp);
    return r ? r : r2;
}

static int toku_db_env_open(DB_ENV * env, const char *home, u_int32_t flags, int mode) {
    int r;

    if (db_env_opened(env))
        return EINVAL;

    if ((flags & DB_USE_ENVIRON) && (flags & DB_USE_ENVIRON_ROOT)) return EINVAL;

    if (home) {
        if ((flags & DB_USE_ENVIRON) || (flags & DB_USE_ENVIRON_ROOT)) return EINVAL;
    }
    else if ((flags & DB_USE_ENVIRON) ||
             ((flags & DB_USE_ENVIRON_ROOT) && geteuid() == 0)) home = getenv("DB_HOME");

    if (!home) home = ".";

	// Verify that the home exists.
	{
	struct stat buf;
	r = stat(home, &buf);
	if (r!=0) return errno;
    }



    if (!(flags & DB_PRIVATE)) {
	// There is no good place to send this error message.
	// fprintf(stderr, "tokudb requires DB_PRIVATE\n");
        // This means that we don't have to do anything with shared memory.  
        // And that's good enough for mysql. 
        return EINVAL; 
    }

    if (env->i->dir)
        toku_free(env->i->dir);
    env->i->dir = toku_strdup(home);
    if (env->i->dir == 0) 
        return ENOMEM;
    if (0) {
        died1:
        toku_free(env->i->dir);
        env->i->dir = NULL;
        return r;
    }
    if ((r = db_env_read_config(env)) != 0) goto died1;
    
    env->i->open_flags = flags;
    env->i->open_mode = mode;

    if (flags & (DB_INIT_TXN | DB_INIT_LOG)) {
        char* full_dir = NULL;
        if (env->i->lg_dir) full_dir = construct_full_name(env->i->dir, env->i->lg_dir);
        r = toku_logger_create_and_open_logger(
            full_dir ? full_dir : env->i->dir, &env->i->logger);
        if (full_dir) toku_free(full_dir);
	if (r!=0) goto died1;
	if (0) {
	died2:
	    toku_logger_log_close(&env->i->logger);
	    goto died1;
	}
    }

    r = toku_brt_create_cachetable(&env->i->cachetable, env->i->cachetable_size, ZERO_LSN, env->i->logger);
    if (r!=0) goto died2;
    return 0;
}

static int toku_db_env_close(DB_ENV * env, u_int32_t flags) {
    int r0=0,r1=0;
    if (env->i->cachetable)
        r0=toku_cachetable_close(&env->i->cachetable);
    if (env->i->logger)
        r1=toku_logger_log_close(&env->i->logger);
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
    if (env->i->errpfx)
        toku_free(env->i->errpfx);
    toku_free(env->i->dir);
    toku_free(env->i);
    toku_free(env);
    ydb_unref();
    if (flags!=0) return EINVAL;
    if (r0) return r0;
    if (r1) return r1;
    return 0;
}

static int toku_db_env_log_archive(DB_ENV * env, char **list[], u_int32_t flags) {
    env=env; flags=flags; // Suppress compiler warnings.
    *list = NULL;
    return 0;
}

static int toku_db_env_log_flush(DB_ENV * env, const DB_LSN * lsn) {
    env=env; lsn=lsn;
    barf();
    return 1;
}

static int toku_db_env_set_cachesize(DB_ENV * env, u_int32_t gbytes, u_int32_t bytes, int ncache __attribute__((__unused__))) {
    env->i->cachetable_size = ((long) gbytes << 30) + bytes;
    return 0;
}

static int toku_db_env_set_data_dir(DB_ENV * env, const char *dir) {
    u_int32_t i;
    int r;
    char** temp;
    char* new_dir;
    
    if (db_env_opened(env) || !dir)
        return EINVAL;
    
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
    if (new_dir==NULL) {assert(errno == ENOMEM); return ENOMEM;}
    temp = (char**) toku_realloc(env->i->data_dirs, (1 + env->i->n_data_dirs) * sizeof(char*));
    if (temp==NULL) {assert(errno == ENOMEM); r = ENOMEM; goto died1;}
    else env->i->data_dirs = temp;
    env->i->data_dirs[env->i->n_data_dirs] = new_dir;
    env->i->n_data_dirs++;
    return 0;
}

static void toku_db_env_set_errcall(DB_ENV * env, toku_env_errcall_t errcall) {
    env->i->errcall = errcall;
}

static void toku_db_env_set_errfile(DB_ENV*env, FILE*errfile) {
    env->i->errfile = errfile;
}

static void toku_db_env_set_errpfx(DB_ENV * env, const char *errpfx) {
    if (env->i->errpfx)
        toku_free(env->i->errpfx);
    env->i->errpfx = toku_strdup(errpfx ? errpfx : "");
}

static int toku_db_env_set_flags(DB_ENV * env, u_int32_t flags, int onoff) {
    env=env;
    if (flags != 0 && onoff)
        return EINVAL; /* no flags are currently supported */
    return 0;
}

static int toku_db_env_set_lg_bsize(DB_ENV * env, u_int32_t bsize) {
    env=env; bsize=bsize;
    return 1;
}

static int toku_db_env_set_lg_dir(DB_ENV * env, const char *dir) {
    if (db_env_opened(env)) return EINVAL;

    if (env->i->lg_dir) toku_free(env->i->lg_dir);
    if (dir) {
        env->i->lg_dir = toku_strdup(dir);
        if (!env->i->lg_dir) return ENOMEM;
    }
    else env->i->lg_dir = NULL;
    return 0;
}

static int toku_db_env_set_lg_max(DB_ENV * env, u_int32_t lg_max) {
    env=env; lg_max=lg_max;
    return 1;
}

static int toku_db_env_set_lk_detect(DB_ENV * env, u_int32_t detect) {
    env=env; detect=detect;
    return 1;
}

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
static int toku_db_env_set_lk_max(DB_ENV * env, u_int32_t lk_max) {
    env=env;lk_max=lk_max;
    return 0;
}
#endif

//void __toku_db_env_set_noticecall (DB_ENV *env, void (*noticecall)(DB_ENV *, db_notices)) {
//    env->i->noticecall = noticecall;
//}

static int toku_db_env_set_tmp_dir(DB_ENV * env, const char *tmp_dir) {
    if (db_env_opened(env)) return EINVAL;
    if (!tmp_dir) return EINVAL;
    if (env->i->tmp_dir)
        toku_free(env->i->tmp_dir);
    env->i->tmp_dir = toku_strdup(tmp_dir);
    return env->i->tmp_dir ? 0 : ENOMEM;
}

static int toku_db_env_set_verbose(DB_ENV * env, u_int32_t which, int onoff) {
    env=env; which=which; onoff=onoff;
    return 1;
}

static int toku_db_env_txn_checkpoint(DB_ENV * env, u_int32_t kbyte, u_int32_t min, u_int32_t flags) {
    env=env; kbyte=kbyte; min=min; flags=flags;
    return 0;
}

static int toku_db_env_txn_stat(DB_ENV * env, DB_TXN_STAT ** statp, u_int32_t flags) {
    env=env;statp=statp;flags=flags;
    return 1;
}

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
void toku_default_errcall(const char *errpfx, char *msg) {
#else
void toku_default_errcall(DB_ENV *env, const char *errpfx, const char *msg) {
    env = env;
#endif
    fprintf(stderr, "YDB: %s: %s", errpfx, msg);
}

static int toku_txn_begin(DB_ENV * env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags);

int db_env_create(DB_ENV ** envp, u_int32_t flags) {
    if (flags!=0) return EINVAL;
    DB_ENV *MALLOC(result);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, sizeof *result);
    result->err = toku_db_env_err;
    result->open = toku_db_env_open;
    result->close = toku_db_env_close;
    result->txn_checkpoint = toku_db_env_txn_checkpoint;
    result->log_flush = toku_db_env_log_flush;
    result->set_errcall = toku_db_env_set_errcall;
    result->set_errfile = toku_db_env_set_errfile;
    result->set_errpfx = toku_db_env_set_errpfx;
    //result->set_noticecall = toku_db_env_set_noticecall;
    result->set_flags = toku_db_env_set_flags;
    result->set_data_dir = toku_db_env_set_data_dir;
    result->set_tmp_dir = toku_db_env_set_tmp_dir;
    result->set_verbose = toku_db_env_set_verbose;
    result->set_lg_bsize = toku_db_env_set_lg_bsize;
    result->set_lg_dir = toku_db_env_set_lg_dir;
    result->set_lg_max = toku_db_env_set_lg_max;
    result->set_cachesize = toku_db_env_set_cachesize;
    result->set_lk_detect = toku_db_env_set_lk_detect;
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    result->set_lk_max = toku_db_env_set_lk_max;
#endif
    result->log_archive = toku_db_env_log_archive;
    result->txn_stat = toku_db_env_txn_stat;
    result->txn_begin = toku_txn_begin;

    MALLOC(result->i);
    if (result->i == 0) {
        toku_free(result);
        return ENOMEM;
    }
    memset(result->i, 0, sizeof *result->i);
    result->i->ref_count = 1;
    result->i->errcall = toku_default_errcall;
    result->i->errpfx = toku_strdup("");
    result->i->errfile = 0;

    ydb_add_ref();
    *envp = result;
    return 0;
}

static int toku_db_txn_commit(DB_TXN * txn, u_int32_t flags) {
    //notef("flags=%d\n", flags);
    int r;
    int nosync = (flags & DB_TXN_NOSYNC)!=0;
    flags &= ~DB_TXN_NOSYNC;
    if (!txn) return EINVAL;
    if (flags!=0) goto return_invalid;
    r = toku_logger_commit(txn->i->tokutxn, nosync);
    if (0) {
    return_invalid:
	r = EINVAL;
	toku_free(txn->i->tokutxn);
    }
    // Cleanup */
    if (txn->i)
        toku_free(txn->i);
    toku_free(txn);
    return r; // The txn is no good after the commit.
}

static u_int32_t toku_db_txn_id(DB_TXN * txn) {
    txn=txn;
    barf();
    abort();
}

static TXNID next_txn = 0;

static int toku_txn_abort(DB_TXN * txn) {
    fprintf(stderr, "toku_txn_abort(%p)\n", txn);
    abort();
}

static int toku_txn_begin(DB_ENV * env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags) {
    if (!env->i->logger) return EINVAL;
    flags=flags;
    DB_TXN *MALLOC(result);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, sizeof *result);
    //notef("parent=%p flags=0x%x\n", stxn, flags);
    result->mgrp = env;
    result->abort = toku_txn_abort;
    result->commit = toku_db_txn_commit;
    result->id = toku_db_txn_id;
    MALLOC(result->i);
    assert(result->i);
    result->i->parent = stxn;
    int r = toku_logger_txn_begin(stxn ? stxn->i->tokutxn : 0, &result->i->tokutxn, next_txn++, env->i->logger);
    if (r != 0)
        return r;
    *txn = result;
    return 0;
}

#if 0
int txn_commit(DB_TXN * txn, u_int32_t flags) {
    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
    return toku_logger_log_commit(txn->i->tokutxn);
}
#endif

int log_compare(const DB_LSN * a, const DB_LSN * b) {
    fprintf(stderr, "%s:%d log_compare(%p,%p)\n", __FILE__, __LINE__, a, b);
    abort();
}

static int maybe_do_associate_create (DB_TXN*txn, DB*primary, DB*secondary) {
    DBC *dbc;
    int r = secondary->cursor(secondary, txn, &dbc, 0);
    if (r!=0) return r;
    DBT key,data;
    r = dbc->c_get(dbc, &key, &data, DB_FIRST);
    {
	int r2=dbc->c_close(dbc);
	if (r!=DB_NOTFOUND) {
	    return r2;
	}
    }
    /* Now we know the secondary is empty. */
    r = primary->cursor(primary, txn, &dbc, 0);
    if (r!=0) return r;
    for (r = dbc->c_get(dbc, &key, &data, DB_FIRST);
	 r==0;
	 r = dbc->c_get(dbc, &key, &data, DB_NEXT)) {
	r = do_associated_inserts(txn, &key, &data, secondary);
	if (r!=0) {
	    dbc->c_close(dbc);
	    return r;
	}
    }
    return 0;
}

static int toku_db_associate (DB *primary, DB_TXN *txn, DB *secondary,
			      int (*callback)(DB *secondary, const DBT *key, const DBT *data, DBT *result),
			      u_int32_t flags) {
    unsigned int brtflags;
    
    if (secondary->i->primary) return EINVAL; // The secondary already has a primary
    if (primary->i->primary)   return EINVAL; // The primary already has a primary

    toku_brt_get_flags(primary->i->brt, &brtflags);
    if (brtflags & TOKU_DB_DUPSORT) return EINVAL;  //The primary may not have duplicate keys.
    if (brtflags & TOKU_DB_DUP)     return EINVAL;  //The primary may not have duplicate keys.

    if (!list_empty(&secondary->i->associated)) return EINVAL; // The secondary is in some list (or it is a primary)
    assert(secondary->i->associate_callback==0);      // Something's wrong if this isn't null we made it this far.
    secondary->i->associate_callback = callback;
#ifdef DB_IMMUTABLE_KEY
    secondary->i->associate_is_immutable = (DB_IMMUTABLE_KEY&flags)!=0;
    flags &= ~DB_IMMUTABLE_KEY;
#else
    secondary->i->associate_is_immutable = 0;
#endif
    if (flags!=0 && flags!=DB_CREATE) return EINVAL; // after removing DB_IMMUTABLE_KEY the flags better be 0 or DB_CREATE
    list_push(&primary->i->associated, &secondary->i->associated);
    secondary->i->primary = primary;
    if (flags==DB_CREATE) {
	// To do this:  If the secondary is empty, then open a cursor on the primary.  Step through it all, doing the callbacks.
	// Then insert each callback result into the secondary.
	return maybe_do_associate_create(txn, primary, secondary);
    }
    return 0;
}

static int toku_db_close(DB * db, u_int32_t flags) {
    if (db->i->primary==0) {
	// It is a primary.  Unlink all the secondaries. */
	while (!list_empty(&db->i->associated)) {
	    assert(list_struct(list_head(&db->i->associated),
			       struct __toku_db_internal,
			       associated)->primary==db);
	    list_remove(list_head(&db->i->associated));
	}
    } else {
	// It is a secondary.  Remove it from the list, (which it must be in .*/
	if (!list_empty(&db->i->associated)) {
	    list_remove(&db->i->associated);
	}
    }
    flags=flags;
    int r = toku_close_brt(db->i->brt);
    if (r != 0)
        return r;
    // printf("%s:%d %d=__toku_db_close(%p)\n", __FILE__, __LINE__, r, db);
    db_env_unref(db->dbenv);
    toku_free(db->i->database_name);
    toku_free(db->i->full_fname);
    toku_free(db->i);
    toku_free(db);
    ydb_unref();
    return r;
}

struct __toku_dbc_internal {
    BRT_CURSOR c;
    DB_TXN *txn;
};

static int verify_secondary_key(DB *secondary, DBT *pkey, DBT *data, DBT *skey) {
    int r = 0;
    DBT idx;

    assert(secondary->i->primary != 0);
    memset(&idx, 0, sizeof(idx));
    secondary->i->associate_callback(secondary, pkey, data, &idx);
    if (r==DB_DONOTINDEX) return DB_SECONDARY_BAD;
#ifdef DB_DBT_MULTIPLE
    if (idx.flags & DB_DBT_MULTIPLE) {
        return EINVAL; // We aren't ready for this
    }
#endif
	if (skey->size != idx.size || memcmp(skey->data, idx.data, idx.size) != 0) r = DB_SECONDARY_BAD;
    if (idx.flags & DB_DBT_APPMALLOC) {
    	free(idx.data);
    }
    return r;
}

static int toku_c_get_noassociate(DBC * c, DBT * key, DBT * data, u_int32_t flag) {
    int r = toku_brt_cursor_get(c->i->c, key, data, flag, c->i->txn ? c->i->txn->i->tokutxn : 0);
    return r;
}

static int toku_c_del_noassociate(DBC * c, u_int32_t flags) {
    int r;
    
    r = toku_brt_cursor_delete(c->i->c, flags);
    return r;
}

//Get the main portion of a cursor flag (excluding the bitwise or'd components).
static int get_main_cursor_flag(u_int32_t flag) {
#ifdef DB_READ_UNCOMMITTED
    flag &= ~DB_READ_UNCOMMITTED;
#endif    
#ifdef DB_MULTIPLE
    flag &= ~DB_MULTIPLE;
#endif
#ifdef DB_MULTIPLE_KEY
    flag &= ~DB_MULTIPLE_KEY;
#endif    
    flag &= ~DB_RMW;
    return flag;
}

static int toku_c_pget_save_original_data(DBT* dst, DBT* src) {
    int r;
    
    *dst = *src;
#ifdef DB_DBT_PARTIAL
#error toku_c_pget does not properly handle DB_DBT_PARTIAL
#endif
    //We may use this multiple times, we'll free only once at the end.
    dst->flags = DB_DBT_REALLOC;
    //Not using DB_DBT_USERMEM.
    dst->ulen = 0;
    if (src->size) {
        if (!src->data) return EINVAL;
        dst->data = toku_malloc(src->size);
        if (!dst->data) {
            r = ENOMEM;
            return r;
        }
        memcpy(dst->data, src->data, src->size);
    }
    else dst->data = NULL;
    return 0;
}

static int toku_c_pget(DBC * c, DBT *key, DBT *pkey, DBT *data, u_int32_t flag) {
    int r;
    int r2;
    int r3;
    DB *db = c->dbp;
    DB *pdb = db->i->primary;
    
    

    if (!pdb) return EINVAL;  //c_pget does not work on a primary.
	// If data and primary_key are both zeroed, the temporary storage used to fill in data is different in the two cases because they come from different trees.
	assert(db->i->brt!=pdb->i->brt); // Make sure they realy are different trees.
    assert(db!=pdb);

    DBT copied_key;
    DBT copied_pkey;
    DBT copied_data;
    //Store original pointers.
    DBT* o_key = key;
    DBT* o_pkey = pkey;
    DBT* o_data = data;
    //Use copied versions for everything until/if success.
    key  = &copied_key;
    pkey = &copied_pkey;
    data = &copied_data;

    if (0) {
delete_silently_and_retry:
        //Free any old data.
        free(key->data);
        free(pkey->data);
        free(data->data);
        //Silently delete and re-run.
        r = toku_c_del_noassociate(c, 0);
        if (r != 0) return r;
    }
    if (0) {
        died0:
        return r;
    }
    //Need to save all the original data.
    r = toku_c_pget_save_original_data(&copied_key, o_key);   if (r!=0) goto died0;
    if (0) {
        died1:
        free(key->data);
        goto died0;
    }
    r = toku_c_pget_save_original_data(&copied_pkey, o_pkey); if (r!=0) goto died1;
    if (0) {
        died2:
        free(pkey->data);
        goto died1;
    }
    r = toku_c_pget_save_original_data(&copied_data, o_data); if (r!=0) goto died2;
    if (0) {
        died3:
        free(data->data);
        goto died2;
    }

    r = toku_c_get_noassociate(c, key, pkey, flag);
    if (r != 0) goto died3;
    r = pdb->get(pdb, c->i->txn, pkey, data, 0);
    if (r == DB_NOTFOUND)   goto delete_silently_and_retry;
    if (r != 0) goto died3;
    r = verify_secondary_key(db, pkey, data, key);
    if (r != 0)             goto delete_silently_and_retry;

    //Copy everything and return.
    assert(r==0);

    r  = toku_brt_dbt_set_key(db->i->brt,  o_key,  key->data,  key->size);
    r2 = toku_brt_dbt_set_key(pdb->i->brt, o_pkey, pkey->data, pkey->size);
    r3 = toku_brt_dbt_set_value(pdb->i->brt, o_data, data->data, data->size);

    //Cleanup.
    free(key->data);
    free(pkey->data);
    free(data->data);
    if (r!=0) return r;
    if (r2!=0) return r2;
    return r3;
}

static int toku_c_get(DBC * c, DBT * key, DBT * data, u_int32_t flag) {
    DB *db = c->dbp;
    int r;

    if (db->i->primary==0) r = toku_c_get_noassociate(c, key, data, flag);
    else {
        // It's a c_get on a secondary.
        DBT primary_key;
        u_int32_t get_flag = get_main_cursor_flag(flag);
        
        /* It is an error to use the DB_GET_BOTH or DB_GET_BOTH_RANGE flag on a
         * cursor that has been opened on a secondary index handle.
         */
        if ((get_flag == DB_GET_BOTH)
#ifdef DB_GET_BOTH_RANGE
            || (get_flag == DB_GET_BOTH_RANGE)
#endif
        ) return EINVAL;
        memset(&primary_key, 0, sizeof(primary_key));
        r = toku_c_pget(c, key, &primary_key, data, flag);
    }
    return r;
}

static int toku_c_close(DBC * c) {
    int r = toku_brt_cursor_close(c->i->c);
    toku_free(c->i);
    toku_free(c);
    return r;
}

static int toku_db_get_noassociate(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    int r;
    unsigned int brtflags;
    if (flags!=0 && flags!=DB_GET_BOTH) return EINVAL;
    
    toku_brt_get_flags(db->i->brt, &brtflags);
    if ((brtflags & TOKU_DB_DUPSORT) || flags == DB_GET_BOTH) {

        if (flags != 0 && flags != DB_GET_BOTH) return EINVAL;
        // We aren't ready to handle flags such as DB_READ_COMMITTED or DB_READ_UNCOMMITTED or DB_RMW
        
        DBC *dbc;
        r = db->cursor(db, txn, &dbc, 0);
        if (r!=0) return r;
        r = toku_c_get_noassociate(dbc, key, data, flags == DB_GET_BOTH ? DB_GET_BOTH : DB_SET);
        int r2 = dbc->c_close(dbc);
        if (r!=0) return r;
        return r2;
    } else {
        if (flags != 0) return EINVAL;
        return toku_brt_lookup(db->i->brt, key, data);
    }
}

static int toku_db_del_noassociate(DB * db, DB_TXN * txn, DBT * key, u_int32_t flags) {
    int r;
    if (flags!=0 && flags!=DB_DELETE_ANY) return EINVAL;
    //DB_DELETE_ANY supresses the BDB DB->del return value indicating that the key was not found prior to the delete
    if (!(flags & DB_DELETE_ANY)) {
        DBT search_val; memset(&search_val, 0, sizeof search_val); 
        search_val.flags = DB_DBT_MALLOC;
        r = toku_db_get_noassociate(db, txn, key, &search_val, 0);
        if (r != 0)
            return r;
        free(search_val.data);
    } 
    //Do the actual deleting.
    r = toku_brt_delete(db->i->brt, key);
    return r;
}

static int do_associated_deletes(DB_TXN *txn, DBT *key, DBT *data, DB *secondary) {
    u_int32_t brtflags;
    DBT idx;
    memset(&idx, 0, sizeof(idx));
    int r = secondary->i->associate_callback(secondary, key, data, &idx);
    int r2 = 0;
    if (r==DB_DONOTINDEX) return 0;
#ifdef DB_DBT_MULTIPLE
    if (idx.flags & DB_DBT_MULTIPLE) {
        return EINVAL; // We aren't ready for this
    }
#endif
	toku_brt_get_flags(secondary->i->brt, &brtflags);
    if ((brtflags & TOKU_DB_DUPSORT) || (brtflags & TOKU_DB_DUP)) {
        //If the secondary has duplicates we need to use cursor deletes.
	    DBC *dbc;
	    r = secondary->cursor(secondary, txn, &dbc, 0);
	    if (r!=0) goto cleanup;
	    r = toku_c_get_noassociate(dbc, &idx, key, DB_GET_BOTH);
	    if (r!=0) goto cleanup;
	    r = toku_c_del_noassociate(dbc, 0);
cleanup:
        r2 = dbc->c_close(dbc);
    }
    else r = toku_db_del_noassociate(secondary, txn, &idx, DB_DELETE_ANY);
    if (idx.flags & DB_DBT_APPMALLOC) {
    	free(idx.data);
    }
    if (r!=0) return r;
    return r2;
}

static int toku_c_del(DBC * c, u_int32_t flags) {
    int r;
    DB* db = c->dbp;
    
    //It is a primary with secondaries, or is a secondary.
    if (db->i->primary != 0 || !list_empty(&db->i->associated)) {
        DB* pdb;
        DBT pkey;
        DBT data;
        struct list *h;

        memset(&pkey, 0, sizeof(pkey));
        memset(&data, 0, sizeof(data));
        if (db->i->primary == 0) {
            pdb = db;
            r = c->c_get(c, &pkey, &data, DB_CURRENT);
        }
        else {
            DBT skey;
            pdb = db->i->primary;
            memset(&skey, 0, sizeof(skey));
            r = toku_c_pget(c, &skey, &pkey, &data, DB_CURRENT);
        }
        if (r != 0) return r;
        
    	for (h = list_head(&pdb->i->associated); h != &pdb->i->associated; h = h->next) {
    	    struct __toku_db_internal *dbi = list_struct(h, struct __toku_db_internal, associated);
    	    if (dbi->db == db) continue;  //Skip current db (if its primary or secondary)
    	    r = do_associated_deletes(c->i->txn, &pkey, &data, dbi->db);
    	    if (r!=0) return r;
    	}
    	if (db->i->primary != 0) {
    	    //If this is a secondary, we did not delete from the primary.
    	    //Primaries cannot have duplicates, (noncursor) del is safe.
    	    r = toku_db_del_noassociate(pdb, c->i->txn, &pkey, DB_DELETE_ANY);
    	    if (r!=0) return r;
    	}
    }
    r = toku_c_del_noassociate(c, flags);
    return r;    
}

static int toku_c_put(DBC *dbc, DBT *key, DBT *data, u_int32_t flags) {
    DB* db = dbc->dbp;
    unsigned int brtflags;
    int r;
    DBT* put_key  = key;
    DBT* put_data = data;
    DBT* get_key  = key;
    DBT* get_data = data;
    
    //Cannot c_put in a secondary index.
    if (db->i->primary!=0) return EINVAL;
    toku_brt_get_flags(db->i->brt, &brtflags);
    //We do not support duplicates without sorting.
    if (!(brtflags & TOKU_DB_DUPSORT) && (brtflags & TOKU_DB_DUP)) return EINVAL;
    
    if (flags==DB_CURRENT) {
        DBT key_local;
        DBT data_local;
        memset(&key_local, 0, sizeof(DBT));
        memset(&data_local, 0, sizeof(DBT));
        //Can't afford to overwrite the local storage.
        key_local.flags = DB_DBT_MALLOC;
        data_local.flags = DB_DBT_MALLOC;
        r = toku_c_get(dbc, &key_local, &data_local, DB_CURRENT);
        if (0) {
            cleanup:
            if (flags==DB_CURRENT) {
                free(key_local.data);
                free(data_local.data);
            }
            return r;
        }
        if (r==DB_KEYEMPTY) return DB_NOTFOUND;
        if (r!=0) return r;
        if (brtflags & TOKU_DB_DUPSORT) {
            r = db->i->brt->dup_compare(db, &data_local, data);
            if (r!=0) {r = EINVAL; goto cleanup;}
        }
        //Remove old pair.
        r = toku_c_del(dbc, 0);
        if (r!=0) goto cleanup;
        get_key = put_key  = &key_local;
        goto finish;
    }
    else if (flags==DB_KEYFIRST || flags==DB_KEYLAST) {
        goto finish;        
    }
    else if (flags==DB_NODUPDATA) {
        //Must support sorted duplicates.
        if (!(brtflags & TOKU_DB_DUPSORT)) return EINVAL;
        r = toku_c_get(dbc, key, data, DB_GET_BOTH);
        if (r==0) return DB_KEYEXIST;
        if (r!=DB_NOTFOUND) return r;
        goto finish;
    }
    //Flags must NOT be 0.
    else return EINVAL;
finish:
    //Insert new data with the key we got from c_get.
    r = db->put(db, dbc->i->txn, put_key, put_data, DB_YESOVERWRITE); // when doing the put, it should do an overwrite.
    if (r!=0) goto cleanup;
    r = toku_c_get(dbc, get_key, get_data, DB_GET_BOTH);
    goto cleanup;
}

static int toku_db_cursor(DB * db, DB_TXN * txn, DBC ** c, u_int32_t flags) {
    if (flags != 0)
        return EINVAL;
    DBC *MALLOC(result);
    if (result == 0)
        return ENOMEM;
    memset(result, 0, sizeof *result);
    result->c_get = toku_c_get;
    result->c_pget = toku_c_pget;
    result->c_put = toku_c_put;
    result->c_close = toku_c_close;
    result->c_del = toku_c_del;
    MALLOC(result->i);
    assert(result->i);
    result->dbp = db;
    result->i->txn = txn;
    int r = toku_brt_cursor(db->i->brt, &result->i->c);
    assert(r == 0);
    *c = result;
    return 0;
}

static int toku_db_del(DB * db, DB_TXN * txn, DBT * key, u_int32_t flags) {
    int r;

    //It is a primary with secondaries, or is a secondary.
    if (db->i->primary != 0 || !list_empty(&db->i->associated)) {
        DB* pdb;
        DBT data;
        DBT pkey;
        DBT *pdb_key;
        struct list *h;
        u_int32_t brtflags;

        memset(&data, 0, sizeof(data));

        toku_brt_get_flags(db->i->brt, &brtflags);
        if ((brtflags & TOKU_DB_DUPSORT) || (brtflags & TOKU_DB_DUP)) {
            int r2;
    	    DBC *dbc;
    	    BOOL found = FALSE;

            /* If we are deleting all copies from a secondary with duplicates,
             * We have to make certain we cascade all the deletes. */

            assert(db->i->primary!=0);    //Primary cannot have duplicates.
            r = db->cursor(db, txn, &dbc, 0);
            if (r!=0) goto cleanup;
            r = toku_c_get_noassociate(dbc, key, &data, DB_SET);
            if (r!=0) goto cleanup;
            
            while (r==0) {
                r = dbc->c_del(dbc, 0);
                if (r==0) found = TRUE;
                if (r!=0 && r!=DB_KEYEMPTY) goto cleanup;
                r = toku_c_get_noassociate(dbc, key, &data, DB_NEXT_DUP);
                if (r == DB_NOTFOUND) {
                    //If we deleted at least one we're happy.  Quit out.
                    if (found) r = 0;
                    goto cleanup;
                }
            }
cleanup:
            r2 = dbc->c_close(dbc);
            if (r != 0) return r;
            return r2;
        }

        if (db->i->primary == 0) {
            pdb = db;
            r = db->get(db, txn, key, &data, 0);
            pdb_key = key;
        }
        else {
            memset(&pkey, 0, sizeof(pkey));
            pdb = db->i->primary;
            r = db->pget(db, txn, key, &pkey, &data, 0);
            pdb_key = &pkey;
        }
        if (r != 0) return r;
        
    	for (h = list_head(&pdb->i->associated); h != &pdb->i->associated; h = h->next) {
    	    struct __toku_db_internal *dbi = list_struct(h, struct __toku_db_internal, associated);
    	    if (dbi->db == db) continue;                  //Skip current db (if its primary or secondary)
    	    r = do_associated_deletes(txn, pdb_key, &data, dbi->db);
    	    if (r!=0) return r;
    	}
    	if (db->i->primary != 0) {
    	    //If this is a secondary, we did not delete from the primary.
    	    //Primaries cannot have duplicates, (noncursor) del is safe.
    	    r = toku_db_del_noassociate(pdb, txn, pdb_key, DB_DELETE_ANY);
    	    if (r!=0) return r;
    	}
    	//We know for certain it was already found, so no need to return DB_NOTFOUND.
    	flags |= DB_DELETE_ANY;
    }
    r = toku_db_del_noassociate(db, txn, key, flags);
    return r;
}

static int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    int r;

    if (db->i->primary==0) r = toku_db_get_noassociate(db, txn, key, data, flags);
    else {
        // It's a get on a secondary.
        assert(flags == 0); // We aren't ready to handle flags such as DB_GET_BOTH  or DB_READ_COMMITTED or DB_READ_UNCOMMITTED or DB_RMW
        DBT primary_key;
        memset(&primary_key, 0, sizeof(primary_key));
        r = db->pget(db, txn, key, &primary_key, data, 0);
    }
    return r;
}

static int toku_db_pget (DB *db, DB_TXN *txn, DBT *key, DBT *pkey, DBT *data, u_int32_t flags) {
    int r;
    int r2;
    DBC *dbc;
    if (!db->i->primary) return EINVAL; // pget doesn't work on a primary.
    assert(flags==0); // not ready to handle all those other options
	assert(db->i->brt != db->i->primary->i->brt); // Make sure they realy are different trees.
    assert(db!=db->i->primary);

    r = db->cursor(db, txn, &dbc, 0);
    if (r!=0) return r;
    r = dbc->c_pget(dbc, key, pkey, data, DB_SET);
    if (r==DB_KEYEMPTY) r = DB_NOTFOUND;
    r2 = dbc->c_close(dbc);
    if (r!=0) return r;
    return r2;    
}

static int toku_db_key_range(DB * db, DB_TXN * txn, DBT * dbt, DB_KEY_RANGE * kr, u_int32_t flags) {
    db=db; txn=txn; dbt=dbt; kr=kr; flags=flags;
    barf();
    abort();
}

static int construct_full_name_in_buf(const char *dir, const char *fname, char* full, int length) {
    int l;

    if (!full) return EINVAL;
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
    l += snprintf(full + l, length - l, "%s", fname);
    if (l >= length) return ENAMETOOLONG;
    return 0;
}

static char *construct_full_name(const char *dir, const char *fname) {
    if (fname[0] == '/')
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
    struct stat statbuf;
    char* full_name;
    
    assert(full_name_out);    
    if (dbenv->i->data_dirs!=NULL) {
        assert(dbenv->i->n_data_dirs > 0);
        for (i = 0; i < dbenv->i->n_data_dirs; i++) {
            full_name = construct_full_name(dbenv->i->data_dirs[0], fname);
            if (!full_name) return ENOMEM;
            r = stat(full_name, &statbuf);
            if (r == 0) goto finish;
            else {
                toku_free(full_name);
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

// The decision to embedded subdatabases in files is a little bit painful.
// My original design was to simply create another file, but it turns out that we 
//  have to inherit mode bits and so forth from the first file that was created.
// Other problems may ensue (who is responsible for deleting the file?  That's not so bad actually.)
// This suggests that we really need to put the multiple databases into one file.
static int toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    // Warning.  Should check arguments.  Should check return codes on malloc and open and so forth.

    int openflags = 0;
    int r;
    if (dbtype!=DB_BTREE && dbtype!=DB_UNKNOWN) return EINVAL;
    int is_db_excl    = flags & DB_EXCL;    flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  flags&=~DB_CREATE;
    int is_db_rdonly  = flags & DB_RDONLY;  flags&=~DB_RDONLY;
    int is_db_unknown = flags & DB_UNKNOWN; flags&=~DB_UNKNOWN;
    if (flags) return EINVAL; // unknown flags

    if (is_db_excl && !is_db_create) return EINVAL;
    if (dbtype==DB_UNKNOWN && is_db_excl) return EINVAL;

    if (db_opened(db))
        return EINVAL;              /* It was already open. */
    
    r = find_db_file(db->dbenv, fname, &db->i->full_fname);
    if (r != 0) goto error_cleanup;
    // printf("Full name = %s\n", db->i->full_fname);
    db->i->database_name = toku_strdup(dbname ? dbname : "");
    if (db->i->database_name == 0) {
        r = ENOMEM;
        goto error_cleanup;
    }
    if (is_db_rdonly)
        openflags |= O_RDONLY;
    else
        openflags |= O_RDWR;
    
    {
        struct stat statbuf;
        if (stat(db->i->full_fname, &statbuf) == 0) {
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

    r = toku_brt_open(db->i->brt, db->i->full_fname, fname, dbname,
		      is_db_create, is_db_excl, is_db_unknown,
		      db->dbenv->i->cachetable,
		      txn ? txn->i->tokutxn : NULL_TXN);
    if (r != 0)
        goto error_cleanup;

    return 0;
 
error_cleanup:
    if (db->i->database_name) {
        toku_free(db->i->database_name);
        db->i->database_name = NULL;
    }
    if (db->i->full_fname) {
        toku_free(db->i->full_fname);
        db->i->full_fname = NULL;
    }
    return r;
}

static int toku_db_put_noassociate(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    int r;

    unsigned int brtflags;
    r = toku_brt_get_flags(db->i->brt, &brtflags); assert(r == 0);

    /* limit the size of key and data */
    unsigned int nodesize;
    r = toku_brt_get_nodesize(db->i->brt, &nodesize); assert(r == 0);
    if (brtflags & TOKU_DB_DUPSORT) {
        unsigned int limit = nodesize / (2*BRT_FANOUT-1);
        if (key->size + data->size >= limit)
            return EINVAL;
    } else {
        unsigned int limit = nodesize / (3*BRT_FANOUT-1);
        if (key->size >= limit || data->size >= limit)
            return EINVAL;
    }

    if (flags == DB_YESOVERWRITE) {
        /* tokudb does insert or replace */
        ;
    } else if (flags == DB_NOOVERWRITE) {
        /* check if the key already exists */
        DBT testfordata;
        r = toku_db_get_noassociate(db, txn, key, toku_init_dbt(&testfordata), 0);
        if (r == 0)
            return DB_KEYEXIST;
    } else if (flags != 0) {
        /* no other flags are currently supported */
        return EINVAL;
    } else {
        assert(flags == 0);
        if (brtflags & TOKU_DB_DUPSORT) {
#if TDB_EQ_BDB
            r = toku_db_get_noassociate(db, txn, key, data, DB_GET_BOTH);
            if (r == 0)
                return DB_KEYEXIST;
#else
	    do_error(db->dbenv, "Tokudb requires that db->put specify DB_YESOVERWRITE or DB_NOOVERWRITE on DB_DUPSORT databases");
            return EINVAL;
#endif
        }
    }
    
    r = toku_brt_insert(db->i->brt, key, data, txn ? txn->i->tokutxn : 0);
    //printf("%s:%d %d=__toku_db_put(...)\n", __FILE__, __LINE__, r);
    return r;
}

static int do_associated_inserts (DB_TXN *txn, DBT *key, DBT *data, DB *secondary) {
    DBT idx;
    memset(&idx, 0, sizeof(idx));
    int r = secondary->i->associate_callback(secondary, key, data, &idx);
    if (r==DB_DONOTINDEX) return 0;
#ifdef DB_DBT_MULTIPLE
    if (idx.flags & DB_DBT_MULTIPLE) {
	return EINVAL; // We aren't ready for this
    }
#endif
    r = toku_db_put_noassociate(secondary, txn, &idx, key, DB_YESOVERWRITE);
    if (idx.flags & DB_DBT_APPMALLOC) {
        free(idx.data);
    }
    return r;
}

static int toku_db_put(DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    int r;

    //Cannot put directly into a secondary.
    if (db->i->primary != 0) return EINVAL;

    r = toku_db_put_noassociate(db, txn, key, data, flags);
    if (r!=0) return r;
    // For each secondary add the relevant records.
    assert(db->i->primary==0);
    // Only do it if it is a primary.   This loop would run an unknown number of times if we tried it on a secondary.
    struct list *h;
    for (h=list_head(&db->i->associated); h!=&db->i->associated; h=h->next) {
        struct __toku_db_internal *dbi=list_struct(h, struct __toku_db_internal, associated);
        r=do_associated_inserts(txn, key, data, dbi->db);
        if (r!=0) return r;
    }
    return 0;
}

static int toku_db_remove(DB * db, const char *fname, const char *dbname, u_int32_t flags) {
    int r;
    int r2;
    char *full_name;

    //TODO: Verify DB* db not yet opened
    if (dbname) {
        //TODO: Verify the target db is not open
        //TODO: Use master database (instead of manual edit) when implemented.

        if ((r = db->open(db, NULL, fname, dbname, DB_BTREE, 0, 0777)) != 0) goto cleanup;
        r = toku_brt_remove_subdb(db->i->brt, dbname, flags);
cleanup:
        r2 = db->close(db, 0);
        return r ? r : r2;
    }
    //TODO: Verify db file not in use. (all dbs in the file must be unused)
    r = find_db_file(db->dbenv, fname, &full_name);
    if (r!=0) return r;
    assert(full_name);
    r2 = db->close(db, 0);
    if (r == 0 && r2 == 0) {
        if (unlink(full_name) != 0) r = errno;
    }
    toku_free(full_name);
    return r ? r : r2;
}

static int toku_db_rename(DB * db, const char *namea, const char *nameb, const char *namec, u_int32_t flags) {
    if (flags!=0) return EINVAL;
    char afull[PATH_MAX], cfull[PATH_MAX];
    int r;
    assert(nameb == 0);
    r = snprintf(afull, PATH_MAX, "%s%s", db->dbenv->i->dir, namea);
    assert(r < PATH_MAX);
    r = snprintf(cfull, PATH_MAX, "%s%s", db->dbenv->i->dir, namec);
    assert(r < PATH_MAX);
    return rename(afull, cfull);
}

static int toku_db_set_bt_compare(DB * db, int (*bt_compare) (DB *, const DBT *, const DBT *)) {
    int r = toku_brt_set_bt_compare(db->i->brt, bt_compare);
    return r;
}

static int toku_db_set_dup_compare(DB *db, int (*dup_compare)(DB *, const DBT *, const DBT *)) {
    int r = toku_brt_set_dup_compare(db->i->brt, dup_compare);
    return r;
}

static void toku_db_set_errfile (DB*db, FILE *errfile) {
    db->dbenv->set_errfile(db->dbenv, errfile);
}

static int toku_db_set_flags(DB * db, u_int32_t flags) {

    /* the following matches BDB */
    if (db_opened(db) && flags != 0) return EINVAL;

    u_int32_t tflags;
    int r = toku_brt_get_flags(db->i->brt, &tflags);
    if (r!=0) return r;
    
    /* we support no duplicates and sorted duplicates */
    if (flags) {
        if (flags != (DB_DUP + DB_DUPSORT))
            return EINVAL;
        tflags += TOKU_DB_DUP + TOKU_DB_DUPSORT;
    }
    r = toku_brt_set_flags(db->i->brt, tflags);
    return r;
}

static int toku_db_get_flags(DB *db, u_int32_t *pflags) {
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
    int r = toku_brt_set_nodesize(db->i->brt, pagesize);
    return r;
}

static int toku_db_stat(DB * db, void *v, u_int32_t flags) {
    db=db; v=v; flags=flags;
    barf();
    abort();
}

int db_create(DB ** db, DB_ENV * env, u_int32_t flags) {
    int r;

    if (flags) return EINVAL;

    /* if the env already exists then add a ref to it
       otherwise create one */
    if (env) {
        if (!db_env_opened(env))
            return EINVAL;
        db_env_add_ref(env);
    } else {
        r = db_env_create(&env, 0);
        if (r != 0)
            return r;
        r = env->open(env, ".", DB_PRIVATE + DB_INIT_MPOOL, 0);
        if (r != 0) {
            env->close(env, 0);
            return r;
        }
        assert(db_env_opened(env));
    }
    
    DB *MALLOC(result);
    if (result == 0) {
        db_env_unref(env);
        return ENOMEM;
    }
    memset(result, 0, sizeof *result);
    result->dbenv = env;
    result->associate = toku_db_associate;
    result->close = toku_db_close;
    result->cursor = toku_db_cursor;
    result->del = toku_db_del;
    result->get = toku_db_get;
    result->key_range = toku_db_key_range;
    result->open = toku_db_open;
    result->pget = toku_db_pget;
    result->put = toku_db_put;
    result->remove = toku_db_remove;
    result->rename = toku_db_rename;
    result->set_bt_compare = toku_db_set_bt_compare;
    result->set_dup_compare = toku_db_set_dup_compare;
    result->set_errfile = toku_db_set_errfile;
    result->set_pagesize = toku_db_set_pagesize;
    result->set_flags = toku_db_set_flags;
    result->get_flags = toku_db_get_flags;
    result->stat = toku_db_stat;
    MALLOC(result->i);
    if (result->i == 0) {
        toku_free(result);
        db_env_unref(env);
        return ENOMEM;
    }
    memset(result->i, 0, sizeof *result->i);
    result->i->db = result;
    result->i->freed = 0;
    result->i->header = 0;
    result->i->database_number = 0;
    result->i->full_fname = 0;
    result->i->database_name = 0;
    result->i->open_flags = 0;
    result->i->open_mode = 0;
    result->i->brt = 0;
    list_init(&result->i->associated);
    result->i->primary = 0;
    result->i->associate_callback = 0;
    r = toku_brt_create(&result->i->brt);
    if (r != 0) {
        toku_free(result->i);
        toku_free(result);
        db_env_unref(env);
        return ENOMEM;
    }
    ydb_add_ref();
    *db = result;
    return 0;
}

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

    static char unknown_result[100];    // Race condition if two threads call this at the same time.    However even in a bad case, it should be some sort of nul-terminated string.
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
    return DB_VERSION_STRING;
}
