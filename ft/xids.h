/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "x1764.h"
#include "rbuf.h"
#include "wbuf.h"
#include "tokuconst.h"


//Retrieve an XIDS representing the root transaction.
XIDS xids_get_root_xids(void);

void xids_cpy(XIDS target, XIDS source);

//Creates an XIDS representing this transaction.
//You must pass in an XIDS representing the parent of this transaction.
int  xids_create_child(XIDS parent_xids, XIDS *xids_p, TXNID this_xid);

// The following two functions (in order) are equivalent to xids_create child,
// but allow you to do most of the work without knowing the new xid.
int xids_create_unknown_child(XIDS parent_xids, XIDS *xids_p);
void xids_finalize_with_child(XIDS xids, TXNID this_xid);

void xids_create_from_buffer(struct rbuf *rb, XIDS * xids_p);

void xids_destroy(XIDS *xids_p);

TXNID xids_get_xid(XIDS xids, uint8_t index);

uint8_t xids_get_num_xids(XIDS xids);

TXNID xids_get_innermost_xid(XIDS xids);
TXNID xids_get_outermost_xid(XIDS xids);

// return size in bytes
uint32_t xids_get_size(XIDS xids);

uint32_t xids_get_serialize_size(XIDS xids);

unsigned char *xids_get_end_of_array(XIDS xids);

void wbuf_nocrc_xids(struct wbuf *wb, XIDS xids);

void xids_fprintf(FILE* fp, XIDS xids);



#endif 
