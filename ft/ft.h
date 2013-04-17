/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_H
#define FT_H
#ident "$Id: ft.h 43422 2012-05-12 17:51:02Z zardosht $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
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
int toku_ft_unlink_on_commit(FT_HANDLE handle, TOKUTXN txn) __attribute__((__warn_unused_result__));

//Effect: suppresses rollback logs
void toku_ft_suppress_rollbacks(FT h, TOKUTXN txn);

void toku_ft_init_treelock(FT h);
void toku_ft_destroy_treelock(FT h);
void toku_ft_grab_treelock(FT h);
void toku_ft_release_treelock(FT h);

void toku_ft_init_reflock(FT ft);
void toku_ft_destroy_reflock(FT ft);
void toku_ft_grab_reflock(FT ft);
void toku_ft_release_reflock(FT ft);

int toku_create_new_ft(FT *ftp, FT_OPTIONS options, CACHEFILE cf, TOKUTXN txn);
void toku_ft_free (FT h);

int toku_read_ft_and_store_in_cachefile (FT_HANDLE brt, CACHEFILE cf, LSN max_acceptable_lsn, FT *header, BOOL* was_open);
void toku_ft_note_ft_handle_open(FT ft, FT_HANDLE live);

bool toku_ft_needed_unlocked(FT ft);
bool toku_ft_has_one_reference_unlocked(FT ft);

// evict a ft from memory by closing its cachefile. any future work
// will have to read in the ft in a new cachefile and new FT object.
int toku_ft_evict_from_memory(FT ft, char **error_string, BOOL oplsn_valid, LSN oplsn)  __attribute__ ((warn_unused_result));

FT_HANDLE toku_ft_get_only_existing_ft_handle(FT h);

void toku_ft_note_hot_begin(FT_HANDLE brt);
void toku_ft_note_hot_complete(FT_HANDLE brt, BOOL success, MSN msn_at_start_of_hot);

void
toku_ft_init(
    FT h,
    BLOCKNUM root_blocknum_on_disk,
    LSN checkpoint_lsn,
    TXNID root_xid_that_created,
    uint32_t target_nodesize,
    uint32_t target_basementnodesize,
    enum toku_compression_method compression_method
    );

int toku_dictionary_redirect_abort(FT old_h, FT new_h, TOKUTXN txn) __attribute__ ((warn_unused_result));
int toku_dictionary_redirect (const char *dst_fname_in_env, FT_HANDLE old_ft, TOKUTXN txn);
void toku_reset_root_xid_that_created(FT h, TXNID new_root_xid_that_created);
// Reset the root_xid_that_created field to the given value.  
// This redefines which xid created the dictionary.

void toku_ft_add_txn_ref(FT h);
void toku_ft_remove_txn_ref(FT h);

void toku_calculate_root_offset_pointer ( FT h, CACHEKEY* root_key, u_int32_t *roothash);
void toku_ft_set_new_root_blocknum(FT h, CACHEKEY new_root_key); 
LSN toku_ft_checkpoint_lsn(FT h)  __attribute__ ((warn_unused_result));
int toku_ft_set_panic(FT h, int panic, char *panic_string) __attribute__ ((warn_unused_result));
void toku_ft_stat64 (FT h, struct ftstat64_s *s);

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

#endif
