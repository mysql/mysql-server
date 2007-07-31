#include "key.h"
#include "hashtable.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

void verify_hash_instance (bytevec kv_v, ITEMLEN kl, bytevec dv_v, ITEMLEN dl,
			    int N, int *data, char *saw) {
    char *kv = (char*)kv_v;
    char *dv = (char*)dv_v;
    int num, k;
    assert(kv[0]=='k');
    assert(dv[0]=='d');
    assert(strcmp(kv+1, dv+1)==0);
    assert(strlen(kv)+1==kl);
    assert(strlen(dv)+1==dl);
    num = atoi(kv+1);
    for (k=0; k<N; k++) {
	if (data[k]==num) {
	    assert(!saw[k]);
	    saw[k]=1;
	    return;
	}
    }
    fprintf(stderr, "%s isn't there\n", kv); abort();
}

void verify_htable_instance (bytevec kv_v, ITEMLEN kl, bytevec dv_v, ITEMLEN dl,
			    int N, int *data, char *saw) {
    char *kv = (char*)kv_v;
    char *dv = (char*)dv_v;
    int num, k;
    assert(kv[0]=='k');
    assert(dv[0]=='d');
    assert(strcmp(kv+1, dv+1)==0);
    assert(strlen(kv)+1==kl);
    assert(strlen(dv)+1==dl);
    num = atoi(kv+1);
    for (k=0; k<N; k++) {
	if (data[k]==num) {
	    assert(!saw[k]);
	    saw[k]=1;
	    return;
	}
    }
    fprintf(stderr, "%s isn't there\n", kv); abort();
}

void verify_htable (HASHTABLE htable, int N, int *data, char *saw) {
    int j;
    for (j=0; j<N; j++) {
	saw[j]=0;
    }
    HASHTABLE_ITERATE(htable, kv, kl, dv, dl,
		      verify_htable_instance (kv, kl, dv, dl,
					      N, data, saw));
    for (j=0; j<N; j++) {
	assert(saw[j]);
    }
}

void test0 (void) {
    int r, i, j;
    HASHTABLE htable;
    int n_ops=1000;
    int *data=malloc(sizeof(*data)*n_ops);
    char*saw =malloc(sizeof(*saw)*n_ops);
    int  data_n = 0;
    assert(data!=0);
    r = toku_hashtable_create(&htable); assert(r==0);
    assert(toku_hashtable_n_entries(htable)==0);
#if 0
    {
	bytevec kv=(void*)0xdeadbeef;
	bytevec dv=(void*)0xbeefdead;
	ITEMLEN kl=42, dl=43;
	r = mdict_find_last(htable,&kv,&kl,&dv,&dl);
	assert(r!=0);
	assert((unsigned long)kv==0xdeadbeef);
	assert((unsigned long)dv==0xbeefdead);
	assert(kl==42);
	assert(dl==43);
    }
#endif
    for (i=0; i<n_ops; i++) {
	if (random()%4==1) {
	    // Delete something random
	} else if (random()%2 == 0) {
	    // Insert something
	try_another_random:
	    {
		int ra = random()%(1<<30);
		char kv[100], dv[100];
		for (j=0; j<data_n; j++) {
		    if (ra==data[j]) goto try_another_random;
		}
		snprintf(kv, 99, "k%d", ra);
		snprintf(dv, 99, "d%d", ra);
		toku_hash_insert(htable, kv, strlen(kv)+1, dv, strlen(dv)+1);
		data[data_n++]=ra;
	    }
	} else {
	    // Look up something
	}
	verify_htable(htable, data_n, data, saw);
    }
    toku_hashtable_free(&htable);
    free(data);
    free(saw);
}

void test1(void) {
    HASHTABLE table;
    int i, r;
    r = toku_hashtable_create(&table); assert(r==0);
    for (i=0; i<100; i++) {
	char keys[4][100], vals[4][100];
	int j;
	for (j=0; j<4; j++) {
	    snprintf(keys[j], 100, "k%ld", random());
	    snprintf(vals[j], 100, "v%d", j);
	    toku_hash_insert(table, keys[j], strlen(keys[j])+1, vals[j], strlen(vals[j])+1);
	}
	for (j=0; j<4; j++) {
	    bytevec key, val;
	    ITEMLEN keylen, vallen;
	    r = toku_hashtable_random_pick(table, &key, &keylen, &val, &vallen);
	    assert(r==0);
	    r = toku_hash_delete(table, key, keylen);
	    assert(r==0);
	}
    }
    toku_hashtable_free(&table);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test0();
    test1();
    return 0;
}
