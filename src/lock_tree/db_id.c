/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <memory.h>
#include <hashfun.h>
#include "db_id.h"


BOOL toku_db_id_equals(const toku_db_id* a, const toku_db_id* b) {
    assert(a && b);
    return (BOOL)
        (a == b || 
         (a->saved_hash == b->saved_hash &&
          !strcmp(a->absolute_path, b->absolute_path) &&
          !strcmp(a->sub_database_name, b->sub_database_name)));
}

void toku_db_id_add_ref(toku_db_id* db_id) {
    assert(db_id);
    assert(db_id->ref_count > 0);
    db_id->ref_count++;
}

static void toku_db_id_close(toku_db_id* db_id) {
    toku_free(db_id->absolute_path);
    toku_free(db_id->sub_database_name);
    toku_free(db_id);
}

void toku_db_id_remove_ref(toku_db_id* db_id) {
    assert(db_id);
    assert(db_id->ref_count > 0);
    db_id->ref_count--;
    if (db_id->ref_count > 0) { return; }
    toku_db_id_close(db_id);
}

int toku_db_id_create(toku_db_id** pdbid, const char* path,
                             const char* sub_database_name) {
    int r = ENOSYS;
    assert(sub_database_name);
    toku_db_id* db_id = NULL;

    db_id = (toku_db_id *)toku_malloc(sizeof(*db_id));
    if (!db_id) { r = ENOMEM; goto cleanup; }
    memset(db_id, 0, sizeof(*db_id));

    db_id->absolute_path = (char *)toku_malloc((PATH_MAX + 1) * sizeof(char));
    if (!db_id->absolute_path) { r = ENOMEM; goto cleanup; }

    /* TODO: BUG!  Buffer overflow if the path > PATH_MAX. */    
    if (realpath(path, db_id->absolute_path) == NULL) {
        r = errno;
        goto cleanup;
    }
    char* tmp = (char*)toku_realloc(db_id->absolute_path,
				    (strlen(db_id->absolute_path) + 1) * sizeof(char));
    if (!tmp) { r = ENOMEM; goto cleanup; }
    db_id->absolute_path = tmp;
                    
    db_id->sub_database_name = toku_strdup(sub_database_name);
    if (!db_id->sub_database_name) { r = ENOMEM; goto cleanup; }

    db_id->saved_hash = hash_key((unsigned char*)db_id->absolute_path,
                                 strlen(db_id->absolute_path));
    db_id->saved_hash = hash_key_extend(db_id->saved_hash,
                                       (unsigned char*)db_id->sub_database_name,
                                        strlen(db_id->sub_database_name));

    db_id->ref_count = 1;
    *pdbid = db_id;
    r = 0;
cleanup:
    if (r != 0) {
        if (db_id != NULL) {
            if (db_id->absolute_path)     { toku_free(db_id->absolute_path); }
            if (db_id->sub_database_name) { toku_free(db_id->sub_database_name); }
            toku_free(db_id);
        }
    }
    return r;
}
