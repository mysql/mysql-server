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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_htonl.h>
#include <toku_assert.h>
#include <toku_stdlib.h>

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <portability/toku_path.h>

#include "ft.h"
#include "key.h"
#include "block_table.h"
#include "log-internal.h"
#include "logger.h"
#include "fttypes.h"
#include "ft-ops.h"
#include "cachetable.h"
#include "cachetable-internal.h"

#define CKERR(r) ({ int __r = r; if (__r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, __r, strerror(r)); assert(__r==0); })
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

#define DEBUG_LINE() do { \
    fprintf(stderr, "%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr); \
} while (0)

const ITEMLEN len_ignore = 0xFFFFFFFF;


// dummymsn needed to simulate msn because test messages are injected at a lower level than toku_ft_root_put_msg()
#define MIN_DUMMYMSN ((MSN) {(uint64_t)1<<62})
static MSN dummymsn;      
static int dummymsn_initialized = 0;


static void
initialize_dummymsn(void) {
    if (dummymsn_initialized == 0) {
        dummymsn_initialized = 1;
        dummymsn = MIN_DUMMYMSN;
    }
}

static UU() MSN 
next_dummymsn(void) {
    assert(dummymsn_initialized);
    ++(dummymsn.msn);
    return dummymsn;
}

static UU() MSN 
last_dummymsn(void) {
    assert(dummymsn_initialized);
    return dummymsn;
}


struct check_pair {
    ITEMLEN keylen;  // A keylen equal to 0xFFFFFFFF means don't check the keylen or the key.
    bytevec key;     // A NULL key means don't check the key.
    ITEMLEN vallen;  // Similarly for vallen and null val.
    bytevec val;
    int call_count;
};
static int
lookup_checkf (ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *pair_v, bool lock_only) {
    if (!lock_only) {
        struct check_pair *pair = (struct check_pair *) pair_v;
        if (key!=NULL) {
            if (pair->keylen!=len_ignore) {
                assert(pair->keylen == keylen);
                if (pair->key) 
                    assert(memcmp(pair->key, key, keylen)==0);
            }
            if (pair->vallen!=len_ignore) {
                assert(pair->vallen == vallen);
                if (pair->val)
                    assert(memcmp(pair->val, val, vallen)==0);
            }
            pair->call_count++; // this call_count is really how many calls were made with r==0
        }
    }
    return 0;
}

static inline void
ft_lookup_and_check_nodup (FT_HANDLE t, const char *keystring, const char *valstring)
{
    DBT k;
    toku_fill_dbt(&k, keystring, strlen(keystring) + 1);
    struct check_pair pair = {(ITEMLEN) (1+strlen(keystring)), keystring,
                              (ITEMLEN) (1+strlen(valstring)), valstring,
			      0};
    int r = toku_ft_lookup(t, &k, lookup_checkf, &pair);
    assert(r==0);
    assert(pair.call_count==1);
}

static inline void
ft_lookup_and_fail_nodup (FT_HANDLE t, char *keystring)
{
    DBT k;
    toku_fill_dbt(&k, keystring, strlen(keystring) + 1);
    struct check_pair pair = {(ITEMLEN) (1+strlen(keystring)), keystring,
			      0, 0,
			      0};
    int r = toku_ft_lookup(t, &k, lookup_checkf, &pair);
    assert(r!=0);
    assert(pair.call_count==0);
}

static UU() void fake_ydb_lock(void) {
}

static UU() void fake_ydb_unlock(void) {
}

static UU() void
def_flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void **dd     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep   __attribute__((__unused__)),
       bool c      __attribute__((__unused__)),
       bool UU(is_clone)
       ) {
}

static UU() void 
def_pe_est_callback(
    void* UU(ftnode_pv),
    void* UU(dd), 
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

static UU() int 
def_pe_callback(
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free __attribute__((__unused__)), 
    void* extraargs __attribute__((__unused__)),
    void (*finalize)(PAIR_ATTR bytes_freed, void *extra),
    void *finalize_extra
    )
{
    finalize(bytes_to_free, finalize_extra);
    return 0;
}

static UU() void
def_pe_finalize_impl(PAIR_ATTR UU(bytes_freed), void *UU(extra)) { }

static UU() bool def_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
  return false;
}

  static UU() int def_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
  assert(false);
  return 0;
}

static UU() int
def_fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void **dd     __attribute__((__unused__)),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
    *dirtyp = 0;
    *value = NULL;
    *sizep = make_pair_attr(8);
    return 0;
}

static UU() void
put_callback_nop(
    CACHEKEY UU(key),
    void *UU(v),
    PAIR UU(p)) {
}

static UU() int
fetch_die(
    CACHEFILE UU(thiscf), 
    PAIR UU(p),
    int UU(fd), 
    CACHEKEY UU(key), 
    uint32_t UU(fullhash), 
    void **UU(value),
    void **UU(dd), 
    PAIR_ATTR *UU(sizep), 
    int *UU(dirtyp), 
    void *UU(extraargs)
    )
{
    assert(0); // should not be called
    return 0;
}


static UU() int
def_cleaner_callback(
    void* UU(ftnode_pv),
    BLOCKNUM UU(blocknum),
    uint32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(false);
    return 0;
}

static UU() CACHETABLE_WRITE_CALLBACK def_write_callback(void* write_extraargs) {
    CACHETABLE_WRITE_CALLBACK wc;
    wc.flush_callback = def_flush;
    wc.pe_est_callback = def_pe_est_callback;
    wc.pe_callback = def_pe_callback;
    wc.cleaner_callback = def_cleaner_callback;
    wc.write_extraargs = write_extraargs;
    wc.clone_callback = nullptr;
    wc.checkpoint_complete_callback = nullptr;
    return wc;
}

class evictor_test_helpers {
public:
    static void set_hysteresis_limits(evictor* ev, long low_size_watermark, long high_size_watermark) {
        ev->m_low_size_watermark = low_size_watermark;
        ev->m_low_size_hysteresis = low_size_watermark;
        ev->m_high_size_hysteresis = high_size_watermark;
        ev->m_high_size_watermark = high_size_watermark;
    }
    static void disable_ev_thread(evictor* ev) {
        toku_mutex_lock(&ev->m_ev_thread_lock);
        ev->m_period_in_seconds = 0;
        // signal eviction thread so that it wakes up
        // and then sleeps indefinitely
        ev->signal_eviction_thread();
        toku_mutex_unlock(&ev->m_ev_thread_lock);
        // sleep for one second to ensure eviction thread picks up new period
        usleep(1*1024*1024);
    }
    static uint64_t get_num_eviction_runs(evictor* ev) {
        return ev->m_num_eviction_thread_runs;
    }
};

UU()
static void copy_dbt(DBT *dest, const DBT *src) {
    assert(dest->flags & DB_DBT_REALLOC);
    dest->data = toku_realloc(dest->data, src->size);
    dest->size = src->size;
    memcpy(dest->data, src->data, src->size);
}

int verbose=0;

static inline void
default_parse_args (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    ++verbose;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

int test_main(int argc, const char *argv[]);

int
main(int argc, const char *argv[]) {
    initialize_dummymsn();
    int rinit = toku_ft_layer_init();
    CKERR(rinit);
    int r = test_main(argc, argv);
    toku_ft_layer_destroy();
    return r;
}

