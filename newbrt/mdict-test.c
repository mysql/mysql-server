/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "mdict.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

void verify_mdict_instance (bytevec kv_v, ITEMLEN kl, bytevec dv_v, ITEMLEN dl,
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

void verify_mdict (MDICT mdict, int N, int *data, char *saw) {
    int j;
    for (j=0; j<N; j++) {
	saw[j]=0;
    }
    MDICT_ITERATE(mdict, kv, kl, dv, dl,
		  verify_mdict_instance (kv, kl, dv, dl,
					 N, data, saw));
    for (j=0; j<N; j++) {
	assert(saw[j]);
    }
}

void test0 (void) {
    int r, i, j;
    MDICT mdict;
    int n_ops=1000;
    int *data=malloc(sizeof(*data)*n_ops);
    char*saw =malloc(sizeof(*saw)*n_ops);
    int  data_n = 0;
    assert(data!=0);
    r = mdict_create(&mdict); assert(r==0);
    assert(mdict_n_entries(mdict)==0);
    {
	bytevec kv=(void*)0xdeadbeef;
	bytevec dv=(void*)0xbeefdead;
	ITEMLEN kl=42, dl=43;
	r = mdict_find_last(mdict,&kv,&kl,&dv,&dl);
	assert(r!=0);
	assert((unsigned long)kv==0xdeadbeef);
	assert((unsigned long)dv==0xbeefdead);
	assert(kl==42);
	assert(dl==43);
    }
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
		mdict_insert(mdict, kv, strlen(kv)+1, dv, strlen(dv)+1);
		data[data_n++]=ra;
	    }
	} else {
	    // Look up something
	}
	verify_mdict(mdict, data_n, data, saw);
    }
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test0();
    return 0;
}
