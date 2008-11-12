/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "portability.h"
#include "os.h"
#include <brttypes.h>

#if !defined(TOKU_DB_ID_H)
#define TOKU_DB_ID_H

typedef struct __toku_db_id {
    struct fileid   id;
    char*           sub_database_name;
    u_int32_t       saved_hash;
    u_int32_t       ref_count;
} toku_db_id;

/* db_id methods */
int toku_db_id_create(toku_db_id** pdbid, int fd,
                             const char* sub_database_name);

BOOL toku_db_id_equals(const toku_db_id* a, const toku_db_id* b);

void toku_db_id_add_ref(toku_db_id* db_id);

void toku_db_id_remove_ref(toku_db_id** pdb_id);

#endif /* #if !defined(TOKU_DB_ID_H) */
