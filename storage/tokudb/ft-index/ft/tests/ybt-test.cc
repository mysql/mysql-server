/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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
  This software is covered by US Patent No. 8,489,638.

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

#include "test.h"



static void
cleanup_and_free(struct simple_dbt *v) {
    if (v->data) toku_free(v->data);
    v->data = NULL;
    v->len = 0;
}

static void
cleanup(struct simple_dbt *v) {
    v->data = NULL;
    v->len = 0;
}

static void ybt_test0 (void) {
    struct simple_dbt v0 = {0,0}, v1 = {0,0};
    DBT  t0,t1;
    toku_init_dbt(&t0);
    toku_init_dbt(&t1);
    {
	const void *temp1 = "hello";
        toku_dbt_set(6, temp1, &t0, &v0);
    }
    {
        const void *temp2 = "foo";
	toku_dbt_set(  4, temp2, &t1, &v1);
    }
    assert(t0.size==6);
    assert(strcmp((char*)t0.data, "hello")==0); 
    assert(t1.size==4);
    assert(strcmp((char*)t1.data, "foo")==0);

    {
        const void *temp3 = "byebye";
	toku_dbt_set(7, temp3, &t1, &v0);      /* Use v0, not v1 */
    }
    // This assertion would be wrong, since v0 may have been realloc'd, and t0.data may now point
    // at the wrong place
    //assert(strcmp(t0.data, "byebye")==0);     /* t0's data should be changed too, since it used v0 */
    assert(strcmp((char*)t1.data, "byebye")==0);

    cleanup_and_free(&v0);
    cleanup_and_free(&v1);
    

    /* See if we can probe to find out how big something is by setting ulen=0 with YBT_USERMEM */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_USERMEM;
    t0.ulen  = 0;
    {
        const void *temp4 = "hello";
	toku_dbt_set(6, temp4, &t0, 0);
    }
    assert(t0.data==0);
    assert(t0.size==6);

    /* Check realloc. */
    toku_init_dbt(&t0);
    t0.flags = DB_DBT_REALLOC;
    cleanup(&v0);
    {
        const void *temp5 = "internationalization";
	toku_dbt_set(21, temp5, &t0, &v0);
    }
    assert(v0.data==0); /* Didn't change v0 */
    assert(t0.size==21);
    assert(strcmp((char*)t0.data, "internationalization")==0);

    {
        const void *temp6 = "provincial";
	toku_dbt_set(11, temp6, &t0, &v0);
    }
    assert(t0.size==11);
    assert(strcmp((char*)t0.data, "provincial")==0);
    
    toku_free(t0.data);
    
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    ybt_test0();
    return 0;
}
