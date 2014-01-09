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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "test.h"



#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
const double USECS_PER_SEC = 1000000.0;

static void
le_add_to_bn(bn_data* bn, uint32_t idx, char *key, int keylen, char *val, int vallen)
{
    LEAFENTRY r = NULL;
    uint32_t size_needed = LE_CLEAN_MEMSIZE(vallen);
    bn->get_space_for_insert(
        idx, 
        key,
        keylen,
        size_needed,
        &r
        );
    resource_assert(r);
    r->type = LE_CLEAN;
    r->u.clean.vallen = vallen;
    memcpy(r->u.clean.val, val, vallen);
}

static int
long_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    const long *CAST_FROM_VOIDP(x, a->data);
    const long *CAST_FROM_VOIDP(y, b->data);
    return (*x > *y) - (*x < *y);
}

static void
test_serialize_leaf(int valsize, int nelts, double entropy) {
    //    struct ft_handle source_ft;
    struct ftnode *sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    XCALLOC(sn);

    sn->max_msn_applied_to_node_on_disk.msn = 0;
    sn->flags = 0x11223344;
    sn->thisnodename.b = 20;
    sn->layout_version = FT_LAYOUT_VERSION;
    sn->layout_version_original = FT_LAYOUT_VERSION;
    sn->height = 0;
    sn->n_children = 8;
    sn->dirty = 1;
    sn->oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(sn->n_children, sn->bp);
    MALLOC_N(sn->n_children-1, sn->childkeys);
    sn->totalchildkeylens = 0;
    for (int i = 0; i < sn->n_children; ++i) {
        BP_STATE(sn,i) = PT_AVAIL;
        set_BLB(sn, i, toku_create_empty_bn());
    }
    int nperbn = nelts / sn->n_children;
    for (int ck = 0; ck < sn->n_children; ++ck) {
        long k;
        for (long i = 0; i < nperbn; ++i) {
            k = ck * nperbn + i;
            char buf[valsize];
            int c;
            for (c = 0; c < valsize * entropy; ) {
                int *p = (int *) &buf[c];
                *p = rand();
                c += sizeof(*p);
            }
            memset(&buf[c], 0, valsize - c);
            le_add_to_bn(
                BLB_DATA(sn,ck),
                i,
                (char *)&k, 
                sizeof k, 
                buf, 
                sizeof buf
                );
        }
        if (ck < 7) {
            toku_memdup_dbt(&sn->childkeys[ck], &k, sizeof k);
            sn->totalchildkeylens += sizeof k;
        }
    }

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD,
                 16);
    brt->ft = brt_h;
    
    brt_h->compare_fun = long_key_cmp;
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }

    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), sn, &ndd, true, brt->ft, false);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    double dt;
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("serialize leaf:   %0.05lf\n", dt);

    struct ftnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt_h);
    gettimeofday(&t[0], NULL);
    FTNODE_DISK_DATA ndd2 = NULL;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd2, &bfe);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("deserialize leaf: %0.05lf\n", dt);
    printf("io time %lf decompress time %lf deserialize time %lf\n",
           tokutime_to_seconds(bfe.io_time),
           tokutime_to_seconds(bfe.decompress_time),
           tokutime_to_seconds(bfe.deserialize_time)
           );

    toku_ftnode_free(&dn);
    toku_ftnode_free(&sn);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(ndd);
    toku_free(ndd2);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_nonleaf(int valsize, int nelts, double entropy) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_ft.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 1;
    sn.n_children = 8;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = 0;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_BLOCKNUM(&sn, i).b = 30 + (i*5);
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BNC(&sn, i, toku_create_empty_nl());
    }
    //Create XIDS
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    int nperchild = nelts / 8;
    for (int ck = 0; ck < sn.n_children; ++ck) {
        long k;
        NONLEAF_CHILDINFO bnc = BNC(&sn, ck);
        for (long i = 0; i < nperchild; ++i) {
            k = ck * nperchild + i;
            char buf[valsize];
            int c;
            for (c = 0; c < valsize * entropy; ) {
                int *p = (int *) &buf[c];
                *p = rand();
                c += sizeof(*p);
            }
            memset(&buf[c], 0, valsize - c);

            toku_bnc_insert_msg(bnc, &k, sizeof k, buf, valsize, FT_NONE, next_dummymsn(), xids_123, true, NULL, long_key_cmp);
        }
        if (ck < 7) {
            toku_memdup_dbt(&sn.childkeys[ck], &k, sizeof k);
            sn.totalchildkeylens += sizeof k;
        }
    }

    //Cleanup:
    xids_destroy(&xids_0);
    xids_destroy(&xids_123);

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD,
                 16);
    brt->ft = brt_h;
    
    brt_h->compare_fun = long_key_cmp;
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }

    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), &sn, &ndd, true, brt->ft, false);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    double dt;
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("serialize nonleaf:   %0.05lf\n", dt);

    struct ftnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt_h);
    gettimeofday(&t[0], NULL);
    FTNODE_DISK_DATA ndd2 = NULL;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd2, &bfe);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("deserialize nonleaf: %0.05lf\n", dt);
    printf("io time %lf decompress time %lf deserialize time %lf\n",
           tokutime_to_seconds(bfe.io_time),
           tokutime_to_seconds(bfe.decompress_time),
           tokutime_to_seconds(bfe.deserialize_time)
           );

    toku_ftnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        destroy_nonleaf_childinfo(BNC(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(ndd);
    toku_free(ndd2);

    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    long valsize, nelts;
    double entropy = 0.3;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <valsize> <nelts>\n", argv[0]);
        return 2;
    }
    valsize = strtol(argv[1], NULL, 0);
    nelts = strtol(argv[2], NULL, 0);

    initialize_dummymsn();
    test_serialize_leaf(valsize, nelts, entropy);
    test_serialize_nonleaf(valsize, nelts, entropy);

    return 0;
}
