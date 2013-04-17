/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."

/* See merger.h for a description of this module. */

#include <toku_portability.h>
#include "brttypes.h"
#include "merger.h"
#include <memory.h>
#include <toku_assert.h>
#include <string.h>

struct merger {
    int n_files;
    FILE **files;
    BOOL *present;// one for each file: present[i] is TRUE if and only if keys[i] and vals[i] are valid.  (If the file gets empty then files[i] will be NULL)
    DBT *keys; // one for each file.   ulen keeps track of how much memory is actually allocated. 
    DBT *vals; // one for each file.
    DB  *db;
    COMPARISON_FUNCTION cf;
    MEMORY_ALLOCATION_UPDATER mup;
    
};

MERGER create_merger (int n_files, char *file_names[n_files], DB *db, COMPARISON_FUNCTION cf, MEMORY_ALLOCATION_UPDATER mup) {
    MERGER MALLOC(result);
    result->n_files = n_files;
    MALLOC_N(n_files, result->files);
    MALLOC_N(n_files, result->present);
    MALLOC_N(n_files, result->keys);
    MALLOC_N(n_files, result->vals);
    for (int i=0; i<n_files; i++) {
	result->files[i] = fopen(file_names[i], "r");
	assert(result->files[i]);
	result->present[i] = FALSE;
	memset(&result->keys[i], 0, sizeof(result->keys[0]));
	memset(&result->vals[i], 0, sizeof(result->vals[0]));
    }
    result->db  = db;
    result->cf  = cf;
    result->mup = mup;
    return result;
}

void merger_close (MERGER m) {
    for (int i=0; i<m->n_files; i++) {
	if (m->files[i]) {
	    int r = fclose(m->files[i]);
	    assert(r==0);
	}
	if (m->keys[i].data) {
	    toku_free(m->keys[i].data);
	}
	if (m->vals[i].data) {
	    toku_free(m->vals[i].data);
	}
    }
    toku_free(m->files);
    toku_free(m->present);
    toku_free(m->keys);
    toku_free(m->vals);
    toku_free(m);
}

static int merge_fill_dbt (MERGER m, int i)
// Effect: Make sure that keys[i] has data in it and return 0.
// If we cannot, then return nonzero.
{
    if (m->present[i]) return 0; // it's there, so we are OK.
    if (m->files[i]==NULL) return -1; // the file was previously empty, so no more.
    u_int32_t keylen;
    {
	int n = fread(&keylen, sizeof(keylen), 1, m->files[i]);
	if (n!=1) {
	    // must have hit EOF, so close the file and set return -1.
	    int r = fclose(m->files[i]);
	    assert(r==0);
	    m->files[i] = NULL;
	    return -1;
	}
    }
    // Got something, so we should be able to get the rest.
    if (m->keys[i].ulen < keylen) {
	m->keys[i].data = toku_xrealloc(m->keys[i].data, keylen);
	m->keys[i].ulen = keylen;
    }
    {
	size_t n = fread(m->keys[i].data, 1, keylen, m->files[i]);
	assert(n==keylen);
    }
    u_int32_t vallen;
    {
	int n = fread(&vallen, sizeof(vallen), 1, m->files[i]);
	assert(n==1);
    }
    if (m->vals[i].ulen < vallen) {
	m->vals[i].data = toku_xrealloc(m->vals[i].data, vallen);
	m->vals[i].ulen = vallen;
    }
    {
	size_t n = fread(m->vals[i].data, 1, vallen, m->files[i]);
	assert(n==vallen);
    }
    m->keys[i].size = keylen;
    m->vals[i].size = vallen;
    m->present[i] = TRUE;
    return 0;
}

static int find_first_nonempty (MERGER m, int *besti) {
    for (int i=0; i<m->n_files; i++) {
	if (merge_fill_dbt(m, i)==0) {
	    *besti = i;
	    return 0;
	}
    }
    return -1;
}

int merger_pop (MERGER m,
		/*out*/ DBT *key,
		/*out*/ DBT *val)
// This version is as simple as I can make it.
{
    int firsti = -1;
    if (find_first_nonempty(m, &firsti)) {
	// there are no more nonempty rows.
	return -1;
    }
    int besti = firsti;
    // besti is the first nonempty item.
    for (int i=firsti+1; i<m->n_files; i++) {
	if (merge_fill_dbt(m, i)==0) {
	    // there is something there, so we continue 
	    if (m->cf(m->db, &m->keys[besti], &m->keys[i])>0) {
		// then i is the new besti.
		besti = i;
	    }
	}
    }
    // Now besti is the one to return.
    *key = m->keys[besti];
    *val = m->vals[besti];
    m->present[besti] = FALSE;
    return 0;
}

