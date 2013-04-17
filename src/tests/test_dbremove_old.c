/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include "toku_portability.h"
#include <fcntl.h>
#include <errno.h>

#include <dirent.h>


#define stringify(x) #x
// ENVDIR is defined in the Makefile

DB_ENV *env;
DB *db;

static void
create_db(char *name) {
    char fullname[strlen(name) + sizeof(ENVDIR) + sizeof("/")];
    sprintf(fullname, "%s/%s", ENVDIR, name);
    int r;
    r=db_create(&db, NULL, 0);
        CKERR(r);
    r=db->open(db, NULL, fullname, NULL, DB_BTREE, DB_CREATE, 0666);
        CKERR(r);
    r=db->close(db, 0);
        CKERR(r);
}

static void
delete_db(char *name) {
    char fullname[strlen(name) + sizeof(ENVDIR) + sizeof("/")];
    sprintf(fullname, "%s/%s", ENVDIR, name);
    int r;
    toku_struct_stat buf;
    r = toku_stat(fullname, &buf);
        CKERR(r);

    r=db_create(&db, NULL, 0);
        CKERR(r);
    r=db->remove(db, fullname, NULL, 0);
        CKERR(r);
    r = toku_stat(fullname, &buf);
        CKERR2(r, -1);
    r = errno;
        CKERR2(r, ENOENT);
}

int
test_main (int UU(argc), char UU(*argv[])) {
    int r;
    r=system("rm -rf " ENVDIR);
        CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    system("cp test_dbremove_old.dir/* "ENVDIR);
        CKERR(r);
    {
        //Create and delete a brand new version db.
        char *unnamed_db = "version_now_unnamed.tokudb";
        create_db(unnamed_db);
        delete_db(unnamed_db);
    }
    {
        //Delete old version dbs
        DIR* direct = opendir(ENVDIR);
        struct dirent *entry;
        while ((entry = readdir(direct))) {
            if (entry->d_type&DT_REG) {
                delete_db(entry->d_name);
            }
        }
        closedir(direct);
    }
    return 0;
}
