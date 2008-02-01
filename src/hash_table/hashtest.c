/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "key.h"
#include "hashtable.h"
#include "memory.h"
#include "primes.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

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

void verify_htable_instance (bytevec kv_v, ITEMLEN kl, bytevec dv_v, ITEMLEN dl, int type,
			    int N, int *data, char *saw) {
    char *kv = (char*)kv_v;
    char *dv = (char*)dv_v;
    int num, k;
    assert(kv[0]=='k');
    assert(dv[0]=='d');
    assert(strcmp(kv+1, dv+1)==0);
    assert(strlen(kv)+1==kl);
    assert(strlen(dv)+1==dl);
    assert(type == 0);
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
    HASHTABLE_ITERATE(htable, kv, kl, dv, dl, type,
		      verify_htable_instance (kv, kl, dv, dl, type,
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
		toku_hash_insert(htable, kv, strlen(kv)+1, dv, strlen(dv)+1, 0);
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
	    snprintf(keys[j], 100, "k%ld", (long)(random()));
	    snprintf(vals[j], 100, "v%d", j);
	    toku_hash_insert(table, keys[j], strlen(keys[j])+1, vals[j], strlen(vals[j])+1, 0);
	}
	for (j=0; j<4; j++) {
	    bytevec key, val;
	    ITEMLEN keylen, vallen;
            int type;
	    long int randnum=random();
	    r = toku_hashtable_random_pick(table, &key, &keylen, &val, &vallen, &type, &randnum);
	    assert(r==0);
	    r = toku_hash_delete(table, key, keylen);
	    assert(r==0);
	}
    }
    toku_hashtable_free(&table);
}

void test_insert_nodup(int n) {
    HASHTABLE t;
    int r;

    r = toku_hashtable_create(&t);
    assert(r == 0);

    toku_hashtable_set_dups(t, 0);

    int keys[n], vals[n];

    int i;
    for (i=0; i<n; i++) {
        keys[i] = htonl(i);
        vals[i] = i;
        r = toku_hash_insert(t, &keys[i], sizeof keys[i], &vals[i], sizeof vals[i], i);
        assert(r == 0);
    }

    for (i=0; i<n; i++) {
        bytevec data; ITEMLEN datalen; int type;

        r = toku_hash_find(t, &keys[i], sizeof keys[i], &data, &datalen, &type);
        assert(r == 0);
        assert(datalen == sizeof vals[i]);
        assert(type == i);
        int vv;
        memcpy(&vv, data, datalen);
        assert(vv == vals[i]);
    }

    /* try to insert duplicates should fail */
    for (i=0; i<n; i++) {
        keys[i] = htonl(i);
        vals[i] = i;
        r = toku_hash_insert(t, &keys[i], sizeof keys[i], &vals[i], sizeof vals[i], i);
        assert(r != 0);
    }

    toku_hashtable_free(&t);
    assert(t == 0);
}

void test_insert_dup(int n, int do_delete_all) {
    HASHTABLE t;
    int r;

    r = toku_hashtable_create(&t);
    assert(r == 0);

    toku_hashtable_set_dups(t, 1);

    int keys[n], vals[n];
    int dupkey = n + n/2;

    int i;
    for (i=0; i<n; i++) {
        keys[i] = htonl(i);
        vals[i] = i;
        r = toku_hash_insert(t, &keys[i], sizeof keys[i], &vals[i], sizeof vals[i], i);
        assert(r == 0);
    }

    for (i=0; i<n; i++) {
        int key = htonl(dupkey);
        int val = i;
        r = toku_hash_insert(t, &key, sizeof key, &val, sizeof val, i);
        assert(r == 0);
    }

    for (i=0; i<n; i++) {
        bytevec data; ITEMLEN datalen; int type;

        r = toku_hash_find(t, &keys[i], sizeof keys[i], &data, &datalen, &type);
        assert(r == 0);
        assert(datalen == sizeof vals[i]);
        assert(type == i);
        int vv;
        memcpy(&vv, data, datalen);
        assert(vv == vals[i]);
    }

    for (i=0; ; i++) {
        int key = htonl(dupkey);
        bytevec data; ITEMLEN datalen; int type;

        r = toku_hash_find(t, &key, sizeof key, &data, &datalen, &type);
        if (r != 0) break;
            assert(datalen == sizeof vals[i]);
        assert(type == i);
        int vv;
        memcpy(&vv, data, datalen);
        assert(vv == vals[i]);

        if (do_delete_all)
            r = toku_hash_delete_all(t, &key, sizeof key);
        else
            r = toku_hash_delete(t, &key, sizeof key);
        assert(r == 0);
    }
    if (do_delete_all)
        assert(i == 1);
    else
        assert(i == n);

    toku_hashtable_free(&t);
    assert(t == 0);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    toku_test_primes();
    test0();
    test1();
    test_insert_nodup(1000);
    test_insert_dup(1000, 0);
    test_insert_dup(1000, 1);
    toku_malloc_cleanup();
    return 0;
}
