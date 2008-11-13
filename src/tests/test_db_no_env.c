/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <portability.h>
#include <db.h>

#include "test.h"

int main (int UU(argc), char UU(*argv[])) {
    DB *db;
    int r;
    r = db_create(&db, 0, 0); 
    assert(r == 0);
    r = db->close(db, 0);       
    assert(r == 0);
    return 0;
}
