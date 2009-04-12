#ifndef FIFO_H
#define FIFO_H
#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brttypes.h"

struct fifo_entry {
    unsigned int keylen;
    unsigned int vallen;
    unsigned char type;
    TXNID xid;
    unsigned char key[];
};

typedef struct fifo *FIFO;

int toku_fifo_create(FIFO *);
void toku_fifo_free(FIFO *);
int toku_fifo_n_entries(FIFO);
int toku_fifo_enq_cmdstruct (FIFO fifo, const BRT_CMD cmd);
int toku_fifo_enq (FIFO, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, int type, TXNID xid);
int toku_fifo_peek (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, u_int32_t *type, TXNID *xid);
int toku_fifo_peek_cmdstruct (FIFO, BRT_CMD, DBT*, DBT*); // fill in the BRT_CMD, using the two DBTs for the DBT part.
int toku_fifo_deq(FIFO);

unsigned long toku_fifo_memory_size(FIFO); // return how much memory the fifo uses.

//These two are problematic, since I don't want to malloc() the bytevecs, but dequeueing the fifo frees the memory.
//int toku_fifo_peek_deq (FIFO, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, u_int32_t *type, TXNID *xid);
//int toku_fifo_peek_deq_cmdstruct (FIFO, BRT_CMD, DBT*, DBT*); // fill in the BRT_CMD, using the two DBTs for the DBT part.
void toku_fifo_iterate (FIFO, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen,int type, TXNID xid, void*), void*);

#define FIFO_ITERATE(fifo,keyvar,keylenvar,datavar,datalenvar,typevar,xidvar,body) do {    \
  int fifo_iterate_off;                                                                    \
  for (fifo_iterate_off = toku_fifo_iterate_internal_start(fifo);                          \
       toku_fifo_iterate_internal_has_more(fifo, fifo_iterate_off);			   \
       fifo_iterate_off = toku_fifo_iterate_internal_next(fifo, fifo_iterate_off)) {       \
      struct fifo_entry *e = toku_fifo_iterate_internal_get_entry(fifo, fifo_iterate_off); \
      bytevec keyvar = e->key;                                                             \
      ITEMLEN keylenvar = e->keylen;                                                       \
      bytevec datavar = e->key + e->keylen;                                           \
      ITEMLEN datalenvar = e->vallen;                                                 \
      int     typevar = e->type;                                                      \
      TXNID   xidvar = e->xid;                                                        \
      body;                                                                           \
  } } while (0)

// Internal functions for the iterator.
int toku_fifo_iterate_internal_start(FIFO fifo);
int toku_fifo_iterate_internal_has_more(FIFO fifo, int off);
int toku_fifo_iterate_internal_next(FIFO fifo, int off);
struct fifo_entry * toku_fifo_iterate_internal_get_entry(FIFO fifo, int off);


#endif
