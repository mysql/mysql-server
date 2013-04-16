/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* The purpose of this file is to provide access to the ft_msg,
 * which is the ephemeral version of the fifo_msg.
 */

#ifndef FT_MSG_H
#define FT_MSG_H

#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


uint32_t ft_msg_get_keylen(FT_MSG ft_msg);

uint32_t ft_msg_get_vallen(FT_MSG ft_msg);

XIDS ft_msg_get_xids(FT_MSG ft_msg);

void * ft_msg_get_key(FT_MSG ft_msg);

void * ft_msg_get_val(FT_MSG ft_msg);

enum ft_msg_type ft_msg_get_type(FT_MSG ft_msg);

void ft_msg_from_fifo_msg(FT_MSG ft_msg, FIFO_MSG fifo_msg);

#if 0

void ft_msg_from_dbts(FT_MSG ft_msg, DBT *key, DBT *val, XIDS xids, enum ft_msg_type type);

#endif



#endif  // FT_MSG_H

