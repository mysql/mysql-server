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
    struct db_header *header;
    int database_number; // -1 if it is the single unnamed database.  Nonnengative number otherwise.
    char *full_fname;
    char *database_name;
    //int fd;
    u_int32_t open_flags;
    int open_mode;
    BRT brt;
    FILENUM fileid;
};
#endif
