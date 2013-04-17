/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"


#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>
#include <signal.h>

static __attribute__((__noreturn__)) void catch_abort (int sig __attribute__((__unused__))) {
    exit(1);
}

int
test_main (int UU(argc), char UU(*const argv[])) {
    signal (SIGABRT, catch_abort);
    DB *db;
    int r;
    r = db_create(&db, 0, 0); 
    assert(r == 0);
    r = db->close(db, 0);       
    assert(r == 0);
    return 0;
}
