/* The goal of this test:  Make sure that when we aggressively promote 
 * that we don't get a fencepost error on the size.  (#399, I think)
 * 
 * For various values of I do the following:
 *
 *   Make a tree of height 3 (that is, the root is of height 2)
 *   use small nodes (say 4KB)
 *   you have this tree:
 *                   A
 *                     B
 *                      C0 C1 C2 .. C15
 *   A has only one child.  B has as many children as it can get.
 *   Fill the C nodes (the leaves) all almost full.
 *   Fill B's buffer up with a big message X for C15, and a slightly smaller message Y for C1.
 *   Put into A's buffer a little message Z aimed at C0.
 *   Now when insert a message of size I aimed at C0.  I and Z together are too big to fit in A.
 *   First: X will be pushed into C15, resulting in this split
 *    A
 *     B0
 *       C0 C1 ... C8
 *     B1
 *       C9 C10 ... C15 C16
 *   At this point C0 through C14 are full, Y is in B0's buffer, and A's buffer contains I and Z.
 *   So we try to push Z if it fits.  Which it does.
 *   So then we try to I if it fits.  If we calculated wrong, everything  breaks now.
 *  
 */

#include "brt.h"
#include "key.h"
#include "toku_assert.h"
#include "brt-internal.h"


#include <stdio.h>
#include <string.h>
#include <unistd.h>


static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, PSIZE=20 };

CACHETABLE ct;
BRT t;
int fnamelen;
char *fname;

void doit (int ksize) {
    DISKOFF cnodes[BRT_FANOUT], bnode, anode;
    u_int32_t fingerprints[BRT_FANOUT];

    char *keys[BRT_FANOUT-1];
    int keylens[BRT_FANOUT-1];
    int i;
    int r;
    
    fnamelen = strlen(__FILE__) + 20;
    fname = malloc(fnamelen);
    assert(fname!=0);

    snprintf(fname, fnamelen, "%s.brt", __FILE__);
    r = toku_brt_create_cachetable(&ct, 16*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 0, 1, &t, NODESIZE, ct, null_txn, toku_default_compare_fun, null_db);
    assert(r==0);

    for (i=0; i<BRT_FANOUT; i++) {
	r=toku_testsetup_leaf(t, &cnodes[i]);
	assert(r==0);
	fingerprints[i]=0;
	char key[KSIZE+10];
	int keylen = 1+snprintf(key, KSIZE, "%08d%0*d", i*10000+1, KSIZE-9, 0);
	char val[1];
	char vallen=0;
	r=toku_testsetup_insert_to_leaf(t, cnodes[i], key, keylen, val, vallen, &fingerprints[i]);
	assert(r==0);
    }

    // Now we have a bunch of leaves, all of which are with 100 bytes of full.
    for (i=0; i+1<BRT_FANOUT; i++) {
	char key[PSIZE];
	keylens[i]=1+snprintf(key, PSIZE, "%08d", (i+1)*10000);
	keys[i]=strdup(key);
    }

    r = toku_testsetup_nonleaf(t, 1, &bnode, BRT_FANOUT, cnodes, fingerprints, keys, keylens);
    assert(r==0);

    u_int32_t bfingerprint=0;
    {
	const int magic_size = (NODESIZE-toku_testsetup_get_sersize(t, bnode))/2-25;
	printf("magic_size=%d\n", magic_size);
	char key [KSIZE];
	int keylen = 1+snprintf(key, KSIZE, "%08d%0*d", 150002, magic_size, 0);
	char val[1];
	char vallen=0;
	r=toku_testsetup_insert_to_nonleaf(t, bnode, BRT_INSERT, key, keylen, val, vallen, &bfingerprint);

	keylen = 1+snprintf(key, KSIZE, "%08d%0*d", 2, magic_size-1, 0);
	r=toku_testsetup_insert_to_nonleaf(t, bnode, BRT_INSERT, key, keylen, val, vallen, &bfingerprint);	
    }
    printf("%lld sersize=%d\n", bnode, toku_testsetup_get_sersize(t, bnode));
    // Now we have an internal node which has full children and the buffers are nearly full

    r = toku_testsetup_nonleaf(t, 2, &anode, 1,          &bnode, &bfingerprint,    0, 0);
    assert(r==0);
    {
	char key[20];
	int keylen = 1+snprintf(key, 20, "%08d", 3);
	char val[1];
	char vallen=0;
	r=toku_testsetup_insert_to_nonleaf(t, anode, BRT_INSERT, key, keylen, val, vallen, &bfingerprint); 
    }
    if (0)
    {
	const int magic_size = 1; //NODESIZE-toku_testsetup_get_sersize(t, anode)-100;
	DBT k,v;
	char key[20];
	char data[magic_size];
	int keylen=1+snprintf(key, sizeof(key), "%08d", 4);
	int vallen=magic_size;
	snprintf(data, magic_size, "%*s", magic_size-1, " ");
	r=toku_brt_insert(t,
			  toku_fill_dbt(&k, key, keylen),
			  toku_fill_dbt(&v, data, vallen),
			  null_txn);
    }

    r = toku_testsetup_root(t, anode);
    assert(r==0);

    r = toku_close_brt(t);          assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    printf("ksize=%d, unused\n", ksize);

}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int i;
    doit(53); exit(0);
    for (i=1; i<NODESIZE/2; i++) {
	printf("extrasize=%d\n", i);
	doit(i);
    }
    return 0;
}
