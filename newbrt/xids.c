/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Purpose of this file is to implement xids list of nested transactions
 * ids.
 *
 * See design documentation for nested transactions at
 * TokuWiki/Imp/TransactionsOverview.
 *
 * NOTE: xids are always stored in disk byte order.  
 *       Accessors are responsible for transposing bytes to 
 *       host order.
 */


#include <errno.h>
#include <string.h>

#include <toku_portability.h>
#include "brttypes.h"
#include "xids.h"
#include "xids-internal.h"
#include "toku_assert.h"
#include "memory.h"
#include <toku_htod.h>


/////////////////////////////////////////////////////////////////////////////////
//  This layer of abstraction (xids_xxx) understands xids<> and nothing else.
//  It contains all the functions that understand xids<>
//
//  xids<> do not store the implicit transaction id of 0 at index 0.
//  The accessor functions make the id of 0 explicit at index 0.
//  The number of xids physically stored in the xids array is in
//  the variable num_xids.
//
// The xids struct is immutable.  The caller gets an initial version of XIDS
// by calling xids_get_root_xids(), which returns the constant struct
// representing the root transaction (id 0).  When a transaction begins, 
// a new XIDS is created with the id of the current transaction appended to
// the list.
// 
//


// This is the xids list for a transactionless environment.
// It is also the initial state of any xids list created for
// nested transactions.


XIDS
xids_get_root_xids(void) {
    static const struct xids_t root_xids = {
        .num_xids = 0
    };

    XIDS rval = (XIDS)&root_xids;
    return rval;
} 


// xids is immutable.  This function creates a new xids by copying the 
// parent's list and then appending the xid of the new transaction.
int
xids_create_child(XIDS   parent_xids,		// xids list for parent transaction
		  XIDS * xids_p,		// xids list created
		  TXNID  this_xid) {		// xid of this transaction (new innermost)
    int rval;
    invariant(parent_xids);
    invariant(this_xid > xids_get_innermost_xid(parent_xids));
    u_int32_t num_child_xids = parent_xids->num_xids + 1;
    invariant(num_child_xids > 0);
    invariant(num_child_xids <= MAX_TRANSACTION_RECORDS);
    if (num_child_xids == MAX_TRANSACTION_RECORDS) rval = EINVAL;
    else {
        XIDS xids = toku_malloc(sizeof(*xids) + num_child_xids*sizeof(xids->ids[0]));
        if (!xids) rval = ENOMEM;
        else {
            xids->num_xids = num_child_xids;
            memcpy(xids->ids,
                   parent_xids->ids,
                   parent_xids->num_xids*sizeof(parent_xids->ids[0])); 
            TXNID this_xid_disk = toku_htod64(this_xid);
            xids->ids[num_child_xids-1] = this_xid_disk;
            *xids_p = xids;
            rval = 0;
        }
    }
    return rval;
}

void
xids_create_from_buffer(struct rbuf *rb,		// xids list for parent transaction
		        XIDS * xids_p) {		// xids list created
    u_int32_t num_xids = rbuf_char(rb);
    invariant(num_xids < MAX_TRANSACTION_RECORDS);
    XIDS xids = toku_xmalloc(sizeof(*xids) + num_xids*sizeof(xids->ids[0]));
    xids->num_xids = num_xids;
    u_int8_t index;
    for (index = 0; index < xids->num_xids; index++) {
        rbuf_TXNID(rb, &xids->ids[index]);
        if (index > 0)
            assert(xids->ids[index] > xids->ids[index-1]);
    }
    *xids_p = xids;
}


void
xids_destroy(XIDS *xids_p) {
    if (*xids_p != xids_get_root_xids()) toku_free(*xids_p);
    *xids_p = NULL;
}


// Return xid at requested position.  
// If requesting an xid out of range (which will be the case if xids array is empty)
// then return 0, the xid of the root transaction.
TXNID 
xids_get_xid(XIDS xids, u_int8_t index) {
    invariant(index < xids_get_num_xids(xids));
    TXNID rval = xids->ids[index];
    rval = toku_dtoh64(rval);
    return rval;
}

// This function assumes that target_xid IS in the list
// of xids.
u_int8_t 
xids_find_index_of_xid(XIDS xids, TXNID target_xid) {
    u_int8_t index = 0;  // search outer to inner
    TXNID current_xid = xids_get_xid(xids, index);
    while (current_xid != target_xid) {
        invariant(current_xid < target_xid);
        index++;
        current_xid = xids_get_xid(xids, index); // Next inner txnid in xids.
    }
    return index;
}

u_int8_t 
xids_get_num_xids(XIDS xids) {
    u_int8_t rval = xids->num_xids;
    return rval;
}


// Return innermost xid 
TXNID 
xids_get_innermost_xid(XIDS xids) {
    TXNID rval = TXNID_NONE;
    if (xids_get_num_xids(xids)) {
        rval = xids_get_xid(xids, xids_get_num_xids(xids)-1);
    }
    return rval;
}

TXNID
xids_get_outermost_xid(XIDS xids) {
    TXNID rval = TXNID_NONE;
    if (xids_get_num_xids(xids))
        rval = xids_get_xid(xids, 0);
    return rval;
}

void
xids_cpy(XIDS target, XIDS source) {
    size_t size = xids_get_size(source);
    memcpy(target, source, size);
}

// return size in bytes
u_int32_t 
xids_get_size(XIDS xids){
    u_int32_t rval;
    u_int8_t num_xids = xids->num_xids;
    rval = sizeof(*xids) + num_xids * sizeof(xids->ids[0]);
    return rval;
}

u_int32_t 
xids_get_serialize_size(XIDS xids){
    u_int32_t rval;
    u_int8_t num_xids = xids->num_xids;
    rval = 1 + //num xids
           8 * num_xids;
    return rval;
}


// Include TXNID zero in checksum to maintain compatibility
// with previously released version.  
void
toku_calc_more_murmur_xids (struct x1764 *mm, XIDS xids) {
    x1764_add(mm, &xids->num_xids, 1);
    TXNID zero = 0;
    x1764_add(mm, &zero, 8);
    u_int8_t index;
    u_int8_t num_xids = xids_get_num_xids(xids);
    for (index = 0; index < num_xids; index++) {
        TXNID current_xid = xids_get_xid(xids, index);
        x1764_add(mm, &current_xid, 8);
    }
}

unsigned char *
xids_get_end_of_array(XIDS xids) {
    TXNID *r = xids->ids + xids->num_xids;
    return (unsigned char*)r;
}

void wbuf_xids(struct wbuf *wb, XIDS xids) {
    wbuf_char(wb, (unsigned char)xids->num_xids);
    u_int8_t index;
    for (index = 0; index < xids->num_xids; index++) {
        wbuf_TXNID(wb, xids->ids[index]);
    }
}

void wbuf_nocrc_xids(struct wbuf *wb, XIDS xids) {
    wbuf_nocrc_char(wb, (unsigned char)xids->num_xids);
    u_int8_t index;
    for (index = 0; index < xids->num_xids; index++) {
        wbuf_nocrc_TXNID(wb, xids->ids[index]);
    }
}

void
xids_fprintf(FILE* fp, XIDS xids) {
    u_int8_t index;
    unsigned num_xids = xids_get_num_xids(xids);
    fprintf(fp, "[|%u| ", num_xids);
    for (index = 0; index < xids_get_num_xids(xids); index++) {
        if (index) fprintf(fp, ",");
        fprintf(fp, "%"PRIx64, xids_get_xid(xids, index));
    }
    fprintf(fp, "]");
}

