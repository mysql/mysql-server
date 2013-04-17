/* -*- mode: C; c-basic-offset: 4 -*- */

/* Purpose of this file is to provide the world with everything necessary
 * to use the xids and nothing else.  
 * Internal requirements of the xids logic do not belong here.
 *
 * xids is (abstractly) an immutable list of nested transaction ids, accessed only
 * via the functions in this file.  
 *
 * See design documentation for nested transactions at
 * TokuWiki/Imp/TransactionsOverview.
 */

#ifndef XIDS_H
#define XIDS_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "x1764.h"
#include "rbuf.h"
#include "wbuf.h"
#include "tokuconst.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

//Retrieve an XIDS representing the root transaction.
XIDS xids_get_root_xids(void);

void xids_cpy(XIDS target, XIDS source);

//Creates an XIDS representing this transaction.
//You must pass in an XIDS representing the parent of this transaction.
int  xids_create_child(XIDS parent_xids, XIDS *xids_p, TXNID this_xid);
void xids_create_from_buffer(struct rbuf *rb, XIDS * xids_p);

void xids_destroy(XIDS *xids_p);

TXNID xids_get_xid(XIDS xids, u_int8_t index);

u_int8_t xids_find_index_of_xid(XIDS xids, TXNID target_xid);

u_int8_t xids_get_num_xids(XIDS xids);

TXNID xids_get_innermost_xid(XIDS xids);
TXNID xids_get_outermost_xid(XIDS xids);

// return size in bytes
u_int32_t xids_get_size(XIDS xids);

u_int32_t xids_get_serialize_size(XIDS xids);

void toku_calc_more_murmur_xids (struct x1764 *mm, XIDS xids);

unsigned char *xids_get_end_of_array(XIDS xids);

void wbuf_xids(struct wbuf *wb, XIDS xids);
void wbuf_nocrc_xids(struct wbuf *wb, XIDS xids);

void xids_fprintf(FILE* fp, XIDS xids);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif


#endif 
