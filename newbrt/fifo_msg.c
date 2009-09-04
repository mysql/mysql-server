/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


/* Purpose of this file is to define and handle the fifo_msg, which
 * is the stored format of a brt_msg.
 *
 * Note, when translating from fifo_msg to brt_msg, the brt_msg
 * will be created with a pointer into the xids in the fifo_msg.
 * (The xids will not be embedded in the brt_msg.)  This means
 * that a valid xids struct must be embedded in the fifo_msg.
 *
 * NOTE: fifo_msg is stored in memory and on disk in same format.
 *       fifo_msg is stored in same byte order both in-memory
 *       and on-disk.  Accessors are responsible for tranposition
 *       to host order.
 */


#include <string.h>

#include <toku_portability.h>
#include "brttypes.h"
#include "xids.h"
#include "xids-internal.h"
#include "brt_msg.h"
#include "fifo_msg.h"
#include <toku_htod.h>


// xids_and_key_and_val field is XIDS_S followed by key
// followed by value.

struct fifo_msg_t {
    u_int32_t keylen;
    u_int32_t vallen;
    u_int8_t  type;
    // u_int8_t pad[7]; // force 64-bit alignment if needed ???
    u_int8_t  xids_and_key_and_val[];	// undifferentiated bytes
};


u_int32_t 
fifo_msg_get_keylen(FIFO_MSG fifo_msg) {
    u_int32_t rval = fifo_msg->keylen;
    rval = toku_dtoh32(rval);
    return rval;
}

u_int32_t 
fifo_msg_get_vallen(FIFO_MSG fifo_msg) {
    u_int32_t rval = fifo_msg->vallen;
    rval = toku_dtoh32(rval);
    return rval;
}

XIDS
fifo_msg_get_xids(FIFO_MSG fifo_msg) {
    XIDS rval = (XIDS) &fifo_msg->xids_and_key_and_val;
    return rval;
}


static u_int32_t
fifo_msg_get_xids_size(FIFO_MSG fifo_msg) {
    u_int32_t rval;
    XIDS xids = fifo_msg_get_xids(fifo_msg);
    rval      = xids_get_size(xids);
    return rval;
}


void *
fifo_msg_get_key(FIFO_MSG fifo_msg) {
    void * rval;
    u_int32_t xidslen = fifo_msg_get_xids_size(fifo_msg);
    rval = (u_int8_t*)fifo_msg->xids_and_key_and_val + xidslen;
    return rval;
}

void *
fifo_msg_get_val(FIFO_MSG fifo_msg) {
    void * rval;
    void * key = fifo_msg_get_key(fifo_msg);
    u_int32_t keylen  = fifo_msg_get_keylen(fifo_msg);
    rval = (u_int8_t*)key + keylen;
    return rval;
}

enum brt_msg_type
fifo_msg_get_type(FIFO_MSG fifo_msg) {
    enum brt_msg_type rval = (enum brt_msg_type) fifo_msg->type;
    return rval;
}


// Finds size of a fifo msg.
u_int32_t 
fifo_msg_get_size(FIFO_MSG fifo_msg) {
    u_int32_t rval;
    u_int32_t keylen  = fifo_msg_get_keylen(fifo_msg);
    u_int32_t vallen  = fifo_msg_get_vallen(fifo_msg);
    u_int32_t xidslen = fifo_msg_get_xids_size(fifo_msg);
    rval = keylen + vallen + xidslen + sizeof(*fifo_msg);
    return rval;
}

// Return number of bytes required for a fifo_msg created from
// the given brt_msg
u_int32_t 
fifo_msg_get_size_required(BRT_MSG brt_msg) {
    u_int32_t rval;
    u_int32_t keylen  = brt_msg_get_keylen(brt_msg);
    u_int32_t vallen  = brt_msg_get_vallen(brt_msg);
    XIDS      xids    = brt_msg_get_xids(brt_msg);
    u_int32_t xidslen = xids_get_size(xids);
    rval = keylen + vallen + xidslen + sizeof(struct fifo_msg_t);
    return rval;
}

