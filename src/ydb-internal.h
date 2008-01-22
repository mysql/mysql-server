#ifndef YDB_INTERNAL_H
#define YDB_INTERNAL_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "../include/db.h"
#include "../newbrt/brttypes.h"
#include "../newbrt/brt.h"
#include "../newbrt/list.h"

struct db_header {
    int n_databases; // Or there can be >=1 named databases.  This is the count.
    char *database_names; // These are the names
    BRT  *database_brts;  // These 
};

struct __toku_db_internal {
    DB *db; // A pointer back to the DB.
    int freed;
    struct db_header *header;
    int database_number; // -1 if it is the single unnamed database.  Nonnengative number otherwise.
    char *full_fname;
    char *database_name;
    //int fd;
    u_int32_t open_flags;
    int open_mode;
    BRT brt;
    FILENUM fileid;
    struct list associated; // All the associated databases.  The primary is the head of the list.
    DB *primary;            // For secondary (associated) databases, what is the primary?  NULL if not a secondary.
    int(*associate_callback)(DB*, const DBT*, const DBT*, DBT*); // For secondary, the callback function for associate.  NULL if not secondary
    int associate_is_immutable; // If this DB is a secondary then this field indicates that the index never changes due to updates.
};

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
typedef void (*toku_env_errcall_t)(const char *, char *);
#elif DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
typedef void (*toku_env_errcall_t)(const DB_ENV *, const char *, const char *);
#else
#error
#endif

struct __toku_db_env_internal {
    int is_panicked;
    int ref_count;
    u_int32_t open_flags;
    int open_mode;
    toku_env_errcall_t errcall;
    void *errfile;
    const char *errpfx;
    char *dir;                  /* A malloc'd copy of the directory. */
    char *tmp_dir;
    char *lg_dir;
    char **data_dirs;
    u_int32_t n_data_dirs;
    //void (*noticecall)(DB_ENV *, db_notices);
    unsigned long cachetable_size;
    CACHETABLE cachetable;
    TOKULOGGER logger;
};

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    TOKUTXN tokutxn;
    DB_TXN *parent;
};

struct __toku_dbc_internal {
    BRT_CURSOR c;
    DB_TXN *txn;
};

#endif
