/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

#if 0
int toku_keycompare (bytevec key1b, ITEMLEN key1len, bytevec key2b, ITEMLEN key2len) {
    const unsigned char *key1 = key1b;
    const unsigned char *key2 = key2b;
    while (key1len > 0 && key2len > 0) {
	unsigned char b1 = key1[0];
	unsigned char b2 = key2[0];
	if (b1<b2) return -1;
	if (b1>b2) return 1;
	key1len--; key1++;
	key2len--; key2++;
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}

#elif 0
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	return -toku_keycompare(key2,key2len,key1,key1len);
    }
}
#elif 0

int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	int r = memcmp(key1,key2,key2len);
	if (r>=0) return 1; /* If the keys are the same up to 2's length, then return 1 since key1 is longer than key2 */
	else return -1;
    }
}
#elif 0
/* This one looks tighter, but it does use memcmp... */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    int comparelen = key1len<key2len ? key1len : key2len;
    const unsigned char *k1;
    const unsigned char *k2;
    for (k1=key1, k2=key2;
	 comparelen>0;
	 k1++, k2++, comparelen--) {
	if (*k1 != *k2) {
	    return (int)*k1-(int)*k2;
	}
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}
#else
/* unroll that one four times */
// when a and b are chars, return a-b is safe here because return type is int.  No over/underflow possible.
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    int comparelen = key1len<key2len ? key1len : key2len;
    const unsigned char *k1;
    const unsigned char *k2;
    for (k1=key1, k2=key2;
	 comparelen>4;
	 k1+=4, k2+=4, comparelen-=4) {
	{ int v1=k1[0], v2=k2[0]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[1], v2=k2[1]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[2], v2=k2[2]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[3], v2=k2[3]; if (v1!=v2) return v1-v2; }
    }
    for (;
	 comparelen>0;
	 k1++, k2++, comparelen--) {
	if (*k1 != *k2) {
	    return (int)*k1-(int)*k2;
	}
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}

#endif

int
toku_default_compare_fun (DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

int
toku_dont_call_this_compare_fun (DB *db __attribute__((__unused__)), const DBT *a __attribute__((__unused__)), const DBT*b __attribute__((__unused__))) {
    assert(0);
    return 0;
}
