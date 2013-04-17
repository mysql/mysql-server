/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


/* The purpose of this file is to provide access to the fifo_msg,
 * which is the stored representation of the brt_msg.
 * 
 * NOTE: Accessor functions return all values in host byte order.
 */




#ifndef FIFO_MSG_H
#define FIFO_MSG_H

u_int32_t fifo_msg_get_keylen(FIFO_MSG fifo_msg);

u_int32_t fifo_msg_get_vallen(FIFO_MSG fifo_msg);

XIDS fifo_msg_get_xids(FIFO_MSG fifo_msg);

void * fifo_msg_get_key(FIFO_MSG fifo_msg);

void * fifo_msg_get_val(FIFO_MSG fifo_msg);

enum brt_msg_type fifo_msg_get_type(FIFO_MSG fifo_msg);

u_int32_t fifo_msg_get_size(FIFO_MSG fifo_msg);

// Return number of bytes required for a fifo_msg created from
// the given brt_msg
u_int32_t fifo_msg_get_size_required(BRT_MSG brt_msg);

#endif

