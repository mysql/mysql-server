/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "xids.h"

struct fifo {
    int n_items_in_fifo;
    char *memory;       // An array of bytes into which fifo_entries are embedded.
    int   memory_size;  // How big is fifo_memory
    int   memory_used;  // How many bytes are in use?
};

const int fifo_initial_size = 4096;
static void fifo_init(struct fifo *fifo) {
    fifo->n_items_in_fifo = 0;
    fifo->memory       = 0;
    fifo->memory_size  = 0;
    fifo->memory_used  = 0;
}

static int fifo_entry_size(struct fifo_entry *entry) {
    return sizeof (struct fifo_entry) + entry->keylen + entry->vallen
                  + xids_get_size(&entry->xids_s)
                  - sizeof(XIDS_S); //Prevent double counting from fifo_entry+xids_get_size
}

int toku_fifo_create(FIFO *ptr) {
    struct fifo *XMALLOC(fifo);
    if (fifo == 0) return ENOMEM;
    fifo_init(fifo);
    *ptr = fifo;
    return 0;
}

void toku_fifo_free(FIFO *ptr) {
    FIFO fifo = *ptr;
    if (fifo->memory) toku_free(fifo->memory);
    fifo->memory=0;
    toku_free(fifo);
    *ptr = 0;
}

int toku_fifo_n_entries(FIFO fifo) {
    return fifo->n_items_in_fifo;
}

static int next_power_of_two (int n) {
    int r = 4096;
    while (r < n) {
	r*=2;
	assert(r>0);
    }
    return r;
}

int toku_fifo_enq(FIFO fifo, const void *key, unsigned int keylen, const void *data, unsigned int datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, long *dest) {
    int need_space_here = sizeof(struct fifo_entry)
                          + keylen + datalen
                          + xids_get_size(xids)
                          - sizeof(XIDS_S); //Prevent double counting
    int need_space_total = fifo->memory_used+need_space_here;
    if (fifo->memory == NULL) {
        fifo->memory_size = next_power_of_two(need_space_total);
        XMALLOC_N(fifo->memory_size, fifo->memory);
    }
    if (need_space_total > fifo->memory_size) {
        // Out of memory at the end.
        int next_2 = next_power_of_two(need_space_total);
        if ((2*next_2 > fifo->memory_size)
            || (8*next_2 < fifo->memory_size)) {
            // resize the fifo
            char *XMALLOC_N(next_2, newmem);
            char *oldmem = fifo->memory;
            if (newmem==0) return ENOMEM;
            memcpy(newmem, oldmem, fifo->memory_used);
            fifo->memory_size = next_2;
            fifo->memory = newmem;
            toku_free(oldmem);
        }
    }
    struct fifo_entry *entry = (struct fifo_entry *)(fifo->memory + fifo->memory_used);
    fifo_entry_set_msg_type(entry, type);
    entry->msn = msn;
    xids_cpy(&entry->xids_s, xids);
    entry->is_fresh = is_fresh;
    entry->keylen = keylen;
    unsigned char *e_key = xids_get_end_of_array(&entry->xids_s);
    memcpy(e_key, key, keylen);
    entry->vallen = datalen;
    memcpy(e_key + keylen, data, datalen);
    if (dest) {
        *dest = fifo->memory_used;
    }
    fifo->n_items_in_fifo++;
    fifo->memory_used += need_space_here;
    return 0;
}

int toku_fifo_iterate_internal_start(FIFO UU(fifo)) { return 0; }
int toku_fifo_iterate_internal_has_more(FIFO fifo, int off) { return off < fifo->memory_used; }
int toku_fifo_iterate_internal_next(FIFO fifo, int off) {
    struct fifo_entry *e = (struct fifo_entry *)(fifo->memory + off);
    return off + fifo_entry_size(e);
}
struct fifo_entry * toku_fifo_iterate_internal_get_entry(FIFO fifo, int off) {
    return (struct fifo_entry *)(fifo->memory + off);
}

void toku_fifo_iterate (FIFO fifo, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, void*), void *arg) {
    FIFO_ITERATE(fifo,
                 key, keylen, data, datalen, type, msn, xids, is_fresh,
                 f(key,keylen,data,datalen,type,msn,xids,is_fresh, arg));
}

unsigned int toku_fifo_buffer_size_in_use (FIFO fifo) {
    return fifo->memory_used;
}

unsigned long toku_fifo_memory_size_in_use(FIFO fifo) {
    return sizeof(*fifo)+fifo->memory_used;
}

unsigned long toku_fifo_memory_footprint(FIFO fifo) {
    size_t size_used = toku_memory_footprint(fifo->memory, fifo->memory_used);
    long rval = sizeof(*fifo) + size_used; 
    return rval;
}

DBT *fill_dbt_for_fifo_entry(DBT *dbt, const struct fifo_entry *entry) {
    return toku_fill_dbt(dbt, xids_get_end_of_array((XIDS) &entry->xids_s), entry->keylen);
}

const struct fifo_entry *toku_fifo_get_entry(FIFO fifo, long off) {
    return toku_fifo_iterate_internal_get_entry(fifo, off);
}

void toku_fifo_clone(FIFO orig_fifo, FIFO* cloned_fifo) {
    struct fifo *XMALLOC(new_fifo);
    assert(new_fifo);
    new_fifo->n_items_in_fifo = orig_fifo->n_items_in_fifo;
    new_fifo->memory_used = orig_fifo->memory_used;
    new_fifo->memory_size = new_fifo->memory_used;
    XMALLOC_N(new_fifo->memory_size, new_fifo->memory);
    memcpy(
        new_fifo->memory, 
        orig_fifo->memory, 
        new_fifo->memory_size
        );
    *cloned_fifo = new_fifo;
}

BOOL toku_are_fifos_same(FIFO fifo1, FIFO fifo2) {
    return (
        fifo1->memory_used == fifo2->memory_used &&
        memcmp(fifo1->memory, fifo2->memory, fifo1->memory_used) == 0
        );
}
