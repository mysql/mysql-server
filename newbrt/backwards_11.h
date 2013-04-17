/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef BACKWARD_11_H
#define BACKWARD_11_H

static int upgrade_brtheader_11_12 (int fd, struct brt_header **brth_11, struct brt_header **brth_12);
static int upgrade_brtnode_11_12 (BRTNODE *brtnode_11, BRTNODE *brtnode_12);

static int deserialize_brtheader_11 (int fd, struct rbuf *rb, struct brt_header **brth);

static int decompress_brtnode_from_raw_block_into_rbuf_11(u_int8_t *raw_block, struct rbuf *rb, BLOCKNUM blocknum);
static int deserialize_brtnode_from_rbuf_11 (BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h, struct rbuf *rb);

#endif

