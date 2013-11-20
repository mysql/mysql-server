/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "$Id$"
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
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


// Purpose of this file is to handle all modifications and queries to the database
// at the level of leafentry.  
// 
// ule = Unpacked Leaf Entry
//
// This design unpacks the leafentry into a convenient format, performs all work
// on the unpacked form, then repacks the leafentry into its compact format.
//
// See design documentation for nested transactions at
// TokuWiki/Imp/TransactionsOverview.

#include <toku_portability.h>
#include "fttypes.h"
#include "ft-internal.h"

#include <util/omt.h>

#include "leafentry.h"
#include "xids.h"
#include "ft_msg.h"
#include "ule.h"
#include "txn_manager.h"
#include "ule-internal.h"
#include <util/status.h>


#define ULE_DEBUG 0

static uint32_t ule_get_innermost_numbytes(ULE ule, uint32_t keylen);


///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static LE_STATUS_S le_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(le_status, k, c, t, "le: " l, inc)

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(LE_MAX_COMMITTED_XR,   nullptr, UINT64, "max committed xr", TOKU_ENGINE_STATUS);
    STATUS_INIT(LE_MAX_PROVISIONAL_XR, nullptr, UINT64, "max provisional xr", TOKU_ENGINE_STATUS);
    STATUS_INIT(LE_EXPANDED,           nullptr, UINT64, "expanded", TOKU_ENGINE_STATUS);
    STATUS_INIT(LE_MAX_MEMSIZE,        nullptr, UINT64, "max memsize", TOKU_ENGINE_STATUS);
    le_status.initialized = true;
}
#undef STATUS_INIT

void
toku_le_get_status(LE_STATUS statp) {
    if (!le_status.initialized)
        status_init();
    *statp = le_status;
}

#define STATUS_VALUE(x) le_status.status[x].value.num


///////////////////////////////////////////////////////////////////////////////////
// Accessor functions used by outside world (e.g. indexer)
//

ULEHANDLE 
toku_ule_create(LEAFENTRY le) {
    ULE XMALLOC(ule_p);
    le_unpack(ule_p, le);
    return (ULEHANDLE) ule_p;
}

void toku_ule_free(ULEHANDLE ule_p) {
    ule_cleanup((ULE) ule_p);
    toku_free(ule_p);
}

///////////////////////////////////////////////////////////////////////////////////
//
// Question: Can any software outside this file modify or read a leafentry?  
// If so, is it worthwhile to put it all here?
//
// There are two entries, one each for modification and query:
//   toku_le_apply_msg()        performs all inserts/deletes/aborts
//
//
//
//

//This is what we use to initialize Xuxrs[0] in a new unpacked leafentry.
const UXR_S committed_delete = {
    .type   = XR_DELETE,
    .vallen = 0,
    .valp   = NULL,
    .xid    = 0
};  // static allocation of uxr with type set to committed delete and xid = 0

#define INSERT_LENGTH(len) ((1U << 31) | len)
#define DELETE_LENGTH(len) (0)
#define GET_LENGTH(len) (len & ((1U << 31)-1))
#define IS_INSERT(len)  (len & (1U << 31))
#define IS_VALID_LEN(len) (len < (1U<<31))

// Local functions:

