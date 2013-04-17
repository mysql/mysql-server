/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
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
          memcmp(&a->id, &b->id, sizeof(b->id))==0));
}

void toku_db_id_add_ref(toku_db_id* db_id) {
    assert(db_id);
    assert(db_id->ref_count > 0);
    db_id->ref_count++;
}

static void toku_db_id_close(toku_db_id** pdb_id) {
    toku_db_id* db_id = *pdb_id;
    toku_free(db_id);
    *pdb_id = NULL;
}

void toku_db_id_remove_ref(toku_db_id** pdb_id) {
    toku_db_id* db_id = *pdb_id;
    assert(db_id);
    assert(db_id->ref_count > 0);
    db_id->ref_count--;
    if (db_id->ref_count > 0) { return; }
    toku_db_id_close(pdb_id);
}

int toku_db_id_create(toku_db_id** pdbid, int fd) {
    int r = ENOSYS;
    toku_db_id* db_id = NULL;

    db_id = (toku_db_id *)toku_malloc(sizeof(*db_id));
    if (!db_id) { r = ENOMEM; goto cleanup; }
    memset(db_id, 0, sizeof(*db_id));

    r = toku_os_get_unique_file_id(fd, &db_id->id);
    if (r!=0) goto cleanup;

    db_id->saved_hash = hash_key((unsigned char*)&db_id->id, sizeof(db_id->id));

    db_id->ref_count = 1;
    *pdbid = db_id;
    r = 0;
cleanup:
    if (r != 0) {
        if (db_id != NULL) {
            toku_free(db_id);
        }
    }
    return r;
}

