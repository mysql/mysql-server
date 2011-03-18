/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."


#include <toku_portability.h>
#include "brttypes.h"
#include "xids.h"
#include "brt_msg.h"


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

