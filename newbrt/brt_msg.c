/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."


#include <toku_portability.h>
#include "brttypes.h"
#include "xids.h"
#include "brt_msg.h"

#if 0

//BRT_MSG internals are in host order
//XIDS are not 'internal' to BRT_MSG

void
brt_msg_from_dbts(BRT_MSG brt_msg,
                  DBT *key, DBT *val,
                  XIDS xids, enum brt_msg_type type) {
    brt_msg->u.id.key = key;
    brt_msg->u.id.val = val;
    brt_msg->xids   = xids;
    brt_msg->type   = type;
}

#endif

//No conversion (from disk to host) is necessary
//Accessor functions for fifo return host order bytes.
#if 0
#include "fifo_msg.h"
void
brt_msg_from_fifo_msg(BRT_MSG brt_msg, FIFO_MSG fifo_msg) {
    brt_msg->keylen = fifo_msg_get_keylen(fifo_msg);
    brt_msg->vallen = fifo_msg_get_vallen(fifo_msg);
    brt_msg->vallen = fifo_msg_get_vallen(fifo_msg);
    brt_msg->key    = fifo_msg_get_key(fifo_msg);
    brt_msg->val    = fifo_msg_get_val(fifo_msg);
    brt_msg->xids   = fifo_msg_get_xids(fifo_msg);
    brt_msg->type   = fifo_msg_get_type(fifo_msg);
}
#endif

u_int32_t 
brt_msg_get_keylen(BRT_MSG brt_msg) {
    u_int32_t rval = brt_msg->u.id.key->size;
    return rval;
}

u_int32_t 
brt_msg_get_vallen(BRT_MSG brt_msg) {
    u_int32_t rval = brt_msg->u.id.val->size;
    return rval;
}

XIDS
brt_msg_get_xids(BRT_MSG brt_msg) {
    XIDS rval = brt_msg->xids;
    return rval;
}

void *
brt_msg_get_key(BRT_MSG brt_msg) {
    void * rval = brt_msg->u.id.key->data;
    return rval;
}

void *
brt_msg_get_val(BRT_MSG brt_msg) {
    void * rval = brt_msg->u.id.val->data;
    return rval;
}

enum brt_msg_type
brt_msg_get_type(BRT_MSG brt_msg) {
    enum brt_msg_type rval = brt_msg->type;
    return rval;
}

