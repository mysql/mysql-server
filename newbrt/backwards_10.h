/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt-serialize.c 13852 2009-08-05 19:34:12Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef BACKWARD_10_H
#define BACKWARD_10_H

static int deserialize_brtheader_10 (int fd, struct rbuf *rb, brt_header_10 **brth);

static int upgrade_brtheader_10_11 (brt_header_10 **brth_10, brt_header_11 **brth_11);

static int decompress_brtnode_from_raw_block_into_rbuf_10(u_int8_t *raw_block, struct rbuf *rb, BLOCKNUM blocknum);

static int deserialize_brtnode_from_rbuf_10 (BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h, struct rbuf *rb);

#endif