static void msg_init_empty_ule(ULE ule);
static void msg_modify_ule(ULE ule, FT_MSG msg);
static void ule_init_empty_ule(ULE ule);
static void ule_do_implicit_promotions(ULE ule, XIDS xids);
static void ule_try_promote_provisional_outermost(ULE ule, TXNID oldest_possible_live_xid);
static void ule_promote_provisional_innermost_to_index(ULE ule, uint32_t index);
static void ule_promote_provisional_innermost_to_committed(ULE ule);
static void ule_apply_insert(ULE ule, XIDS xids, uint32_t vallen, void * valp);
static void ule_apply_delete(ULE ule, XIDS xids);
static void ule_prepare_for_new_uxr(ULE ule, XIDS xids);
static void ule_apply_abort(ULE ule, XIDS xids);
static void ule_apply_broadcast_commit_all (ULE ule);
static void ule_apply_commit(ULE ule, XIDS xids);
static void ule_push_insert_uxr(ULE ule, bool is_committed, TXNID xid, uint32_t vallen, void * valp);
static void ule_push_delete_uxr(ULE ule, bool is_committed, TXNID xid);
static void ule_push_placeholder_uxr(ULE ule, TXNID xid);
static UXR ule_get_innermost_uxr(ULE ule);
static UXR ule_get_first_empty_uxr(ULE ule);
static void ule_remove_innermost_uxr(ULE ule);
static TXNID ule_get_innermost_xid(ULE ule);
static TXNID ule_get_xid(ULE ule, uint32_t index);
static void ule_remove_innermost_placeholders(ULE ule);
static void ule_add_placeholders(ULE ule, XIDS xids);
static void ule_optimize(ULE ule, XIDS xids);
static inline bool uxr_type_is_insert(uint8_t type);
static inline bool uxr_type_is_delete(uint8_t type);
static inline bool uxr_type_is_placeholder(uint8_t type);
static inline size_t uxr_pack_txnid(UXR uxr, uint8_t *p);
static inline size_t uxr_pack_type_and_length(UXR uxr, uint8_t *p);
static inline size_t uxr_pack_length_and_bit(UXR uxr, uint8_t *p);
static inline size_t uxr_pack_data(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_txnid(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_type_and_length(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_length_and_bit(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_data(UXR uxr, uint8_t *p);

static void get_space_for_le(
    bn_data* data_buffer, 
    uint32_t idx,
    void* keyp,
    uint32_t keylen,
    uint32_t old_le_size,
    size_t size, 
    LEAFENTRY* new_le_space
    ) 
{
    if (data_buffer == NULL) {
        CAST_FROM_VOIDP(*new_le_space, toku_xmalloc(size));
    }
    else {
        // this means we are overwriting something
        if (old_le_size > 0) {
            data_buffer->get_space_for_overwrite(idx, keyp, keylen, old_le_size, size, new_le_space);
        }
        // this means we are inserting something new
        else {
            data_buffer->get_space_for_insert(idx, keyp, keylen, size, new_le_space);
        }
    }
}


/////////////////////////////////////////////////////////////////////
// Garbage collection related functions
//

static TXNID
get_next_older_txnid(TXNID xc, const xid_omt_t &omt) {
    int r;
    TXNID xid;
    r = omt.find<TXNID, toku_find_xid_by_xid>(xc, -1, &xid, nullptr);
    if (r==0) {
        invariant(xid < xc); //sanity check
    }
    else {
        invariant(r==DB_NOTFOUND);
        xid = TXNID_NONE;
    }
    return xid;
}

//
// This function returns true if live transaction TL1 is allowed to read a value committed by
// transaction xc, false otherwise.
//
static bool
xid_reads_committed_xid(TXNID tl1, TXNID xc, const xid_omt_t &snapshot_txnids, const rx_omt_t &referenced_xids) {
    bool rval;
    if (tl1 < xc) rval = false; //cannot read a newer txn
    else {
        TXNID x = toku_get_youngest_live_list_txnid_for(xc, snapshot_txnids, referenced_xids);
        if (x == TXNID_NONE) rval = true; //Not in ANY live list, tl1 can read it.
        else rval = tl1 > x;              //Newer than the 'newest one that has it in live list'
        // we know tl1 > xc
        // we know x > xc
        // if tl1 == x, then we do not read, because tl1 is in xc's live list
        // if x is older than tl1, that means that xc < x < tl1
        // and if xc is in x's live list, it CANNOT be in tl1's live list
    }
    return rval;
}

//
// This function does some simple garbage collection given a TXNID known
// to be the oldest referenced xid, that is, the oldest xid in any live list.
// We find the youngest entry in the stack with an xid less 
// than oldest_referenced_xid. All elements below this entry are garbage,
// so we get rid of them.
//
static void
ule_simple_garbage_collection(ULE ule, TXNID oldest_referenced_xid, GC_INFO gc_info) {
    uint32_t curr_index = 0;
    uint32_t num_entries;
    if (ule->num_cuxrs == 1) {
        goto done;
    }
    if (gc_info.mvcc_needed) {
        // starting at the top of the committed stack, find the first
        // uxr with a txnid that is less than oldest_referenced_xid
        for (uint32_t i = 0; i < ule->num_cuxrs; i++) {
            curr_index = ule->num_cuxrs - i - 1;
            if (ule->uxrs[curr_index].xid < oldest_referenced_xid) {
                break;
            }
        }
    }
    else {
        // if mvcc is not needed, we can need the top committed
        // value and nothing else
        curr_index = ule->num_cuxrs - 1;
    }
    // curr_index is now set to the youngest uxr older than oldest_referenced_xid
    if (curr_index == 0) {
        goto done;
    }

    // now get rid of the entries below curr_index
    num_entries = ule->num_cuxrs + ule->num_puxrs - curr_index;
    memmove(&ule->uxrs[0], &ule->uxrs[curr_index], num_entries * sizeof(ule->uxrs[0]));
    ule->uxrs[0].xid = TXNID_NONE; //New 'bottom of stack' loses its TXNID
    ule->num_cuxrs -= curr_index;
    
done:;
}

static void
ule_garbage_collect(ULE ule, const xid_omt_t &snapshot_xids, const rx_omt_t &referenced_xids, const xid_omt_t &live_root_txns) {
    if (ule->num_cuxrs == 1) goto done;
    // will fail if too many num_cuxrs
    bool necessary_static[MAX_TRANSACTION_RECORDS];
    bool *necessary;
    necessary = necessary_static;
    if (ule->num_cuxrs >= MAX_TRANSACTION_RECORDS) {
        XMALLOC_N(ule->num_cuxrs, necessary);
    }
    memset(necessary, 0, sizeof(necessary[0])*ule->num_cuxrs);

    uint32_t curr_committed_entry;
    curr_committed_entry = ule->num_cuxrs - 1;
    while (true) {
        // mark the curr_committed_entry as necessary
        necessary[curr_committed_entry] = true;
        if (curr_committed_entry == 0) break; //nothing left

        // find the youngest live transaction that reads something 
        // below curr_committed_entry, if it exists
        TXNID tl1;
        TXNID xc = ule->uxrs[curr_committed_entry].xid;

        //
        // If we find that the committed transaction is in the live list,
        // then xc is really in the process of being committed. It has not
        // been fully committed. As a result, our assumption that transactions
        // newer than what is currently in these OMTs will read the top of the stack
        // is not necessarily accurate. Transactions may read what is just below xc.
        // As a result, we must mark what is just below xc as necessary and move on.
        // This issue was found while testing flusher threads, and was fixed for #3979
        //
        bool is_xc_live = toku_is_txn_in_live_root_txn_list(live_root_txns, xc);
        if (is_xc_live) {
            curr_committed_entry--;
            continue;            
        }

        tl1 = toku_get_youngest_live_list_txnid_for(xc, snapshot_xids, referenced_xids);
        if (tl1 == xc) {
            // if tl1 == xc, that means xc should be live and show up in 
            // live_root_txns, which we check above. So, if we get
            // here, something is wrong.
            assert(false);
        }
        if (tl1 == TXNID_NONE) {
            // set tl1 to youngest live transaction older than ule->uxrs[curr_committed_entry]->xid
            tl1 = get_next_older_txnid(xc, snapshot_xids);
            if (tl1 == TXNID_NONE) {
                //Remainder is garbage, we're done
                break;
            }
        }
        if (garbage_collection_debug)
        {
            int r = snapshot_xids.find_zero<TXNID, toku_find_xid_by_xid>(tl1, nullptr, nullptr);
            invariant(r==0); //make sure that the txn you are claiming is live is actually live
        }
        //
        // tl1 should now be set
        //
        curr_committed_entry--;
        while (curr_committed_entry > 0) {
            xc = ule->uxrs[curr_committed_entry].xid;
            if (xid_reads_committed_xid(tl1, xc, snapshot_xids, referenced_xids)) {
                break;
            }
            curr_committed_entry--;
        }
    } 
    uint32_t first_free;
    first_free = 0;
    uint32_t i;
    for (i = 0; i < ule->num_cuxrs; i++) {
        //Shift values to 'delete' garbage values.
        if (necessary[i]) {
            ule->uxrs[first_free] = ule->uxrs[i];
            first_free++;
        }
    }
    uint32_t saved;
    saved = first_free;
    invariant(saved <= ule->num_cuxrs);
    invariant(saved >= 1);
    ule->uxrs[0].xid = TXNID_NONE; //New 'bottom of stack' loses its TXNID
    if (first_free != ule->num_cuxrs) {
        //Shift provisional values
        memmove(&ule->uxrs[first_free], &ule->uxrs[ule->num_cuxrs], ule->num_puxrs * sizeof(ule->uxrs[0]));
    }
    ule->num_cuxrs = saved;
    if (necessary != necessary_static) {
        toku_free(necessary);
    }
done:;
}

/////////////////////////////////////////////////////////////////////////////////
// This is the big enchilada.  (Bring Tums.)  Note that this level of abstraction 
// has no knowledge of the inner structure of either leafentry or msg.  It makes
// calls into the next lower layer (msg_xxx) which handles messages.
//
// NOTE: This is the only function (at least in this body of code) that modifies
//       a leafentry.
// NOTE: It is the responsibility of the caller to make sure that the key is set
//       in the FT_MSG, as it will be used to store the data in the data_buffer
//
// Return 0 on success.  
//   If the leafentry is destroyed it sets *new_leafentry_p to NULL.
//   Otehrwise the new_leafentry_p points at the new leaf entry.
// As of October 2011, this function always returns 0.
void
toku_le_apply_msg(FT_MSG   msg,
                  LEAFENTRY old_leafentry, // NULL if there was no stored data.
                  bn_data* data_buffer, // bn_data storing leafentry, if NULL, means there is no bn_data
                  uint32_t idx, // index in data_buffer where leafentry is stored (and should be replaced
                  TXNID oldest_referenced_xid,
                  GC_INFO gc_info,
                  LEAFENTRY *new_leafentry_p,
                  int64_t * numbytes_delta_p) {  // change in total size of key and val, not including any overhead
    ULE_S ule;
    int64_t oldnumbytes = 0;
    int64_t newnumbytes = 0;
    uint64_t oldmemsize = 0;
    uint32_t keylen = ft_msg_get_keylen(msg);
    LEAFENTRY copied_old_le = NULL;
    bool old_le_malloced = false;
    if (old_leafentry) {
        size_t old_le_size = leafentry_memsize(old_leafentry);
        if (old_le_size > 100*1024) { // completely arbitrary limit
            CAST_FROM_VOIDP(copied_old_le, toku_malloc(old_le_size));
            old_le_malloced = true;
        }
        else {
            CAST_FROM_VOIDP(copied_old_le, alloca(old_le_size));
        }
        memcpy(copied_old_le, old_leafentry, old_le_size);
    }

    if (old_leafentry == NULL) {
        msg_init_empty_ule(&ule);
    } else {
        oldmemsize = leafentry_memsize(old_leafentry);
        le_unpack(&ule, copied_old_le); // otherwise unpack leafentry
        oldnumbytes = ule_get_innermost_numbytes(&ule, keylen);
    }
    msg_modify_ule(&ule, msg);          // modify unpacked leafentry
    ule_simple_garbage_collection(&ule, oldest_referenced_xid, gc_info);
    int rval = le_pack(
        &ule, // create packed leafentry
        data_buffer,
        idx,
        ft_msg_get_key(msg), // contract of this function is caller has this set, always
        keylen, // contract of this function is caller has this set, always
        oldmemsize,
        new_leafentry_p
        );
    invariant_zero(rval);
    if (new_leafentry_p) {
        newnumbytes = ule_get_innermost_numbytes(&ule, keylen);
    }
    *numbytes_delta_p = newnumbytes - oldnumbytes;
    ule_cleanup(&ule);
    if (old_le_malloced) {
        toku_free(copied_old_le);
    }
}

bool toku_le_worth_running_garbage_collection(LEAFENTRY le, TXNID oldest_referenced_xid_known) {
// Effect: Quickly determines if it's worth trying to run garbage collection on a leafentry
// Return: True if it makes sense to try garbage collection, false otherwise.
// Rationale: Garbage collection is likely to clean up under two circumstances:
//            1.) There are multiple committed entries. Some may never be read by new txns.
//            2.) There is only one committed entry, but the outermost provisional entry
//            is older than the oldest known referenced xid, so it must have commited.
//            Therefor we can promote it to committed and get rid of the old commited entry.
    if (le->type != LE_MVCC) {
        return false;
    }
    if (le->u.mvcc.num_cxrs > 1) {
        return true;
    } else {
        paranoid_invariant(le->u.mvcc.num_cxrs == 1);
    }
    return le->u.mvcc.num_pxrs > 0 && le_outermost_uncommitted_xid(le) < oldest_referenced_xid_known;
}

// Garbage collect one leaf entry, using the given OMT's.
// Parameters:
// -- old_leaf_entry : the leaf we intend to clean up through garbage
// collecting.
// -- new_leaf_entry (OUTPUT) : a pointer to the leaf entry after
// garbage collection.
// -- new_leaf_entry_memory_size : after this call, our leaf entry
// should be empty or smaller.  This number represents that and is
// used in a previous call to truncate the existing size.
// -- omt : the memory where our leaf entry resides.
// -- mp : our memory pool.
// -- maybe_free (OUTPUT) : in a previous call, we may be able to free
// the memory completely, if we removed the leaf entry.
// -- snapshot_xids : we use these in memory transaction ids to
// determine what to garbage collect.
// -- referenced_xids : list of in memory active transactions.
// NOTE: it is not a good idea to garbage collect a leaf
// entry with only one committed value.
void
toku_le_garbage_collect(LEAFENTRY old_leaf_entry,
                        bn_data* data_buffer,
                        uint32_t idx,
                        void* keyp,
                        uint32_t keylen,
                        LEAFENTRY *new_leaf_entry,
                        const xid_omt_t &snapshot_xids,
                        const rx_omt_t &referenced_xids,
                        const xid_omt_t &live_root_txns,
                        TXNID oldest_referenced_xid_known,
                        int64_t * numbytes_delta_p) {
    ULE_S ule;
    int64_t oldnumbytes = 0;
    int64_t newnumbytes = 0;
    LEAFENTRY copied_old_le = NULL;
    bool old_le_malloced = false;
    if (old_leaf_entry) {
        size_t old_le_size = leafentry_memsize(old_leaf_entry);
        if (old_le_size > 100*1024) { // completely arbitrary limit
            CAST_FROM_VOIDP(copied_old_le, toku_malloc(old_le_size));
            old_le_malloced = true;
        }
        else {
            CAST_FROM_VOIDP(copied_old_le, alloca(old_le_size));
        }
        memcpy(copied_old_le, old_leaf_entry, old_le_size);
    }

    le_unpack(&ule, copied_old_le);

    oldnumbytes = ule_get_innermost_numbytes(&ule, keylen);
    uint32_t old_mem_size = leafentry_memsize(old_leaf_entry);

    // Before running garbage collection, try to promote the outermost provisional
    // entries to committed if its xid is older than the oldest possible live xid.
    // 
    // The oldest known refeferenced xid is a lower bound on the oldest possible
    // live xid, so we use that. It's usually close enough to get rid of most
    // garbage in leafentries.
    TXNID oldest_possible_live_xid = oldest_referenced_xid_known;
    ule_try_promote_provisional_outermost(&ule, oldest_possible_live_xid);
    ule_garbage_collect(&ule, snapshot_xids, referenced_xids, live_root_txns);
    
    int r = le_pack(
        &ule,
        data_buffer,
        idx,
        keyp,
        keylen,
        old_mem_size,
        new_leaf_entry
        );
    assert(r == 0);
    if (new_leaf_entry) {
        newnumbytes = ule_get_innermost_numbytes(&ule, keylen);
    }
    *numbytes_delta_p = newnumbytes - oldnumbytes;
    ule_cleanup(&ule);
    if (old_le_malloced) {
        toku_free(copied_old_le);
    }
}

/////////////////////////////////////////////////////////////////////////////////
// This layer of abstraction (msg_xxx)
// knows the accessors of msg, but not of leafentry or unpacked leaf entry.
// It makes calls into the lower layer (le_xxx) which handles leafentries.

// Purpose is to init the ule with given key and no transaction records
// 
static void 
msg_init_empty_ule(ULE ule) {
    ule_init_empty_ule(ule);
}

// Purpose is to modify the unpacked leafentry in our private workspace.
//
static void 
msg_modify_ule(ULE ule, FT_MSG msg) {
    XIDS xids = ft_msg_get_xids(msg);
    invariant(xids_get_num_xids(xids) < MAX_TRANSACTION_RECORDS);
    enum ft_msg_type type = ft_msg_get_type(msg);
    if (type != FT_OPTIMIZE && type != FT_OPTIMIZE_FOR_UPGRADE) {
        ule_do_implicit_promotions(ule, xids);
    }
    switch (type) {
    case FT_INSERT_NO_OVERWRITE: {
        UXR old_innermost_uxr = ule_get_innermost_uxr(ule);
        //If something exists, quit (no overwrite).
        if (uxr_is_insert(old_innermost_uxr)) break;
        //else it is just an insert, so
        //fall through to FT_INSERT on purpose.
    }
    case FT_INSERT: {
        uint32_t vallen = ft_msg_get_vallen(msg);
        invariant(IS_VALID_LEN(vallen));
        void * valp      = ft_msg_get_val(msg);
        ule_apply_insert(ule, xids, vallen, valp);
        break;
    }
    case FT_DELETE_ANY:
        ule_apply_delete(ule, xids);
        break;
    case FT_ABORT_ANY:
    case FT_ABORT_BROADCAST_TXN:
        ule_apply_abort(ule, xids);
        break;
    case FT_COMMIT_BROADCAST_ALL:
        ule_apply_broadcast_commit_all(ule);
        break;
    case FT_COMMIT_ANY:
    case FT_COMMIT_BROADCAST_TXN:
        ule_apply_commit(ule, xids);
        break;
    case FT_OPTIMIZE:
    case FT_OPTIMIZE_FOR_UPGRADE:
        ule_optimize(ule, xids);
        break;
    case FT_UPDATE:
    case FT_UPDATE_BROADCAST_ALL:
        assert(false); // These messages don't get this far.  Instead they get translated (in setval_fun in do_update) into FT_INSERT messages.
        break;
    default:
        assert(false /* illegal FT_MSG.type */);
        break;
    }
}

void 
test_msg_modify_ule(ULE ule, FT_MSG msg){
    msg_modify_ule(ule,msg);
}


static void ule_optimize(ULE ule, XIDS xids) {
    if (ule->num_puxrs) {
        TXNID uncommitted = ule->uxrs[ule->num_cuxrs].xid;      // outermost uncommitted
        TXNID oldest_living_xid = TXNID_NONE;
        uint32_t num_xids = xids_get_num_xids(xids);
        if (num_xids > 0) {
            invariant(num_xids==1);
            oldest_living_xid = xids_get_xid(xids, 0);
        }
        if (oldest_living_xid == TXNID_NONE || uncommitted < oldest_living_xid) {
            ule_promote_provisional_innermost_to_committed(ule);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////
// This layer of abstraction (le_xxx) understands the structure of the leafentry
// and of the unpacked leafentry.  It is the only layer that understands the
// structure of leafentry.  It has no knowledge of any other data structures.
//

//
// required for every le_unpack that is done
//
void
ule_cleanup(ULE ule) {
    invariant(ule->uxrs);
    if (ule->uxrs != ule->uxrs_static) {
        toku_free(ule->uxrs);
        ule->uxrs = NULL;
    }
}

// populate an unpacked leafentry using pointers into the given leafentry.
// thus, the memory referenced by 'le' must live as long as the ULE.
void
le_unpack(ULE ule, LEAFENTRY le) {
    uint8_t  type = le->type;
    uint8_t *p;
    uint32_t i;
    switch (type) {
        case LE_CLEAN: {
            ule->uxrs = ule->uxrs_static; //Static version is always enough.
            ule->num_cuxrs = 1;
            ule->num_puxrs = 0;
            UXR uxr     = ule->uxrs;
            uxr->type   = XR_INSERT;
            uxr->vallen = toku_dtoh32(le->u.clean.vallen);
            uxr->valp   = le->u.clean.val;
            uxr->xid    = TXNID_NONE;
            //Set p to immediately after leafentry
            p = le->u.clean.val + uxr->vallen;
            break;
        }
        case LE_MVCC:
            ule->num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(ule->num_cuxrs);
            ule->num_puxrs = le->u.mvcc.num_pxrs;
            //Dynamic memory
            if (ule->num_cuxrs < MAX_TRANSACTION_RECORDS) {
                ule->uxrs = ule->uxrs_static;
            }
            else {
                XMALLOC_N(ule->num_cuxrs + 1 + MAX_TRANSACTION_RECORDS, ule->uxrs);
            }
            p = le->u.mvcc.xrs;

            //unpack interesting TXNIDs inner to outer.
            if (ule->num_puxrs!=0) {
                UXR outermost = ule->uxrs + ule->num_cuxrs;
                p += uxr_unpack_txnid(outermost, p);
            }
            //unpack other TXNIDS (not for ule->uxrs[0])
            ule->uxrs[0].xid = TXNID_NONE; //0 for super-root is implicit
            for (i = 0; i < ule->num_cuxrs - 1; i++) {
                p += uxr_unpack_txnid(ule->uxrs + ule->num_cuxrs - 1 - i, p);
            }

            //unpack interesting lengths inner to outer.
            if (ule->num_puxrs!=0) {
                UXR innermost = ule->uxrs + ule->num_cuxrs + ule->num_puxrs - 1;
                p += uxr_unpack_length_and_bit(innermost, p);
            }
            for (i = 0; i < ule->num_cuxrs; i++) {
                p += uxr_unpack_length_and_bit(ule->uxrs + ule->num_cuxrs - 1 - i, p);
            }
 
            //unpack interesting values inner to outer
            if (ule->num_puxrs!=0) {
                UXR innermost = ule->uxrs + ule->num_cuxrs + ule->num_puxrs - 1;
                p += uxr_unpack_data(innermost, p);
            }
            for (i = 0; i < ule->num_cuxrs; i++) {
                p += uxr_unpack_data(ule->uxrs + ule->num_cuxrs - 1 - i, p);
            }

            //unpack provisional xrs outer to inner
            if (ule->num_puxrs > 1) {
                {
                    //unpack length, bit, data for outermost uncommitted
                    UXR outermost = ule->uxrs + ule->num_cuxrs;
                    p += uxr_unpack_type_and_length(outermost, p);
                    p += uxr_unpack_data(outermost, p);
                }
                //unpack txnid, length, bit, data for non-outermost, non-innermost
                for (i = ule->num_cuxrs + 1; i < ule->num_cuxrs + ule->num_puxrs - 1; i++) {
                    UXR uxr = ule->uxrs + i;
                    p += uxr_unpack_txnid(uxr, p);
                    p += uxr_unpack_type_and_length(uxr, p);
                    p += uxr_unpack_data(uxr, p);
                }
                {
                    //Just unpack txnid for innermost
                    UXR innermost = ule->uxrs + ule->num_cuxrs + ule->num_puxrs - 1;
                    p += uxr_unpack_txnid(innermost, p);
                }
            }
            break;
        default:
            invariant(false);
    }
    
#if ULE_DEBUG
    size_t memsize = le_memsize_from_ule(ule);
    assert(p == ((uint8_t*)le) + memsize);
#endif
}

static inline size_t
uxr_pack_txnid(UXR uxr, uint8_t *p) {
    *(TXNID*)p = toku_htod64(uxr->xid);
    return sizeof(TXNID);
}

static inline size_t
uxr_pack_type_and_length(UXR uxr, uint8_t *p) {
    size_t rval = 1;
    *p = uxr->type;
    if (uxr_is_insert(uxr)) {
        *(uint32_t*)(p+1) = toku_htod32(uxr->vallen);
        rval += sizeof(uint32_t);
    }
    return rval;
}

static inline size_t
uxr_pack_length_and_bit(UXR uxr, uint8_t *p) {
    uint32_t length_and_bit;
    if (uxr_is_insert(uxr)) {
        length_and_bit = INSERT_LENGTH(uxr->vallen);
    }
    else {
        length_and_bit = DELETE_LENGTH(uxr->vallen);
    }
    *(uint32_t*)p = toku_htod32(length_and_bit);
    return sizeof(uint32_t);
}

static inline size_t
uxr_pack_data(UXR uxr, uint8_t *p) {
    if (uxr_is_insert(uxr)) {
        memcpy(p, uxr->valp, uxr->vallen);
        return uxr->vallen;
    }
    return 0;
}

static inline size_t
uxr_unpack_txnid(UXR uxr, uint8_t *p) {
    uxr->xid = toku_dtoh64(*(TXNID*)p);
    return sizeof(TXNID);
}

static inline size_t
uxr_unpack_type_and_length(UXR uxr, uint8_t *p) {
    size_t rval = 1;
    uxr->type = *p;
    if (uxr_is_insert(uxr)) {
        uxr->vallen = toku_dtoh32(*(uint32_t*)(p+1));
        rval += sizeof(uint32_t);
    }
    return rval;
}

static inline size_t
uxr_unpack_length_and_bit(UXR uxr, uint8_t *p) {
    uint32_t length_and_bit = toku_dtoh32(*(uint32_t*)p);
    if (IS_INSERT(length_and_bit)) {
        uxr->type = XR_INSERT;
        uxr->vallen = GET_LENGTH(length_and_bit);
    }
    else {
        uxr->type   = XR_DELETE;
        uxr->vallen = 0;
    }
    return sizeof(uint32_t);
}

static inline size_t
uxr_unpack_data(UXR uxr, uint8_t *p) {
    if (uxr_is_insert(uxr)) {
        uxr->valp = p;
        return uxr->vallen;
    }
    return 0;
}

// executed too often to be worth making threadsafe
static inline void
update_le_status(ULE ule, size_t memsize) {
    if (ule->num_cuxrs > STATUS_VALUE(LE_MAX_COMMITTED_XR))
        STATUS_VALUE(LE_MAX_COMMITTED_XR) = ule->num_cuxrs;
    if (ule->num_puxrs > STATUS_VALUE(LE_MAX_PROVISIONAL_XR))
        STATUS_VALUE(LE_MAX_PROVISIONAL_XR) = ule->num_puxrs;
    if (ule->num_cuxrs > MAX_TRANSACTION_RECORDS)
        STATUS_VALUE(LE_EXPANDED)++;
    if (memsize > STATUS_VALUE(LE_MAX_MEMSIZE))
        STATUS_VALUE(LE_MAX_MEMSIZE) = memsize;
}

// Purpose is to return a newly allocated leaf entry in packed format, or
// return null if leaf entry should be destroyed (if no transaction records 
// are for inserts).
// Transaction records in packed le are stored inner to outer (first xr is innermost),
// with some information extracted out of the transaction records into the header.
// Transaction records in ule are stored outer to inner (uxr[0] is outermost).
int
le_pack(ULE ule, // data to be packed into new leafentry
        bn_data* data_buffer,
        uint32_t idx,
        void* keyp,
        uint32_t keylen,
        uint32_t old_le_size,
        LEAFENTRY * const new_leafentry_p // this is what this function creates
        )
{
    invariant(ule->num_cuxrs > 0);
    invariant(ule->uxrs[0].xid == TXNID_NONE);
    int rval;
    size_t memsize = 0;
    {
        // The unpacked leafentry may contain no inserts anywhere on its stack.
        // If so, then there IS no leafentry to pack, we should return NULL
        // So, first we check the stack to see if there is any insert. If not,
        // Then we can return NULL and exit the function, otherwise, we goto
        // found_insert, and proceed with packing the leafentry
        uint32_t i;
        for (i = 0; i < ule->num_cuxrs + ule->num_puxrs; i++) {
            if (uxr_is_insert(&ule->uxrs[i])) {
                goto found_insert;
            }
        }
        if (data_buffer && old_le_size > 0) {
            data_buffer->delete_leafentry(idx, keylen, old_le_size);
        }
        *new_leafentry_p = NULL;
        rval = 0;
        goto cleanup;
    }
found_insert:;
    memsize = le_memsize_from_ule(ule);
    LEAFENTRY new_leafentry;
    get_space_for_le(data_buffer, idx, keyp, keylen, old_le_size, memsize, &new_leafentry);

    //p always points to first unused byte after leafentry we are packing
    uint8_t *p;
    invariant(ule->num_cuxrs>0);
    //Type specific data
    if (ule->num_cuxrs == 1 && ule->num_puxrs == 0) {
        //Pack a 'clean leafentry' (no uncommitted transactions, only one committed value)
        new_leafentry->type = LE_CLEAN;

        uint32_t vallen = ule->uxrs[0].vallen;
        //Store vallen
        new_leafentry->u.clean.vallen  = toku_htod32(vallen);

        //Store actual val
        memcpy(new_leafentry->u.clean.val, ule->uxrs[0].valp, vallen);

        //Set p to after leafentry
        p = new_leafentry->u.clean.val + vallen;
    }
    else {
        uint32_t i;
        //Pack an 'mvcc leafentry'
        new_leafentry->type = LE_MVCC;

        new_leafentry->u.mvcc.num_cxrs = toku_htod32(ule->num_cuxrs);
        // invariant makes cast that follows ok, although not sure if 
        // check should be "< MAX_TRANSACTION_RECORDS" or
        // "< MAX_TRANSACTION_RECORDS - 1"
        invariant(ule->num_puxrs < MAX_TRANSACTION_RECORDS);
        new_leafentry->u.mvcc.num_pxrs = (uint8_t)ule->num_puxrs;

        p = new_leafentry->u.mvcc.xrs;

        //pack interesting TXNIDs inner to outer.
        if (ule->num_puxrs!=0) {
            UXR outermost = ule->uxrs + ule->num_cuxrs;
            p += uxr_pack_txnid(outermost, p);
        }
        //pack other TXNIDS (not for ule->uxrs[0])
        for (i = 0; i < ule->num_cuxrs - 1; i++) {
            p += uxr_pack_txnid(ule->uxrs + ule->num_cuxrs - 1 - i, p);
        }

        //pack interesting lengths inner to outer.
        if (ule->num_puxrs!=0) {
            UXR innermost = ule->uxrs + ule->num_cuxrs + ule->num_puxrs - 1;
            p += uxr_pack_length_and_bit(innermost, p);
        }
        for (i = 0; i < ule->num_cuxrs; i++) {
            p += uxr_pack_length_and_bit(ule->uxrs + ule->num_cuxrs - 1 - i, p);
        }
        
        //pack interesting values inner to outer
        if (ule->num_puxrs!=0) {
            UXR innermost = ule->uxrs + ule->num_cuxrs + ule->num_puxrs - 1;
            p += uxr_pack_data(innermost, p);
        }
        for (i = 0; i < ule->num_cuxrs; i++) {
            p += uxr_pack_data(ule->uxrs + ule->num_cuxrs - 1 - i, p);
        }

        //pack provisional xrs outer to inner
        if (ule->num_puxrs > 1) {
            {
                //pack length, bit, data for outermost uncommitted
                UXR outermost = ule->uxrs + ule->num_cuxrs;
                p += uxr_pack_type_and_length(outermost, p);
                p += uxr_pack_data(outermost, p);
            }
            //pack txnid, length, bit, data for non-outermost, non-innermost
            for (i = ule->num_cuxrs + 1; i < ule->num_cuxrs + ule->num_puxrs - 1; i++) {
                UXR uxr = ule->uxrs + i;
                p += uxr_pack_txnid(uxr, p);
                p += uxr_pack_type_and_length(uxr, p);
                p += uxr_pack_data(uxr, p);
            }
            {
                //Just pack txnid for innermost
                UXR innermost = ule->uxrs + ule->num_cuxrs + ule->num_puxrs - 1;
                p += uxr_pack_txnid(innermost, p);
            }
        }
    }

    //p points to first unused byte after packed leafentry

    size_t bytes_written;
    bytes_written = (size_t)p - (size_t)new_leafentry;
    invariant(bytes_written == memsize);
         
#if ULE_DEBUG
    if (omt) { //Disable recursive debugging.
        size_t memsize_verify = leafentry_memsize(new_leafentry);
        invariant(memsize_verify == memsize);

        ULE_S ule_tmp;
        le_unpack(&ule_tmp, new_leafentry);

        memsize_verify = le_memsize_from_ule(&ule_tmp);
        invariant(memsize_verify == memsize);
        //Debugging code inside le_unpack will repack and verify it is the same.

        LEAFENTRY le_copy;

        int r_tmp = le_pack(&ule_tmp, &memsize_verify, &memsize_verify,
                            &le_copy);
        invariant(r_tmp==0);
        invariant(memsize_verify == memsize);

        invariant(memcmp(new_leafentry, le_copy, memsize)==0);
        toku_free(le_copy);

        ule_cleanup(&ule_tmp);
    }
#endif

    *new_leafentry_p = (LEAFENTRY)new_leafentry;
    rval = 0;
cleanup:
    update_le_status(ule, memsize);
    return rval;
}

//////////////////////////////////////////////////////////////////////////////////
// Following functions provide convenient access to a packed leafentry.

//Requires:
//  Leafentry that ule represents should not be destroyed (is not just all deletes)
size_t
le_memsize_from_ule (ULE ule) {
    invariant(ule->num_cuxrs);
    size_t rval;
    if (ule->num_cuxrs == 1 && ule->num_puxrs == 0) {
        UXR committed = ule->uxrs;
        invariant(uxr_is_insert(committed));
        rval = 1                    //type
              +4                    //vallen
              +committed->vallen;   //actual val
    }
    else {
        rval = 1                    //type
              +4                    //num_cuxrs
              +1                    //num_puxrs
              +4*(ule->num_cuxrs)   //types+lengths for committed
              +8*(ule->num_cuxrs + ule->num_puxrs - 1);  //txnids (excluding superroot)
        uint32_t i;
        //Count data from committed uxrs and innermost puxr
        for (i = 0; i < ule->num_cuxrs; i++) {
            UXR uxr = &ule->uxrs[i];
            if (uxr_is_insert(uxr)) {
                rval += uxr->vallen; //actual val
            }
        }
        if (ule->num_puxrs) {
            UXR uxr = ule_get_innermost_uxr(ule);
            if (uxr_is_insert(uxr)) {
                rval += uxr->vallen; //actual val
            }
            rval += 4; //type+length for innermost puxr
            rval += 1*(ule->num_puxrs - 1); //type for remaining puxrs.
            //Count data and lengths from other puxrs
            for (i = 0; i < ule->num_puxrs-1; i++) {
                uxr = &ule->uxrs[i+ule->num_cuxrs];
                if (uxr_is_insert(uxr)) {
                    rval += 4 + uxr->vallen; //length plus actual val
                }
            }
        }
    }
    return rval;
}

// TODO: rename
size_t
leafentry_rest_memsize(uint32_t num_puxrs, uint32_t num_cuxrs, uint8_t* start) {
    UXR_S uxr;
    size_t   lengths = 0;
    uint8_t* p = start;

    //Skip TXNIDs
    if (num_puxrs!=0) {
        p += sizeof(TXNID);
    }
    p += (num_cuxrs-1)*sizeof(TXNID);

    //Retrieve interesting lengths inner to outer.
    if (num_puxrs!=0) {
        p += uxr_unpack_length_and_bit(&uxr, p);
        if (uxr_is_insert(&uxr)) {
            lengths += uxr.vallen;
        }
    }
    uint32_t i;
    for (i = 0; i < num_cuxrs; i++) {
        p += uxr_unpack_length_and_bit(&uxr, p);
        if (uxr_is_insert(&uxr)) {
            lengths += uxr.vallen;
        }
    }
    //Skip all interesting 'data'
    p += lengths;

    //unpack provisional xrs outer to inner
    if (num_puxrs > 1) {
        {
            p += uxr_unpack_type_and_length(&uxr, p);
            p += uxr_unpack_data(&uxr, p);
        }
        //unpack txnid, length, bit, data for non-outermost, non-innermost
        for (i = 0; i < num_puxrs - 2; i++) {
            p += uxr_unpack_txnid(&uxr, p);
            p += uxr_unpack_type_and_length(&uxr, p);
            p += uxr_unpack_data(&uxr, p);
        }
        {
            //Just unpack txnid for innermost
            p += uxr_unpack_txnid(&uxr, p);
        }
    }
    size_t rval = (size_t)p - (size_t)start;
    return rval;
}

size_t
leafentry_memsize (LEAFENTRY le) {
    size_t rval = 0;

    uint8_t  type = le->type;

    uint8_t *p = NULL;
    switch (type) {
        case LE_CLEAN: {
            uint32_t vallen = toku_dtoh32(le->u.clean.vallen);
            rval = LE_CLEAN_MEMSIZE(vallen);
            break;
        }
        case LE_MVCC: {
            p = le->u.mvcc.xrs;
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;
            p += leafentry_rest_memsize(num_puxrs, num_cuxrs, p);
            rval = (size_t)p - (size_t)le;
            break;
        }
        default:
            invariant(false);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    size_t slow_rval = le_memsize_from_ule(&ule);
    if (slow_rval!=rval) {
        int r = print_klpair(stderr, le, NULL, 0);
        fprintf(stderr, "\nSlow: [%" PRIu64 "] Fast: [%" PRIu64 "]\n", slow_rval, rval);
        invariant(r==0);
    }
    assert(slow_rval == rval);
    ule_cleanup(&ule);
#endif
    return rval;
}

size_t
leafentry_disksize (LEAFENTRY le) {
    return leafentry_memsize(le);
}

bool
le_is_clean(LEAFENTRY le) {
    uint8_t  type = le->type;
    uint32_t rval;
    switch (type) {
        case LE_CLEAN:
            rval = true;
            break;
        case LE_MVCC:;
            rval = false;
            break;
        default:
            invariant(false);
    }
    return rval;
}

int le_latest_is_del(LEAFENTRY le) {
    int rval;
    uint8_t  type = le->type;
    uint8_t *p;
    switch (type) {
        case LE_CLEAN: {
            rval = 0;
            break;
        }
        case LE_MVCC: {
            UXR_S uxr;
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;

            //Position p.
            p = le->u.mvcc.xrs;

            //Skip TXNIDs
            if (num_puxrs!=0) {
                p += sizeof(TXNID);
            }
            p += (num_cuxrs-1)*sizeof(TXNID);

            p += uxr_unpack_length_and_bit(&uxr, p);
            rval = uxr_is_delete(&uxr);
            break;
        }
        default:
            invariant(false);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    int slow_rval = uxr_is_delete(uxr);
    assert((rval==0) == (slow_rval==0));
    ule_cleanup(&ule);
#endif
    return rval;
}


//
// returns true if the outermost provisional transaction id on the leafentry's stack matches
// the outermost transaction id in xids
// It is used to determine if a broadcast commit/abort message (look in ft-ops.c)  should be applied to this leafentry
// If the outermost transactions match, then the broadcast commit/abort should be applied
//
bool
le_has_xids(LEAFENTRY le, XIDS xids) {
    //Read num_uxrs
    uint32_t num_xids = xids_get_num_xids(xids);
    invariant(num_xids > 0); //Disallow checking for having TXNID_NONE
    TXNID xid = xids_get_xid(xids, 0);
    invariant(xid!=TXNID_NONE);

    bool rval = (le_outermost_uncommitted_xid(le) == xid);
    return rval;
}

void*
le_latest_val_and_len (LEAFENTRY le, uint32_t *len) {
    uint8_t  type = le->type;
    void *valp;

    uint8_t *p;
    switch (type) {
        case LE_CLEAN:
            *len = toku_dtoh32(le->u.clean.vallen);
            valp = le->u.clean.val;
            break;
        case LE_MVCC:;
            UXR_S uxr;
            uint32_t num_cuxrs;
            num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs;
            num_puxrs = le->u.mvcc.num_pxrs;

            //Position p.
            p = le->u.mvcc.xrs;

            //Skip TXNIDs
            if (num_puxrs!=0) {
                p += sizeof(TXNID);
            }
            p += (num_cuxrs-1)*sizeof(TXNID);

            p += uxr_unpack_length_and_bit(&uxr, p);
            if (uxr_is_insert(&uxr)) {
                *len = uxr.vallen;
                valp = p + (num_cuxrs - 1 + (num_puxrs!=0))*sizeof(uint32_t);
            }
            else {
                *len = 0;
                valp = NULL;
            }
            break;
        default:
            invariant(false);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    void     *slow_valp;
    uint32_t slow_len;
    if (uxr_is_insert(uxr)) {
        slow_valp = uxr->valp;
        slow_len  = uxr->vallen; 
    }
    else {
        slow_valp = NULL;
        slow_len  = 0;
    }
    assert(slow_valp == le_latest_val(le));
    assert(slow_len == le_latest_vallen(le));
    assert(valp==slow_valp);
    assert(*len==slow_len);
    ule_cleanup(&ule);
#endif
    return valp;
}

//DEBUG ONLY can be slow
void*
le_latest_val (LEAFENTRY le) {
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    void *slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = uxr->valp;
    else
        slow_rval = NULL;
    ule_cleanup(&ule);
    return slow_rval;
}

//needed to be fast for statistics.
uint32_t
le_latest_vallen (LEAFENTRY le) {
    uint32_t rval;
    uint8_t  type = le->type;
    uint8_t *p;
    switch (type) {
        case LE_CLEAN:
            rval = toku_dtoh32(le->u.clean.vallen);
            break;
        case LE_MVCC:;
            UXR_S uxr;
            uint32_t num_cuxrs;
            num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs;
            num_puxrs = le->u.mvcc.num_pxrs;

            //Position p.
            p = le->u.mvcc.xrs;

            //Skip TXNIDs
            if (num_puxrs!=0) {
                p += sizeof(TXNID);
            }
            p += (num_cuxrs-1)*sizeof(TXNID);

            uxr_unpack_length_and_bit(&uxr, p);
            if (uxr_is_insert(&uxr)) {
                rval = uxr.vallen;
            }
            else {
                rval = 0;
            }
            break;
        default:
            invariant(false);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    uint32_t slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = uxr->vallen;
    else
        slow_rval = 0;
    ule_cleanup(&ule);
    invariant(slow_rval == rval);
#endif
    return rval;
}

uint64_t 
le_outermost_uncommitted_xid (LEAFENTRY le) {
    uint64_t rval = TXNID_NONE;
    uint8_t  type = le->type;

    uint8_t *p;
    switch (type) {
        case LE_CLEAN:
            break;
        case LE_MVCC:;
            UXR_S uxr;
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;

            if (num_puxrs) {
                p = le->u.mvcc.xrs;
                uxr_unpack_txnid(&uxr, p);
                rval = uxr.xid;
            }
            break;
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    TXNID slow_rval = 0;
    if (ule.num_puxrs > 0)
        slow_rval = ule.uxrs[ule.num_cuxrs].xid;
    assert(rval==slow_rval);
    ule_cleanup(&ule);
#endif
    return rval;
}


//Optimization not required.  This is a debug only function.
//Print a leafentry out in human-readable format
int
print_klpair (FILE *outf, const void* keyp, uint32_t keylen, LEAFENTRY le) {
    ULE_S ule;
    le_unpack(&ule, le);
    uint32_t i;
    invariant(ule.num_cuxrs > 0);
    UXR uxr;
    if (!le) { printf("NULL"); return 0; }
    if (keyp) {
        fprintf(outf, "{key=");
        toku_print_BYTESTRING(outf, keylen, (char *) keyp);
    }
    for (i = 0; i < ule.num_cuxrs+ule.num_puxrs; i++) {
        // fprintf(outf, "\n%*s", i+1, " "); //Nested indenting
        uxr = &ule.uxrs[i];
        char prov = i < ule.num_cuxrs ? 'c' : 'p';
        fprintf(outf, " ");
        if (uxr_is_placeholder(uxr))
            fprintf(outf, "P: xid=%016" PRIx64, uxr->xid);
        else if (uxr_is_delete(uxr))
            fprintf(outf, "%cD: xid=%016" PRIx64, prov, uxr->xid);
        else {
            assert(uxr_is_insert(uxr));
            fprintf(outf, "%cI: xid=%016" PRIx64 " val=", prov, uxr->xid);
            toku_print_BYTESTRING(outf, uxr->vallen, (char *) uxr->valp);
        }
    }
    fprintf(outf, "}");
    ule_cleanup(&ule);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// This layer of abstraction (ule_xxx) knows the structure of the unpacked
// leafentry and no other structure.
//

// ule constructor
// Note that transaction 0 is explicit in the ule
static void 
ule_init_empty_ule(ULE ule) {
    ule->num_cuxrs = 1;
    ule->num_puxrs = 0;
    ule->uxrs      = ule->uxrs_static;
    ule->uxrs[0]   = committed_delete;
}

static inline int32_t 
min_i32(int32_t a, int32_t b) {
    int32_t rval = a < b ? a : b;
    return rval;
}

///////////////////
// Implicit promotion logic:
//
// If the leafentry has already been promoted, there is nothing to do.
// We have two transaction stacks (one from message, one from leaf entry).
// We want to implicitly promote transactions newer than (but not including) 
// the innermost common ancestor (ICA) of the two stacks of transaction ids.  We 
// know that this is the right thing to do because each transaction with an id
// greater (later) than the ICA must have been either committed or aborted.
// If it was aborted then we would have seen an abort message and removed the
// xid from the stack of transaction records.  So any transaction still on the 
// leaf entry stack must have been successfully promoted.
// 
// After finding the ICA, promote transaction later than the ICA by copying
// value and type from innermost transaction record of leafentry to transaction
// record of ICA, keeping the transaction id of the ICA.
// Outermost xid is zero for both ule and xids<>
//
static void 
ule_do_implicit_promotions(ULE ule, XIDS xids) {
    //Optimization for (most) common case.
    //No commits necessary if everything is already committed.
    if (ule->num_puxrs > 0) {
        int num_xids = xids_get_num_xids(xids);
        invariant(num_xids>0);
        uint32_t max_index = ule->num_cuxrs + min_i32(ule->num_puxrs, num_xids) - 1;
        uint32_t ica_index = max_index;
        uint32_t index;
        for (index = ule->num_cuxrs; index <= max_index; index++) {
            TXNID current_msg_xid = xids_get_xid(xids, index - ule->num_cuxrs);
            TXNID current_ule_xid = ule_get_xid(ule, index);
            if (current_msg_xid != current_ule_xid) {
                //ica is innermost transaction with matching xids.
                ica_index = index - 1;
                break;
            }
        }

        if (ica_index < ule->num_cuxrs) {
            invariant(ica_index == ule->num_cuxrs - 1);
            ule_promote_provisional_innermost_to_committed(ule);
        }
        else if (ica_index < ule->num_cuxrs + ule->num_puxrs - 1) {
            //If ica is the innermost uxr in the leafentry, no commits are necessary.
            ule_promote_provisional_innermost_to_index(ule, ica_index);
        }

    }
}

static void
ule_promote_provisional_innermost_to_committed(ULE ule) {
    //Must be something to promote.
    invariant(ule->num_puxrs);
    //Take value (or delete flag) from innermost.
    //Take TXNID from outermost uncommitted txn
    //"Delete" provisional stack
    //add one UXR that is committed using saved TXNID,val/delete flag

    UXR old_innermost_uxr = ule_get_innermost_uxr(ule);
    assert(!uxr_is_placeholder(old_innermost_uxr));

    UXR old_outermost_uncommitted_uxr = &ule->uxrs[ule->num_cuxrs];

    ule->num_puxrs = 0; //Discard all provisional uxrs.
    if (uxr_is_delete(old_innermost_uxr)) {
        ule_push_delete_uxr(ule, true, old_outermost_uncommitted_uxr->xid);
    }
    else {
        ule_push_insert_uxr(ule, true,
                            old_outermost_uncommitted_uxr->xid,
                            old_innermost_uxr->vallen,
                            old_innermost_uxr->valp);
    }
}

static void
ule_try_promote_provisional_outermost(ULE ule, TXNID oldest_possible_live_xid) {
// Effect: If there is a provisional record whose outermost xid is older than
//         the oldest known referenced_xid, promote it to committed.
    if (ule->num_puxrs > 0 && ule_get_xid(ule, ule->num_cuxrs) < oldest_possible_live_xid) {
        ule_promote_provisional_innermost_to_committed(ule);
    }
}

// Purpose is to promote the value (and type) of the innermost transaction
// record to the uxr at the specified index (keeping the txnid of the uxr at
// specified index.)
static void 
ule_promote_provisional_innermost_to_index(ULE ule, uint32_t index) {
    //Must not promote to committed portion of stack.
    invariant(index >= ule->num_cuxrs);
    //Must actually be promoting.
    invariant(index < ule->num_cuxrs + ule->num_puxrs - 1);
    UXR old_innermost_uxr = ule_get_innermost_uxr(ule);
    assert(!uxr_is_placeholder(old_innermost_uxr));
    TXNID new_innermost_xid = ule->uxrs[index].xid;
    ule->num_puxrs = index - ule->num_cuxrs; //Discard old uxr at index (and everything inner)
    if (uxr_is_delete(old_innermost_uxr)) {
        ule_push_delete_uxr(ule, false, new_innermost_xid);
    }
    else {
        ule_push_insert_uxr(ule, false,
                            new_innermost_xid,
                            old_innermost_uxr->vallen,
                            old_innermost_uxr->valp);
    }
}

///////////////////
//  All ule_apply_xxx operations are done after implicit promotions,
//  so the innermost transaction record in the leafentry is the ICA.
//


// Purpose is to apply an insert message to this leafentry:
static void 
ule_apply_insert(ULE ule, XIDS xids, uint32_t vallen, void * valp) {
    ule_prepare_for_new_uxr(ule, xids);
    TXNID this_xid = xids_get_innermost_xid(xids);  // xid of transaction doing this insert
    ule_push_insert_uxr(ule, this_xid == TXNID_NONE, this_xid, vallen, valp);
}

// Purpose is to apply a delete message to this leafentry:
static void 
ule_apply_delete(ULE ule, XIDS xids) {
    ule_prepare_for_new_uxr(ule, xids);
    TXNID this_xid = xids_get_innermost_xid(xids);  // xid of transaction doing this delete
    ule_push_delete_uxr(ule, this_xid == TXNID_NONE, this_xid);
}

// First, discard anything done earlier by this transaction.
// Then, add placeholders if necessary.  This transaction may be nested within 
// outer transactions that are newer than then newest (innermost) transaction in
// the leafentry.  If so, record those outer transactions in the leafentry
// with placeholders.
static void 
ule_prepare_for_new_uxr(ULE ule, XIDS xids) {
    TXNID this_xid = xids_get_innermost_xid(xids);
    //This is for LOADER_USE_PUTS or transactionless environment
    //where messages use XIDS of 0
    if (this_xid == TXNID_NONE && ule_get_innermost_xid(ule) == TXNID_NONE) {
        ule_remove_innermost_uxr(ule);
    }
    // case where we are transactional and xids stack matches ule stack
    else if (ule->num_puxrs > 0 && ule_get_innermost_xid(ule) == this_xid) {
        ule_remove_innermost_uxr(ule);
    }
    // case where we are transactional and xids stack does not match ule stack
    else {
        ule_add_placeholders(ule, xids);
    }
}

// Purpose is to apply an abort message to this leafentry.
// If the aborted transaction (the transaction whose xid is the innermost xid
// in the id stack passed in the message), has not modified this leafentry,
// then there is nothing to be done.
// If this transaction did modify the leafentry, then undo whatever it did (by
// removing the transaction record (uxr) and any placeholders underneath.
// Remember, the innermost uxr can only be an insert or a delete, not a placeholder. 
static void 
ule_apply_abort(ULE ule, XIDS xids) {
    TXNID this_xid = xids_get_innermost_xid(xids);   // xid of transaction doing this abort
    invariant(this_xid!=TXNID_NONE);
    UXR innermost = ule_get_innermost_uxr(ule);
    // need to check for provisional entries in ule, otherwise
    // there is nothing to abort, not checking this may result
    // in a bug where the most recently committed has same xid
    // as the XID's innermost
    if (ule->num_puxrs > 0 && innermost->xid == this_xid) {
        invariant(ule->num_puxrs>0);
        ule_remove_innermost_uxr(ule);                    
        ule_remove_innermost_placeholders(ule); 
    }
    invariant(ule->num_cuxrs > 0);
}

static void 
ule_apply_broadcast_commit_all (ULE ule) {
    ule->uxrs[0] = ule->uxrs[ule->num_puxrs + ule->num_cuxrs - 1];
    ule->uxrs[0].xid = TXNID_NONE;
    ule->num_puxrs = 0;
    ule->num_cuxrs = 1;
}

// Purpose is to apply a commit message to this leafentry.
// If the committed transaction (the transaction whose xid is the innermost xid
// in the id stack passed in the message), has not modified this leafentry,
// then there is nothing to be done.
// Also, if there are no uncommitted transaction records there is nothing to do.
// If this transaction did modify the leafentry, then promote whatever it did.
// Remember, the innermost uxr can only be an insert or a delete, not a placeholder. 
void ule_apply_commit(ULE ule, XIDS xids) {
    TXNID this_xid = xids_get_innermost_xid(xids);  // xid of transaction committing
    invariant(this_xid!=TXNID_NONE);
    // need to check for provisional entries in ule, otherwise
    // there is nothing to abort, not checking this may result
    // in a bug where the most recently committed has same xid
    // as the XID's innermost
    if (ule->num_puxrs > 0 && ule_get_innermost_xid(ule) == this_xid) {
        // 3 cases:
        //1- it's already a committed value (do nothing) (num_puxrs==0)
        //2- it's provisional but root level (make a new committed value (num_puxrs==1)
        //3- it's provisional and not root (promote); (num_puxrs>1)
        if (ule->num_puxrs == 1) { //new committed value
            ule_promote_provisional_innermost_to_committed(ule);
        }
        else if (ule->num_puxrs > 1) {
            //ule->uxrs[ule->num_cuxrs+ule->num_puxrs-1] is the innermost (this transaction)
            //ule->uxrs[ule->num_cuxrs+ule->num_puxrs-2] is the 2nd innermost
            //We want to promote the innermost uxr one level out.
            ule_promote_provisional_innermost_to_index(ule, ule->num_cuxrs+ule->num_puxrs-2);
        }
    }
}

///////////////////
// Helper functions called from the functions above:
//

// Purpose is to record an insert for this transaction (and set type correctly).
static void 
ule_push_insert_uxr(ULE ule, bool is_committed, TXNID xid, uint32_t vallen, void * valp) {
    UXR uxr     = ule_get_first_empty_uxr(ule);
    if (is_committed) {
        invariant(ule->num_puxrs==0);
        ule->num_cuxrs++;
    }
    else {
        ule->num_puxrs++;
    }
    uxr->xid    = xid;
    uxr->vallen = vallen;
    uxr->valp   = valp;
    uxr->type   = XR_INSERT;
}

// Purpose is to record a delete for this transaction.  If this transaction
// is the root transaction, then truly delete the leafentry by marking the 
// ule as empty.
static void 
ule_push_delete_uxr(ULE ule, bool is_committed, TXNID xid) {
    UXR uxr     = ule_get_first_empty_uxr(ule);
    if (is_committed) {
        invariant(ule->num_puxrs==0);
        ule->num_cuxrs++;
    }
    else {
        ule->num_puxrs++;
    }
    uxr->xid    = xid;
    uxr->type   = XR_DELETE;
}

// Purpose is to push a placeholder on the top of the leafentry's transaction stack.
static void 
ule_push_placeholder_uxr(ULE ule, TXNID xid) {
    invariant(ule->num_cuxrs>0);
    UXR uxr           = ule_get_first_empty_uxr(ule);
    uxr->xid          = xid;
    uxr->type         = XR_PLACEHOLDER;
    ule->num_puxrs++;
}

// Return innermost transaction record.
static UXR 
ule_get_innermost_uxr(ULE ule) {
    invariant(ule->num_cuxrs > 0);
    UXR rval = &(ule->uxrs[ule->num_cuxrs + ule->num_puxrs - 1]);
    return rval;
}

// Return first empty transaction record
static UXR 
ule_get_first_empty_uxr(ULE ule) {
    invariant(ule->num_puxrs < MAX_TRANSACTION_RECORDS-1);
    UXR rval = &(ule->uxrs[ule->num_cuxrs+ule->num_puxrs]);
    return rval;
}

// Remove the innermost transaction (pop the leafentry's stack), undoing
// whatever the innermost transaction did.
static void 
ule_remove_innermost_uxr(ULE ule) {
    //It is possible to remove the committed delete at first insert.
    invariant(ule->num_cuxrs > 0);
    if (ule->num_puxrs) {
        ule->num_puxrs--;
    }
    else {
        //This is for LOADER_USE_PUTS or transactionless environment
        //where messages use XIDS of 0
        invariant(ule->num_cuxrs == 1);
        invariant(ule_get_innermost_xid(ule)==TXNID_NONE);
        ule->num_cuxrs--;
    }
}

static TXNID 
ule_get_innermost_xid(ULE ule) {
    TXNID rval = ule_get_xid(ule, ule->num_cuxrs + ule->num_puxrs - 1);
    return rval;
}

static TXNID 
ule_get_xid(ULE ule, uint32_t index) {
    invariant(index < ule->num_cuxrs + ule->num_puxrs);
    TXNID rval = ule->uxrs[index].xid;
    return rval;
}

// Purpose is to remove any placeholders from the top of the leaf stack (the 
// innermost recorded transactions), if necessary.  This function is idempotent.
// It makes no logical sense for a placeholder to be the innermost recorded
// transaction record, so placeholders at the top of the stack are not legal.
static void 
ule_remove_innermost_placeholders(ULE ule) {
    UXR uxr = ule_get_innermost_uxr(ule);
    while (uxr_is_placeholder(uxr)) {
        invariant(ule->num_puxrs>0);
        ule_remove_innermost_uxr(ule);
        uxr = ule_get_innermost_uxr(ule);
    }
}

// Purpose is to add placeholders to the top of the leaf stack (the innermost
// recorded transactions), if necessary.  This function is idempotent.
// Note, after placeholders are added, an insert or delete will be added.  This 
// function temporarily leaves the transaction stack in an illegal state (having
// placeholders on top).
static void 
ule_add_placeholders(ULE ule, XIDS xids) {
    //Placeholders can be placed on top of the committed uxr.
    invariant(ule->num_cuxrs > 0);

    uint32_t num_xids = xids_get_num_xids(xids);
    // we assume that implicit promotion has happened
    // when we get this call, so the number of xids MUST
    // be greater than the number of provisional entries
    invariant(num_xids >= ule->num_puxrs);
    // make sure that the xids stack matches up to a certain amount
    // this first for loop is just debug code
    for (uint32_t i = 0; i < ule->num_puxrs; i++) {
        TXNID current_msg_xid = xids_get_xid(xids, i);
        TXNID current_ule_xid = ule_get_xid(ule, i + ule->num_cuxrs);
        invariant(current_msg_xid == current_ule_xid);
    }
    for (uint32_t i = ule->num_puxrs; i < num_xids-1; i++) {
        TXNID current_msg_xid = xids_get_xid(xids, i);
        ule_push_placeholder_uxr(ule, current_msg_xid);
    }
}

uint64_t
ule_num_uxrs(ULE ule) {
    return ule->num_cuxrs + ule->num_puxrs;
}

UXR 
ule_get_uxr(ULE ule, uint64_t ith) {
    invariant(ith < ule_num_uxrs(ule));
    return &ule->uxrs[ith];
}

uint32_t
ule_get_num_committed(ULE ule) {
    return ule->num_cuxrs;
}

uint32_t
ule_get_num_provisional(ULE ule) {
    return ule->num_puxrs;
}

int 
ule_is_committed(ULE ule, uint64_t  ith) {
    invariant(ith < ule_num_uxrs(ule));
    return ith < ule->num_cuxrs;
}

int 
ule_is_provisional(ULE ule, uint64_t ith) {
    invariant(ith < ule_num_uxrs(ule));
    return ith >= ule->num_cuxrs;
}

// return size of data for innermost uxr, the size of val
uint32_t
ule_get_innermost_numbytes(ULE ule, uint32_t keylen) {
    uint32_t rval;
    UXR uxr = ule_get_innermost_uxr(ule);
    if (uxr_is_delete(uxr))
        rval = 0;
    else {
        rval = uxr_get_vallen(uxr) + keylen;
    }
    return rval;
}


/////////////////////////////////////////////////////////////////////////////////
//  This layer of abstraction (uxr_xxx) understands uxr and nothing else.
//

static inline bool
uxr_type_is_insert(uint8_t type) {
    bool rval = (bool)(type == XR_INSERT);
    return rval;
}

bool
uxr_is_insert(UXR uxr) {
    return uxr_type_is_insert(uxr->type);
}

static inline bool
uxr_type_is_delete(uint8_t type) {
    bool rval = (bool)(type == XR_DELETE);
    return rval;
}

bool
uxr_is_delete(UXR uxr) {
    return uxr_type_is_delete(uxr->type);
}

static inline bool
uxr_type_is_placeholder(uint8_t type) {
    bool rval = (bool)(type == XR_PLACEHOLDER);
    return rval;
}

bool
uxr_is_placeholder(UXR uxr) {
    return uxr_type_is_placeholder(uxr->type);
}

void *
uxr_get_val(UXR uxr) {
    return uxr->valp;
}

uint32_t 
uxr_get_vallen(UXR uxr) {
    return uxr->vallen;
}


TXNID 
uxr_get_txnid(UXR uxr) {
    return uxr->xid;
}

static int
le_iterate_get_accepted_index(TXNID *xids, uint32_t *index, uint32_t num_xids, LE_ITERATE_CALLBACK f, TOKUTXN context) {
    uint32_t i;
    int r = 0;
    // if this for loop does not return anything, we return num_xids-1, which should map to T_0
    for (i = 0; i < num_xids - 1; i++) {
        TXNID xid = toku_dtoh64(xids[i]);
        r = f(xid, context);
        if (r==TOKUDB_ACCEPT) {
            r = 0;
            break; //or goto something
        }
        else if (r!=0) {
            break;
        }
    }
    *index = i;
    return r;
}

#if ULE_DEBUG
static void
ule_verify_xids(ULE ule, uint32_t interesting, TXNID *xids) {
    int has_p = (ule->num_puxrs != 0);
    invariant(ule->num_cuxrs + has_p == interesting);
    uint32_t i;
    for (i = 0; i < interesting - 1; i++) {
        TXNID xid = toku_dtoh64(xids[i]);
        invariant(ule->uxrs[ule->num_cuxrs - 1 + has_p - i].xid == xid);
    }
}
#endif

//
// Iterates over "possible" TXNIDs in a leafentry's stack, until one is accepted by 'f'. If the value 
// associated with the accepted TXNID is not an insert, then set *is_emptyp to true, otherwise false
// The "possible" TXNIDs are:
//   if provisionals exist, then the first possible TXNID is the outermost provisional.
//   The next possible TXNIDs are the committed TXNIDs, from most recently committed to T_0.
// If provisionals exist, and the outermost provisional is accepted by 'f', 
// the associated value checked is the innermost provisional's value.
// Parameters:
//    le - leafentry to iterate over
//    f - callback function that checks if a TXNID in le is accepted, and its associated value should be examined.
//    is_delp - output parameter that returns answer
//    context - parameter for f
//
int
le_iterate_is_del(LEAFENTRY le, LE_ITERATE_CALLBACK f, bool *is_delp, TOKUTXN context) {
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
#endif

    uint8_t type = le->type;
    int r;
    bool is_del = false;
    switch (type) {
        case LE_CLEAN: {
            r = 0;
#if ULE_DEBUG
            invariant(ule.num_cuxrs == 1);
            invariant(ule.num_puxrs == 0);
            invariant(uxr_is_insert(ule.uxrs));
#endif
            break;
        }
        case LE_MVCC:;
            uint32_t num_cuxrs;
            num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            uint32_t num_puxrs;
            num_puxrs = le->u.mvcc.num_pxrs;
            uint8_t *p;
            p = le->u.mvcc.xrs;

            uint32_t index;
            uint32_t num_interesting;
            num_interesting = num_cuxrs + (num_puxrs != 0);
            TXNID *xids;
            xids = (TXNID*)p;
#if ULE_DEBUG
            ule_verify_xids(&ule, num_interesting, xids);
#endif
            r = le_iterate_get_accepted_index(xids, &index, num_interesting, f, context);
            if (r!=0) goto cleanup;
            invariant(index < num_interesting);

            //Skip TXNIDs
            p += (num_interesting - 1)*sizeof(TXNID);

            uint32_t *length_and_bits;
            length_and_bits  = (uint32_t*)p;
            uint32_t my_length_and_bit;
            my_length_and_bit = toku_dtoh32(length_and_bits[index]);
            is_del = !IS_INSERT(my_length_and_bit);
#if ULE_DEBUG
            {
                uint32_t has_p = (ule.num_puxrs != 0);
                uint32_t ule_index = (index==0) ? ule.num_cuxrs + ule.num_puxrs - 1 : ule.num_cuxrs - 1 + has_p - index;
                UXR uxr = ule.uxrs + ule_index;
                invariant(uxr_is_delete(uxr) == is_del);
            }
#endif
            break;
        default:
            invariant(false);
    }
cleanup:
#if ULE_DEBUG
    ule_cleanup(&ule);
#endif
    if (!r) *is_delp = is_del;
    return r;
}

//
// Iterates over "possible" TXNIDs in a leafentry's stack, until one is accepted by 'f'. Set
// valpp and vallenp to value and length associated with accepted TXNID
// The "possible" TXNIDs are:
//   if provisionals exist, then the first possible TXNID is the outermost provisional.
//   The next possible TXNIDs are the committed TXNIDs, from most recently committed to T_0.
// If provisionals exist, and the outermost provisional is accepted by 'f', 
// the associated length value is the innermost provisional's length and value.
// Parameters:
//    le - leafentry to iterate over
//    f - callback function that checks if a TXNID in le is accepted, and its associated value should be examined.
//    valpp - output parameter that returns pointer to value
//    vallenp - output parameter that returns length of value
//    context - parameter for f
//
int
le_iterate_val(LEAFENTRY le, LE_ITERATE_CALLBACK f, void** valpp, uint32_t *vallenp, TOKUTXN context) {
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
#endif

    uint8_t type = le->type;
    int r;
    uint32_t vallen = 0;
    void *valp = NULL;
    switch (type) {
        case LE_CLEAN: {
            vallen = toku_dtoh32(le->u.clean.vallen);
            valp   = le->u.clean.val;
            r = 0;
#if ULE_DEBUG
            invariant(ule.num_cuxrs == 1);
            invariant(ule.num_puxrs == 0);
            invariant(uxr_is_insert(ule.uxrs));
            invariant(ule.uxrs[0].vallen == vallen);
            invariant(ule.uxrs[0].valp == valp);
#endif
            break;
        }
        case LE_MVCC:;
            uint32_t num_cuxrs;
            num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            uint32_t num_puxrs;
            num_puxrs = le->u.mvcc.num_pxrs;
            uint8_t *p;
            p = le->u.mvcc.xrs;

            uint32_t index;
            uint32_t num_interesting;
            num_interesting = num_cuxrs + (num_puxrs != 0);
            TXNID *xids;
            xids = (TXNID*)p;
#if ULE_DEBUG
            ule_verify_xids(&ule, num_interesting, xids);
#endif
            r = le_iterate_get_accepted_index(xids, &index, num_interesting, f, context);
            if (r!=0) goto cleanup;
            invariant(index < num_interesting);

            //Skip TXNIDs
            p += (num_interesting - 1)*sizeof(TXNID);

            UXR_S temp;
            size_t offset;
            offset = 0;

            uint32_t *length_and_bits;
            length_and_bits  = (uint32_t*)p;
            uint32_t i;
            //evaluate the offset
            for (i=0; i < index; i++){
                uxr_unpack_length_and_bit(&temp, (uint8_t*)&length_and_bits[i]);
                offset += temp.vallen;
            }
            uxr_unpack_length_and_bit(&temp, (uint8_t*)&length_and_bits[index]);
            if (uxr_is_delete(&temp)) {
                goto verify_is_empty;
            }
            vallen = temp.vallen;
            
            // move p past the length and bits, now points to beginning of data
            p += num_interesting*sizeof(uint32_t);
            // move p to point to the data we care about
            p += offset;
            valp = p;

#if ULE_DEBUG
            {
                uint32_t has_p = (ule.num_puxrs != 0);
                uint32_t ule_index = (index==0) ? ule.num_cuxrs + ule.num_puxrs - 1 : ule.num_cuxrs - 1 + has_p - index;
                UXR uxr = ule.uxrs + ule_index;
                invariant(uxr_is_insert(uxr));
                invariant(uxr->vallen == vallen);
                invariant(uxr->valp == valp);
            }
#endif
            if (0) {
verify_is_empty:;
#if ULE_DEBUG
                uint32_t has_p = (ule.num_puxrs != 0);
                UXR uxr = ule.uxrs + ule.num_cuxrs - 1 + has_p - index;
                invariant(uxr_is_delete(uxr));
#endif
            }
            break;
        default:
            invariant(false);
    }
cleanup:
#if ULE_DEBUG
    ule_cleanup(&ule);
#endif
    if (!r) {
        *valpp   = valp;
        *vallenp = vallen;
    }
    return r;
}

#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif
// This is an on-disk format.  static_asserts verify everything is packed and aligned correctly.
struct __attribute__ ((__packed__)) leafentry_13 {
    struct leafentry_committed_13 {
        uint8_t key_val[0];     //Actual key, then actual val
    };
    static_assert(0 == sizeof(leafentry_committed_13), "wrong size");
    static_assert(0 == __builtin_offsetof(leafentry_committed_13, key_val), "wrong offset");
    struct __attribute__ ((__packed__)) leafentry_provisional_13 {
        uint8_t innermost_type;
        TXNID    xid_outermost_uncommitted;
        uint8_t key_val_xrs[0];  //Actual key,
        //then actual innermost inserted val,
        //then transaction records.
    };
    static_assert(9 == sizeof(leafentry_provisional_13), "wrong size");
    static_assert(9 == __builtin_offsetof(leafentry_provisional_13, key_val_xrs), "wrong offset");

    uint8_t  num_xrs;
    uint32_t keylen;
    uint32_t innermost_inserted_vallen;
    union __attribute__ ((__packed__)) {
        struct leafentry_committed_13 comm;
        struct leafentry_provisional_13 prov;
    } u;
};
static_assert(18 == sizeof(leafentry_13), "wrong size");
static_assert(9 == __builtin_offsetof(leafentry_13, u), "wrong offset");
#if TOKU_WINDOWS
#pragma pack(pop)
#endif

//Requires:
//  Leafentry that ule represents should not be destroyed (is not just all deletes)
static size_t
le_memsize_from_ule_13 (ULE ule, LEAFENTRY_13 le) {
    uint32_t num_uxrs = ule->num_cuxrs + ule->num_puxrs;
    assert(num_uxrs);
    size_t rval;
    if (num_uxrs == 1) {
        assert(uxr_is_insert(&ule->uxrs[0]));
        rval = 1                    //num_uxrs
              +4                    //keylen
              +4                    //vallen
              +le->keylen          //actual key
              +ule->uxrs[0].vallen; //actual val
    }
    else {
        rval = 1                    //num_uxrs
              +4                    //keylen
              +le->keylen          //actual key
              +1*num_uxrs      //types
              +8*(num_uxrs-1); //txnids
        uint8_t i;
        for (i = 0; i < num_uxrs; i++) {
            UXR uxr = &ule->uxrs[i];
            if (uxr_is_insert(uxr)) {
                rval += 4;           //vallen
                rval += uxr->vallen; //actual val
            }
        }
    }
    return rval;
}

//This function is mostly copied from 4.1.1 (which is version 12, same as 13 except that only 13 is upgradable).
// Note, number of transaction records in version 13 has been replaced by separate counters in version 14 (MVCC),
// one counter for committed transaction records and one counter for provisional transaction records.  When 
// upgrading a version 13 le to version 14, the number of committed transaction records is always set to one (1)
// and the number of provisional transaction records is set to the original number of transaction records 
// minus one.  The bottom transaction record is assumed to be a committed value.  (If there is no committed
// value then the bottom transaction record of version 13 is a committed delete.)
// This is the only change from the 4.1.1 code.  The rest of the leafentry is read as is.
static void
le_unpack_13(ULE ule, LEAFENTRY_13 le) {
    //Read num_uxrs
    uint8_t num_xrs = le->num_xrs;
    assert(num_xrs > 0);
    ule->uxrs = ule->uxrs_static; //Static version is always enough.
    ule->num_cuxrs = 1;
    ule->num_puxrs = num_xrs - 1;

    //Read the keylen
    uint32_t keylen = toku_dtoh32(le->keylen);

    //Read the vallen of innermost insert
    uint32_t vallen_of_innermost_insert = toku_dtoh32(le->innermost_inserted_vallen);

    uint8_t *p;
    if (num_xrs == 1) {
        //Unpack a 'committed leafentry' (No uncommitted transactions exist)
        ule->uxrs[0].type   = XR_INSERT; //Must be or the leafentry would not exist
        ule->uxrs[0].vallen = vallen_of_innermost_insert;
        ule->uxrs[0].valp   = &le->u.comm.key_val[keylen];
        ule->uxrs[0].xid    = 0;          //Required.

        //Set p to immediately after leafentry
        p = &le->u.comm.key_val[keylen + vallen_of_innermost_insert];
    }
    else {
        //Unpack a 'provisional leafentry' (Uncommitted transactions exist)

        //Read in type.
        uint8_t innermost_type = le->u.prov.innermost_type;
        assert(!uxr_type_is_placeholder(innermost_type));

        //Read in xid
        TXNID xid_outermost_uncommitted = toku_dtoh64(le->u.prov.xid_outermost_uncommitted);

        //Read pointer to innermost inserted val (immediately after key)
        uint8_t *valp_of_innermost_insert = &le->u.prov.key_val_xrs[keylen];

        //Point p to immediately after 'header'
        p = &le->u.prov.key_val_xrs[keylen + vallen_of_innermost_insert];

        bool found_innermost_insert = false;
        int i; //Index in ULE.uxrs[]
        //Loop inner to outer
        for (i = num_xrs - 1; i >= 0; i--) {
            UXR uxr = &ule->uxrs[i];

            //Innermost's type is in header.
            if (i < num_xrs - 1) {
                //Not innermost, so load the type.
                uxr->type = *p;
                p += 1;
            }
            else {
                //Innermost, load the type previously read from header
                uxr->type = innermost_type;
            }

            //Committed txn id is implicit (0).  (i==0)
            //Outermost uncommitted txnid is stored in header. (i==1)
            if (i > 1) {
                //Not committed nor outermost uncommitted, so load the xid.
                uxr->xid = toku_dtoh64(*(TXNID*)p);
                p += 8;
            }
            else if (i == 1) {
                //Outermost uncommitted, load the xid previously read from header
                uxr->xid = xid_outermost_uncommitted;
            }
            else {
                // i == 0, committed entry
                uxr->xid = 0;
            }

            if (uxr_is_insert(uxr)) {
                if (found_innermost_insert) {
                    //Not the innermost insert.  Load vallen/valp
                    uxr->vallen = toku_dtoh32(*(uint32_t*)p);
                    p += 4;

                    uxr->valp = p;
                    p += uxr->vallen;
                }
                else {
                    //Innermost insert, load the vallen/valp previously read from header
                    uxr->vallen = vallen_of_innermost_insert;
                    uxr->valp   = valp_of_innermost_insert;
                    found_innermost_insert = true;
                }
            }
        }
        assert(found_innermost_insert);
    }
#if ULE_DEBUG
    size_t memsize = le_memsize_from_ule_13(ule);
    assert(p == ((uint8_t*)le) + memsize);
#endif
}

size_t
leafentry_disksize_13(LEAFENTRY_13 le) {
    ULE_S ule;
    le_unpack_13(&ule, le);
    size_t memsize = le_memsize_from_ule_13(&ule, le);
    ule_cleanup(&ule);
    return memsize;
}

int 
toku_le_upgrade_13_14(LEAFENTRY_13 old_leafentry,
                     void** keyp,
                     uint32_t* keylen,
                     size_t *new_leafentry_memorysize, 
                     LEAFENTRY *new_leafentry_p
                     ) {
    ULE_S ule;
    int rval;
    invariant(old_leafentry);
    le_unpack_13(&ule, old_leafentry);
    // get the key
    *keylen = old_leafentry->keylen;
    if (old_leafentry->num_xrs == 1) {
        *keyp = old_leafentry->u.comm.key_val;
    }
    else {
        *keyp = old_leafentry->u.prov.key_val_xrs;
    }
    // We used to pass NULL for omt and mempool, so that we would use
    // malloc instead of a mempool.  However after supporting upgrade,
    // we need to use mempools and the OMT.
    rval = le_pack(&ule, // create packed leafentry
                   NULL,
                   0, //only matters if we are passing in a bn_data
                   NULL, //only matters if we are passing in a bn_data
                   0, //only matters if we are passing in a bn_data
                   0, //only matters if we are passing in a bn_data
                   new_leafentry_p);  
    ule_cleanup(&ule);
    *new_leafentry_memorysize = leafentry_memsize(*new_leafentry_p);
    return rval;
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_ule_helgrind_ignore(void);
void
toku_ule_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&le_status, sizeof le_status);
}

#undef STATUS_VALUE
