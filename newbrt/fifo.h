#ifndef FIFO_H
#define FIFO_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brttypes.h"
#include "xids-internal.h"
#include "xids.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// If the fifo_entry is unpacked, the compiler aligns the xids array and we waste a lot of space
#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif

struct __attribute__((__packed__)) fifo_entry {
    unsigned int keylen;
    unsigned int vallen;
    unsigned char type;
    XIDS_S        xids_s;
};

#if TOKU_WINDOWS
#pragma pack(pop)
#endif

typedef struct fifo *FIFO;

int toku_fifo_create(FIFO *);

void toku_fifo_free(FIFO *);

// Use the size hint to size the storage for the fifo entries in anticipation of putting a bunch of them
// into the fifo.
void toku_fifo_size_hint(FIFO, size_t size_hint);

int toku_fifo_n_entries(FIFO);

int toku_fifo_enq_cmdstruct (FIFO fifo, const BRT_MSG cmd);

int toku_fifo_enq (FIFO, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, int type, XIDS xids);

int toku_fifo_peek (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, u_int32_t *type, XIDS *xids);

// int toku_fifo_peek_cmdstruct (FIFO, BRT_MSG, DBT*, DBT*); // fill in the BRT_MSG, using the two DBTs for the DBT part.
int toku_fifo_deq(FIFO);

unsigned long toku_fifo_memory_size(FIFO); // return how much memory the fifo uses.

//These two are problematic, since I don't want to malloc() the bytevecs, but dequeueing the fifo frees the memory.
//int toku_fifo_peek_deq (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, u_int32_t *type, TXNID *xid);
//int toku_fifo_peek_deq_cmdstruct (FIFO, BRT_MSG, DBT*, DBT*); // fill in the BRT_MSG, using the two DBTs for the DBT part.
void toku_fifo_iterate (FIFO, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen,int type, XIDS xids, void*), void*);

#define FIFO_ITERATE(fifo,keyvar,keylenvar,datavar,datalenvar,typevar,xidsvar,body) do {    \
  int fifo_iterate_off;                                                                    \
  for (fifo_iterate_off = toku_fifo_iterate_internal_start(fifo);                          \
       toku_fifo_iterate_internal_has_more(fifo, fifo_iterate_off);			   \
       fifo_iterate_off = toku_fifo_iterate_internal_next(fifo, fifo_iterate_off)) {       \
      struct fifo_entry *e = toku_fifo_iterate_internal_get_entry(fifo, fifo_iterate_off); \
      ITEMLEN keylenvar = e->keylen;                                                       \
      ITEMLEN datalenvar = e->vallen;                                                 \
      int     typevar = e->type;                                                      \
      XIDS    xidsvar = &e->xids_s;                                                   \
      bytevec keyvar  = xids_get_end_of_array(xidsvar);                               \
      bytevec datavar = (const u_int8_t*)keyvar + e->keylen;                          \
      body;                                                                           \
  } } while (0)

// Internal functions for the iterator.
int toku_fifo_iterate_internal_start(FIFO fifo);
int toku_fifo_iterate_internal_has_more(FIFO fifo, int off);
int toku_fifo_iterate_internal_next(FIFO fifo, int off);
struct fifo_entry * toku_fifo_iterate_internal_get_entry(FIFO fifo, int off);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
