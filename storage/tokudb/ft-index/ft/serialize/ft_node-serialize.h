/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#include "ft/ft.h"
#include "ft/node.h"
#include "ft/serialize/sub_block.h"
#include "ft/serialize/rbuf.h"
#include "ft/serialize/wbuf.h"
#include "ft/serialize/block_table.h"

unsigned int toku_serialize_ftnode_size(FTNODE node);
int toku_serialize_ftnode_to_memory(FTNODE node, FTNODE_DISK_DATA *ndd,
                                    unsigned int basementnodesize,
                                    enum toku_compression_method compression_method,
                                    bool do_rebalancing, bool in_parallel,
                                    size_t *n_bytes_to_write, size_t *n_uncompressed_bytes,
                                    char **bytes_to_write);
int toku_serialize_ftnode_to(int fd, BLOCKNUM, FTNODE node, FTNODE_DISK_DATA *ndd, bool do_rebalancing, FT ft, bool for_checkpoint);
int toku_serialize_rollback_log_to(int fd, ROLLBACK_LOG_NODE log, SERIALIZED_ROLLBACK_LOG_NODE serialized_log, bool is_serialized,
                                    FT ft, bool for_checkpoint);
void toku_serialize_rollback_log_to_memory_uncompressed(ROLLBACK_LOG_NODE log, SERIALIZED_ROLLBACK_LOG_NODE serialized);

int toku_deserialize_rollback_log_from(int fd, BLOCKNUM blocknum, ROLLBACK_LOG_NODE *logp, FT ft);
int toku_deserialize_bp_from_disk(FTNODE node, FTNODE_DISK_DATA ndd, int childnum, int fd, ftnode_fetch_extra *bfe);
int toku_deserialize_bp_from_compressed(FTNODE node, int childnum, ftnode_fetch_extra *bfe);
int toku_deserialize_ftnode_from(int fd, BLOCKNUM off, uint32_t fullhash, FTNODE *node, FTNODE_DISK_DATA *ndd, ftnode_fetch_extra *bfe);

void toku_serialize_set_parallel(bool);

// used by nonleaf node partial eviction
void toku_create_compressed_partition_from_available(FTNODE node, int childnum,
                                                     enum toku_compression_method compression_method, SUB_BLOCK sb);

// <CER> For verifying old, non-upgraded nodes (versions 13 and 14).
int decompress_from_raw_block_into_rbuf(uint8_t *raw_block, size_t raw_block_size, struct rbuf *rb, BLOCKNUM blocknum);

// used by verify
int deserialize_ft_versioned(int fd, struct rbuf *rb, FT *ft, uint32_t version);
void read_block_from_fd_into_rbuf(int fd, BLOCKNUM blocknum, FT ft, struct rbuf *rb);
int read_compressed_sub_block(struct rbuf *rb, struct sub_block *sb);
int verify_ftnode_sub_block(struct sub_block *sb);
void just_decompress_sub_block(struct sub_block *sb);

// used by ft-node-deserialize.cc
void initialize_ftnode(FTNODE node, BLOCKNUM blocknum);
int read_and_check_magic(struct rbuf *rb);
int read_and_check_version(FTNODE node, struct rbuf *rb);
void read_node_info(FTNODE node, struct rbuf *rb, int version);
void allocate_and_read_partition_offsets(FTNODE node, struct rbuf *rb, FTNODE_DISK_DATA *ndd);
int check_node_info_checksum(struct rbuf *rb);
void read_legacy_node_info(FTNODE node, struct rbuf *rb, int version);
int check_legacy_end_checksum(struct rbuf *rb);

// exported so the loader can dump bad blocks
void dump_bad_block(unsigned char *vp, uint64_t size);
