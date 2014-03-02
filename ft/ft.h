/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_H
#define FT_H
#ident "$Id$"
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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "fttypes.h"
#include "ybt.h"
#include <db.h>
#include "cachetable.h"
#include "log.h"
#include "ft-search.h"
#include "ft-ops.h"
#include "compress.h"

// unlink a ft from the filesystem with or without a txn.
// if with a txn, then the unlink happens on commit.
void toku_ft_unlink(FT_HANDLE handle);
void toku_ft_unlink_on_commit(FT_HANDLE handle, TOKUTXN txn);

void toku_ft_init_reflock(FT ft);
void toku_ft_destroy_reflock(FT ft);
void toku_ft_grab_reflock(FT ft);
void toku_ft_release_reflock(FT ft);

void toku_ft_create(FT *ftp, FT_OPTIONS options, CACHEFILE cf, TOKUTXN txn);
void toku_ft_free (FT h);

int toku_read_ft_and_store_in_cachefile (FT_HANDLE ft_h, CACHEFILE cf, LSN max_acceptable_lsn, FT *header);
void toku_ft_note_ft_handle_open(FT ft, FT_HANDLE live);

bool toku_ft_needed_unlocked(FT ft);
bool toku_ft_has_one_reference_unlocked(FT ft);

// evict a ft from memory by closing its cachefile. any future work
// will have to read in the ft in a new cachefile and new FT object.
void toku_ft_evict_from_memory(FT ft, bool oplsn_valid, LSN oplsn);

FT_HANDLE toku_ft_get_only_existing_ft_handle(FT h);

void toku_ft_note_hot_begin(FT_HANDLE ft_h);
void toku_ft_note_hot_complete(FT_HANDLE ft_h, bool success, MSN msn_at_start_of_hot);

void
toku_ft_init(
    FT ft,
    BLOCKNUM root_blocknum_on_disk,
    LSN checkpoint_lsn,
    TXNID root_xid_that_created,
    uint32_t target_nodesize,
    uint32_t target_basementnodesize,
    enum toku_compression_method compression_method,
    uint32_t fanout
    );

int toku_dictionary_redirect_abort(FT old_h, FT new_h, TOKUTXN txn) __attribute__ ((warn_unused_result));
int toku_dictionary_redirect (const char *dst_fname_in_env, FT_HANDLE old_ft, TOKUTXN txn);
void toku_reset_root_xid_that_created(FT h, TXNID new_root_xid_that_created);
// Reset the root_xid_that_created field to the given value.
// This redefines which xid created the dictionary.

void toku_ft_add_txn_ref(FT h);
void toku_ft_remove_txn_ref(FT h);

void toku_calculate_root_offset_pointer ( FT h, CACHEKEY* root_key, uint32_t *roothash);
void toku_ft_set_new_root_blocknum(FT h, CACHEKEY new_root_key);
LSN toku_ft_checkpoint_lsn(FT h)  __attribute__ ((warn_unused_result));
void toku_ft_stat64 (FT h, struct ftstat64_s *s);
void toku_ft_get_fractal_tree_info64 (FT h, struct ftinfo64 *s);
int toku_ft_iterate_fractal_tree_block_map(FT ft, int (*iter)(uint64_t,int64_t,int64_t,int64_t,int64_t,void*), void *iter_extra);

// unconditionally set the descriptor for an open FT. can't do this when
// any operation has already occurred on the ft.
// see toku_ft_change_descriptor(), which is the transactional version
// used by the ydb layer. it better describes the client contract.
void toku_ft_update_descriptor(FT ft, DESCRIPTOR d);
// use this version if the FT is not fully user-opened with a valid cachefile.
// this is a clean hack to get deserialization code to update a descriptor
// while the FT and cf are in the process of opening, for upgrade purposes
void toku_ft_update_descriptor_with_fd(FT ft, DESCRIPTOR d, int fd);
void toku_ft_update_cmp_descriptor(FT ft);

// get the descriptor for a ft. safe to read as long as clients honor the
// strict contract put forth by toku_ft_update_descriptor/toku_ft_change_descriptor
// essentially, there should never be a reader while there is a writer, enforced
// by the client, not the FT.
DESCRIPTOR toku_ft_get_descriptor(FT_HANDLE ft_handle);
DESCRIPTOR toku_ft_get_cmp_descriptor(FT_HANDLE ft_handle);

void toku_ft_update_stats(STAT64INFO headerstats, STAT64INFO_S delta);
void toku_ft_decrease_stats(STAT64INFO headerstats, STAT64INFO_S delta);

void toku_ft_remove_reference(FT ft,
                              bool oplsn_valid, LSN oplsn,
                              remove_ft_ref_callback remove_ref, void *extra);

void toku_ft_set_nodesize(FT ft, unsigned int nodesize);
void toku_ft_get_nodesize(FT ft, unsigned int *nodesize);
void toku_ft_set_basementnodesize(FT ft, unsigned int basementnodesize);
void toku_ft_get_basementnodesize(FT ft, unsigned int *basementnodesize);
void toku_ft_set_compression_method(FT ft, enum toku_compression_method method);
void toku_ft_get_compression_method(FT ft, enum toku_compression_method *methodp);
void toku_ft_set_fanout(FT ft, unsigned int fanout);
void toku_ft_get_fanout(FT ft, unsigned int *fanout);
void toku_node_save_ct_pair(CACHEKEY UU(key), void *value_data, PAIR p);

// mark the ft as a blackhole. any message injections will be a no op.
void toku_ft_set_blackhole(FT_HANDLE ft_handle);

// Effect: Calculates the total space and used space for a FT's leaf data.
//         The difference between the two is MVCC garbage.
void toku_ft_get_garbage(FT ft, uint64_t *total_space, uint64_t *used_space);

int get_num_cores(void);
struct toku_thread_pool *get_ft_pool(void);
void dump_bad_block(unsigned char *vp, uint64_t size);

int toku_single_process_lock(const char *lock_dir, const char *which, int *lockfd);

int toku_single_process_unlock(int *lockfd);

void tokudb_update_product_name_strings(void);
#define TOKU_MAX_PRODUCT_NAME_LENGTH (256)
extern char toku_product_name[TOKU_MAX_PRODUCT_NAME_LENGTH];

struct toku_product_name_strings_struct {
    char db_version[sizeof(toku_product_name) + sizeof("1.2.3 build ") + 256];
    char environmentdictionary[sizeof(toku_product_name) + sizeof(".environment")];
    char fileopsdirectory[sizeof(toku_product_name) + sizeof(".directory")];
    char single_process_lock[sizeof(toku_product_name) + sizeof("___lock_dont_delete_me")];
    char rollback_cachefile[sizeof(toku_product_name) + sizeof(".rollback")];
};

extern struct toku_product_name_strings_struct toku_product_name_strings;
extern int tokudb_num_envs;
#endif
