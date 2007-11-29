/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "mdict.h"
#include "memory.h"
#define USEPMA
#ifdef USEPMA
#include "pma.h"

struct mdict {
    PMA pma;
};

int mdict_create (MDICT* mdict) {
    MDICT result;
    int r;
    MALLOC(result);
    if (result==0) return -1;
    r = toku_pma_create(&result->pma);
    if (r==0) {
	*mdict = result;
    }
    return r;
}
	
void mdict_free (MDICT m) {
    toku_pma_free(m->pma);
    my_free(m);
}

int  mdict_n_entries (MDICT m) {
    return toku_pma_n_entries(m->pma);
}
    

/* Returns an error if the key is already present. */
/* The values returned should not be modified. */
/* May damage the cursor. */
int mdict_insert (MDICT m, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen) {
    return toku_pma_insert(m->pma, key, keylen, data, datalen);
}
/* This returns an error if the key is NOT present. */
int mdict_replace (MDICT, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
/* This returns an error if the key is NOT present. */
int mdict_delete (MDICT m, bytevec key, ITEMLEN keylen) {
    return toku_pma_delete(m->pma, key, keylen);
}

/* Exposes internals of the MDICT by returning a pointer to the guts.
 * Don't modify the returned data.  Don't free it. */
int mdict_lookup (MDICT m, bytevec key, ITEMLEN keylen, bytevec*data, ITEMLEN *datalen) {
    return toku_pma_lookup(m->pma, key, keylen, data, datalen);
}


int mdict_random_pick(MDICT m, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen) {
    return toku_pma_random_pick(m->pma, key, keylen, data, datalen);
}

void mdict_iterate (MDICT m, void(*f)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*v) {
    toku_pma_iterate(m->pma, f, v);
}


#else
foo
#endif
