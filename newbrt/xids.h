/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

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

#include "x1764.h"

#include "rbuf.h"
#include "wbuf.h"
/* The number of transaction ids stored in the xids structure is 
 * represented by an 8-bit value.  The value 255 is reserved. 
 * The constant MAX_NESTED_TRANSACTIONS is one less because
 * one slot in the packed leaf entry is used for the implicit
 * root transaction (id 0).
 */
enum {MAX_NESTED_TRANSACTIONS = 253};
enum {MAX_TRANSACTION_RECORDS = MAX_NESTED_TRANSACTIONS + 1};	


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

// return size in bytes
u_int32_t xids_get_size(XIDS xids);

u_int32_t xids_get_serialize_size(XIDS xids);

void toku_calc_more_murmur_xids (struct x1764 *mm, XIDS xids);

unsigned char *xids_get_end_of_array(XIDS xids);

void wbuf_xids(struct wbuf *wb, XIDS xids);

#endif 
