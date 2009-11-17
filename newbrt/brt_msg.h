/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


/* The purpose of this file is to provide access to the brt_msg,
 * which is the ephemeral version of the fifo_msg.
 */




#ifndef BRT_MSG_H
#define BRT_MSG_H

u_int32_t brt_msg_get_keylen(BRT_MSG brt_msg);

u_int32_t brt_msg_get_vallen(BRT_MSG brt_msg);

XIDS brt_msg_get_xids(BRT_MSG brt_msg);

void * brt_msg_get_key(BRT_MSG brt_msg);

void * brt_msg_get_val(BRT_MSG brt_msg);

enum brt_msg_type brt_msg_get_type(BRT_MSG brt_msg);

void brt_msg_from_fifo_msg(BRT_MSG brt_msg, FIFO_MSG fifo_msg);

#if 0

void brt_msg_from_dbts(BRT_MSG brt_msg, DBT *key, DBT *val, XIDS xids, enum brt_msg_type type);

#endif

#endif

