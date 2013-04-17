/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."


#include <toku_portability.h>
#include "fttypes.h"
#include "xids.h"
#include "ft_msg.h"


uint32_t 
ft_msg_get_keylen(FT_MSG ft_msg) {
    uint32_t rval = ft_msg->u.id.key->size;
    return rval;
}

uint32_t 
ft_msg_get_vallen(FT_MSG ft_msg) {
    uint32_t rval = ft_msg->u.id.val->size;
    return rval;
}

XIDS
ft_msg_get_xids(FT_MSG ft_msg) {
    XIDS rval = ft_msg->xids;
    return rval;
}

void *
ft_msg_get_key(FT_MSG ft_msg) {
    void * rval = ft_msg->u.id.key->data;
    return rval;
}

void *
ft_msg_get_val(FT_MSG ft_msg) {
    void * rval = ft_msg->u.id.val->data;
    return rval;
}

enum ft_msg_type
ft_msg_get_type(FT_MSG ft_msg) {
    enum ft_msg_type rval = ft_msg->type;
    return rval;
}

