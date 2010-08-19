#include <toku_portability.h>
#include "toku_assert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "brt.h"
#include "toku_htonl.h"

#if defined(__cplusplus)
extern "C" {
#endif


#define CKERR(r) do { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); } while (0)
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

#define DEBUG_LINE() do { \
    fprintf(stderr, "%s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr); \
} while (0)

const ITEMLEN len_ignore = 0xFFFFFFFF;

struct check_pair {
    ITEMLEN keylen;  // A keylen equal to 0xFFFFFFFF means don't check the keylen or the key.
    bytevec key;     // A NULL key means don't check the key.
    ITEMLEN vallen;  // Similarly for vallen and null val.
    bytevec val;
    int call_count;
};
static int
lookup_checkf (ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *pair_v) {
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
    return 0;
}

static inline void
brt_lookup_and_check_nodup (BRT t, char *keystring, char *valstring)
{
#if defined(__cplusplus)
    DBT k; memset(&k, 0, sizeof k); k.size=1+strlen(keystring); k.data=keystring;
#else
    DBT k = {.size=1+strlen(keystring), .data=keystring};
#endif
    struct check_pair pair = {1+strlen(keystring), keystring,
			      1+strlen(valstring), valstring,
			      0};
    int r = toku_brt_lookup(t, &k, lookup_checkf, &pair);
    assert(r==0);
    assert(pair.call_count==1);
}

static inline void
brt_lookup_and_fail_nodup (BRT t, char *keystring)
{
#if defined(__cplusplus)
    DBT k; memset(&k, 0, sizeof k); k.size=1+strlen(keystring); k.data=keystring;
#else
    DBT k = {.size=1+strlen(keystring), .data=keystring};
#endif
    struct check_pair pair = {1+strlen(keystring), keystring,
			      0, 0,
			      0};
    int r = toku_brt_lookup(t, &k, lookup_checkf, &pair);
    assert(r!=0);
    assert(pair.call_count==0);
}


int verbose=0;

static inline void
default_parse_args (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
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

static void dummy(void) {}
static void dummy_set_brt(DB *db UU(), BRT brt UU()) {}

int
main(int argc, const char *argv[]) {
    int rinit = toku_brt_init(dummy, dummy, dummy_set_brt);
    CKERR(rinit);
    int r = test_main(argc, argv);
    int rdestroy = toku_brt_destroy();
    CKERR(rdestroy);
    return r;
}

#if defined(__cplusplus)
extern "C" {
#endif
