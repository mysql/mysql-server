#include "toku_assert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "toku_portability.h"
#include "brt.h"
#include "toku_htonl.h"


#define CKERR(r) do { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); } while (0)
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)
#define CKERR2s(r,r2,r3) do { if (r!=r2 && r!=r3) fprintf(stderr, "%s:%d error %d %s, expected %d or %d\n", __FILE__, __LINE__, r, strerror(r), r2,r3); assert(r==r2||r==r3); } while (0)

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
    struct check_pair *pair = pair_v;
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
    DBT k = {.size=1+strlen(keystring), .data=keystring};
    struct check_pair pair = {1+strlen(keystring), keystring,
			      1+strlen(valstring), valstring,
			      0};
    int r = toku_brt_lookup(t, &k, 0, lookup_checkf, &pair);
    assert(r==0);
    assert(pair.call_count==1);
}

static inline void
brt_lookup_and_fail_nodup (BRT t, char *keystring)
{
    DBT k = {.size=1+strlen(keystring), .data=keystring};
    struct check_pair pair = {1+strlen(keystring), keystring,
			      0, 0,
			      0};
    int r = toku_brt_lookup(t, &k, 0, lookup_checkf, &pair);
    assert(r!=0);
    assert(pair.call_count==0);
}


int verbose=0;

void
unlink_file_and_bit(const char *name) {
    char dirty[strlen(name) + sizeof(".dirty")];
    char clean[strlen(name) + sizeof(".clean")];
    sprintf(dirty, "%s.dirty", name);
    sprintf(clean, "%s.clean", name);
    unlink(name);
    unlink(dirty);
    unlink(clean);
}

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

int
main(int argc, const char *argv[]) {
    toku_brt_init();
    int r = test_main(argc, argv);
    toku_brt_destroy();
    return r;
}

