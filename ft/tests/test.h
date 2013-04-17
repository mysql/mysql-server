/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "toku_assert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ft-ops.h"
#include <toku_htonl.h>



#define CKERR(r) ({ int __r = r; if (__r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, __r, strerror(r)); assert(__r==0); })
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

#define DEBUG_LINE() do { \
    fprintf(stderr, "%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr); \
} while (0)

const ITEMLEN len_ignore = 0xFFFFFFFF;


// dummymsn needed to simulate msn because test messages are injected at a lower level than toku_ft_root_put_cmd()
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
def_pe_callback (
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free __attribute__((__unused__)), 
    PAIR_ATTR* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    *bytes_freed = bytes_to_free;
    return 0;
}

static UU() bool def_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
  return false;
}

  static UU() int def_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
  assert(false);
  return 0;
}

static UU() int
def_fetch (CACHEFILE f        __attribute__((__unused__)),
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
    wc.clone_callback = NULL;
    return wc;
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

