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

#include <stdlib.h>
#include <assert.h>
#include <db_cxx.h>
#include <memory.h>

static void hexdump(Dbt *d) {
    unsigned char *cp = (unsigned char *) d->get_data();
    int n = d->get_size();
    printf(" ");
    for (int i=0; i<n; i++)
        printf("%2.2x", cp[i]);
    printf("\n");
}

static int dbdump(const char *env_dir, const char *dbfile, const char *dbname) {
    int r;

#if defined(USE_ENV) && USE_ENV
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    r = env.set_redzone(0); assert(r==0);
    r = env.open(env_dir, DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
    Db db(&env, DB_CXX_NO_EXCEPTIONS);
#else
    Db db(0, DB_CXX_NO_EXCEPTIONS);
#endif
    r = db.open(0, dbfile, dbname, DB_UNKNOWN, 0, 0777); 
    if (r != 0) {
        printf("cant open %s:%s %d:%s\n", dbfile, dbname, r, db_strerror(r));
#if defined(USE_ENV) && USE_ENV
        r = env.close(0); assert(r == 0);
#endif
        return 1;
    }

    u_int32_t dbflags;
    r = db.get_flags(&dbflags); assert(r == 0);
#ifndef TOKUDB
    if (dbflags & DB_DUP)
        printf("duplicates=1\n");
    if (dbflags & DB_DUPSORT)
        printf("dupsort=1\n");
#endif
#if 0
    u_int32_t nodesize;
    r = db.get_nodesize(&nodesize); assert(r == 0);
    printf("nodesize=%d\n", nodesize);
#endif

    Dbc *cursor;
    r = db.cursor(0, &cursor, 0); assert(r == 0);

    Dbt key; key.set_flags(DB_DBT_REALLOC);
    Dbt val; val.set_flags(DB_DBT_REALLOC);
    for (;;) {
        r = cursor->get(&key, &val, DB_NEXT);
        if (r != 0) break;
        // printf("%.*s\n", key.get_size(), (char *)key.get_data());
        hexdump(&key);
        // printf("%.*s\n", val.get_size(), (char *)val.get_data());
        hexdump(&val);
    }
    if (key.get_data()) toku_free(key.get_data());
    if (val.get_data()) toku_free(val.get_data());

    r = cursor->close(); assert(r == 0);
    r = db.close(0); assert(r == 0);
#if defined(USE_ENV) && USE_ENV
    r = env.close(0); assert(r == 0);
#endif
    return 0;
}

static int usage() {
    printf("db_dump [-s DBNAME] DBFILE\n");
    return 1;
}

int main(int argc, const char *argv[]) {
    const char *dbname = 0;
    const char *env_dir = ".";

    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
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
        break;
    }
    if (i >= argc)
        return usage();

    return dbdump(env_dir, argv[i], dbname);
}

