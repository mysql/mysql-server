#ifndef YDB_INTERNAL_H
#define YDB_INTERNAL_H
#include "../include/db.h"
#include "../newbrt/brttypes.h"
#include "../newbrt/brt.h"


struct db_header {
    int n_databases; // Or there can be >=1 named databases.  This is the count.
    char *database_names; // These are the names
    BRT  *database_brts;  // These 
};

struct __toku_db_internal {
    int freed;
    int (*bt_compare)(DB *, const DBT *, const DBT *);
    struct db_header *header;
    int database_number; // -1 if it is the single unnamed database.  Nonnengative number otherwise.
    DB_ENV *env;
    char *full_fname;
    char *database_name;
    //int fd;
    u_int32_t open_flags;
    int open_mode;
    BRT brt;
    int is_db_dup;
    unsigned long long fileid;
};
#endif
