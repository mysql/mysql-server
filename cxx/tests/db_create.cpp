/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <assert.h>
#include <db_cxx.h>

char *data_dir, *env_dir=NULL;

static int dbcreate(char *dbfile, char *dbname, int dbflags, int argc, char *argv[]) {
    int r;
    DbEnv *env = new DbEnv(DB_CXX_NO_EXCEPTIONS);
    if (data_dir) {
        r = env->set_data_dir(data_dir); assert(r == 0);
    }
    r = env->set_redzone(0); assert(r==0);
    r = env->open(env_dir ? env_dir : ".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);

    Db *db = new Db(env, DB_CXX_NO_EXCEPTIONS);
    r = db->set_flags(dbflags); assert(r == 0);
    r = db->open(0, dbfile, dbname, DB_BTREE, DB_CREATE, 0777);
    if (r != 0) {
        printf("db->open %s(%s) %d %s\n", dbfile, dbname, r, db_strerror(r));
	db->close(0);  delete db;
        env->close(0); delete env;
        return 1;
    }

    int i = 0;
    while (i < argc) {
        char *k = argv[i++];
        if (i < argc) {
            char *v = argv[i++];
            Dbt key(k, strlen(k)); Dbt val(v, strlen(v));
            r = db->put(0, &key, &val, 0); assert(r == 0);
        }
    }
            
    r = db->close(0); assert(r == 0);
    delete db;
    r = env->close(0); assert(r == 0);
    delete env;
    
    return 0;
}

static int usage() {
    fprintf(stderr, "db_create [-s DBNAME] DBFILE [KEY VAL]*\n");
    fprintf(stderr, "[--set_data_dir DIRNAME]\n");
    return 1;
}

int main(int argc, char *argv[]) {
    char *dbname = 0;
    int dbflags = 0;

    int i;
    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help"))
            return usage();
        if (0 == strcmp(arg, "-s")) {
            if (i+1 >= argc)
                return usage();
            dbname = argv[++i];
            continue;
        }
	if (0 == strcmp(arg, "--env_dir")) {
            if (i+1 >= argc)
                return usage();
	    env_dir = argv[++i];
	    continue;
	}
        if (0 == strcmp(arg, "--set_data_dir")) {
            if (i+1 >= argc)
                return usage();
            data_dir = argv[++i];
            continue;
        }
	if (arg[0]=='-') {
	    printf("I don't understand this argument: %s\n", arg);
	    return 1;
	}
        break;
    }

    if (i >= argc)
        return usage();
    char *dbfile = argv[i++];
    return dbcreate(dbfile, dbname, dbflags, argc-i, &argv[i]);
}

