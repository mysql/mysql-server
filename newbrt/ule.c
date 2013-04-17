/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id$"
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."


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
#include "brttypes.h"
#include "brt-internal.h"

// Sorry:
#include "mempool.h"
#include "omt.h"


#include "leafentry.h"
#include "xids.h"
#include "brt_msg.h"
#include "ule.h"
#include "ule-internal.h"


#define ULE_DEBUG 0

static LE_STATUS_S status;


///////////////////////////////////////////////////////////////////////////////////
// Accessor functions used by outside world (e.g. indexer)
//

ULEHANDLE 
toku_ule_create(void * le_p) {
    ULE ule_p = toku_malloc(sizeof(ULE_S));
    le_unpack(ule_p, le_p);
    return (ULEHANDLE) ule_p;
}

void toku_ule_free(ULEHANDLE ule_p) {
    ule_cleanup((ULE) ule_p);
    toku_free(ule_p);
}

void 
toku_le_get_status(LE_STATUS s) {
    *s = status;
}


///////////////////////////////////////////////////////////////////////////////////
//
// Question: Can any software outside this file modify or read a leafentry?  
// If so, is it worthwhile to put it all here?
//
// There are two entries, one each for modification and query:
//   apply_msg_to_leafentry()        performs all inserts/deletes/aborts
//   do_implicit_promotions_query()  
//
//
//
//

//This is what we use to initialize Xuxrs[0] in a new unpacked leafentry.
const UXR_S committed_delete = {
    .type   = XR_DELETE,
    .vallen = 0,
    .xid    = 0,
    .valp   = NULL
};  // static allocation of uxr with type set to committed delete and xid = 0

#define INSERT_LENGTH(len) ((1U << 31) | len)
#define DELETE_LENGTH(len) (0)
#define GET_LENGTH(len) (len & ((1U << 31)-1))
#define IS_INSERT(len)  (len & (1U << 31))
#define IS_VALID_LEN(len) (len < (1U<<31))

// Local functions:

static void msg_init_empty_ule(ULE ule, BRT_MSG msg);
static void msg_modify_ule(ULE ule, BRT_MSG msg);
static void ule_init_empty_ule(ULE ule, u_int32_t keylen, void * keyp);
static void ule_do_implicit_promotions(ULE ule, XIDS xids);
static void ule_promote_provisional_innermost_to_index(ULE ule, uint32_t index);
static void ule_promote_provisional_innermost_to_committed(ULE ule);
static void ule_apply_insert(ULE ule, XIDS xids, u_int32_t vallen, void * valp);
static void ule_apply_delete(ULE ule, XIDS xids);
static void ule_prepare_for_new_uxr(ULE ule, XIDS xids);
static void ule_apply_abort(ULE ule, XIDS xids);
static void ule_apply_commit(ULE ule, XIDS xids);
static void ule_push_insert_uxr(ULE ule, BOOL is_committed, TXNID xid, u_int32_t vallen, void * valp);
static void ule_push_delete_uxr(ULE ule, BOOL is_committed, TXNID xid);
static void ule_push_placeholder_uxr(ULE ule, TXNID xid);
static UXR ule_get_innermost_uxr(ULE ule);
static UXR ule_get_first_empty_uxr(ULE ule);
static void ule_remove_innermost_uxr(ULE ule);
static TXNID ule_get_innermost_xid(ULE ule);
static TXNID ule_get_xid(ULE ule, uint32_t index);
static void ule_remove_innermost_placeholders(ULE ule);
static void ule_add_placeholders(ULE ule, XIDS xids);
static void ule_optimize(ULE ule, XIDS xids);
static inline BOOL uxr_type_is_insert(u_int8_t type);
static inline BOOL uxr_type_is_delete(u_int8_t type);
static inline BOOL uxr_type_is_placeholder(u_int8_t type);
static inline size_t uxr_pack_txnid(UXR uxr, uint8_t *p);
static inline size_t uxr_pack_type_and_length(UXR uxr, uint8_t *p);
static inline size_t uxr_pack_length_and_bit(UXR uxr, uint8_t *p);
static inline size_t uxr_pack_data(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_txnid(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_type_and_length(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_length_and_bit(UXR uxr, uint8_t *p);
static inline size_t uxr_unpack_data(UXR uxr, uint8_t *p);

static void *
le_malloc(OMT omt, struct mempool *mp, size_t size, void **maybe_free)
{
    if (omt)
	return mempool_malloc_from_omt(omt, mp, size, maybe_free);
    else
	return toku_malloc(size);
}


/////////////////////////////////////////////////////////////////////
// Garbage collection related functions
//

static TXNID
get_next_older_txnid(TXNID xc, OMT omt) {
    int r;
    TXNID *xid;
    OMTVALUE v;
    uint32_t idx;
    TXNID rval;
    r = toku_omt_find(omt, toku_find_pair_by_xid, &xc, -1, &v, &idx, NULL);
    if (r==0) {
        xid = v;
        invariant(*xid < xc); //sanity check
        rval = *xid;
    }
    else {
        invariant(r==DB_NOTFOUND);
        rval = TXNID_NONE;
    }
    return rval;
}

TXNID
toku_get_youngest_live_list_txnid_for(TXNID xc, OMT live_list_reverse) {
    OMTVALUE pairv;
    XID_PAIR pair;
    uint32_t idx;
    TXNID rval;
    int r;
    r = toku_omt_find_zero(live_list_reverse, toku_find_pair_by_xid, &xc, &pairv, &idx, NULL);
    if (r==0) {
        pair = pairv;
        invariant(pair->xid1 == xc); //sanity check
        rval = pair->xid2;
    }
    else {
        invariant(r==DB_NOTFOUND);
        rval = TXNID_NONE;
    }
    return rval;
}

//
// This function returns TRUE if live transaction TL1 is allowed to read a value committed by
// transaction xc, false otherwise.
//
static BOOL
xid_reads_committed_xid(TXNID tl1, TXNID xc, OMT live_list_reverse) {
    BOOL rval;
    if (tl1 < xc) rval = FALSE; //cannot read a newer txn
    else {
        TXNID x = toku_get_youngest_live_list_txnid_for(xc, live_list_reverse);
        if (x == TXNID_NONE) rval = TRUE; //Not in ANY live list, tl1 can read it.
        else rval = tl1 > x;              //Newer than the 'newest one that has it in live list'
        // we know tl1 > xc
        // we know x > xc
        // if tl1 == x, then we do not read, because tl1 is in xc's live list
        // if x is older than tl1, that means that xc < x < tl1
        // and if xc is in x's live list, it CANNOT be in tl1's live list
    }
    return rval;
}

static void
garbage_collection(ULE ule, OMT snapshot_xids, OMT live_list_reverse) {
    if (ule->num_cuxrs == 1) goto done;
    // will fail if too many num_cuxrs
    BOOL necessary_static[MAX_TRANSACTION_RECORDS];
    BOOL *necessary = necessary_static;
    if (ule->num_cuxrs >= MAX_TRANSACTION_RECORDS) {
        XMALLOC_N(ule->num_cuxrs, necessary);
    }
    memset(necessary, 0, sizeof(necessary[0])*ule->num_cuxrs);

    uint32_t curr_committed_entry = ule->num_cuxrs - 1;
    while (TRUE) {
        // mark the curr_committed_entry as necessary
        necessary[curr_committed_entry] = TRUE;
        if (curr_committed_entry == 0) break; //nothing left

        // find the youngest live transaction that reads something 
        // below curr_committed_entry, if it exists
        TXNID tl1;
        TXNID xc = ule->uxrs[curr_committed_entry].xid;

        tl1 = toku_get_youngest_live_list_txnid_for(xc, live_list_reverse);
        if (tl1 == TXNID_NONE || tl1 == xc) {
            // set tl1 to youngest live transaction older than ule->uxrs[curr_committed_entry]->xid
            tl1 = get_next_older_txnid(xc, snapshot_xids);
            if (tl1 == TXNID_NONE) {
                //Remainder is garbage, we're done
                break;
            }
        }
        if (garbage_collection_debug)
        {
            u_int32_t idx;
            OMTVALUE txnagain;
            int r = toku_omt_find_zero(snapshot_xids, toku_find_xid_by_xid, &tl1, &txnagain, &idx, NULL);
            invariant(r==0); //make sure that the txn you are claiming is live is actually live
        }
        //
        // tl1 should now be set
        //
        curr_committed_entry--;
        while (curr_committed_entry > 0) {
            xc = ule->uxrs[curr_committed_entry].xid;
            if (xid_reads_committed_xid(tl1, xc, live_list_reverse)) {
                break;
            }
            curr_committed_entry--;
        }
    } 
    uint32_t first_free = 0;
    uint32_t i;
    for (i = 0; i < ule->num_cuxrs; i++) {
        //Shift values to 'delete' garbage values.
        if (necessary[i]) {
            ule->uxrs[first_free] = ule->uxrs[i];
            first_free++;
        }
    }
    uint32_t saved = first_free;
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
//        a leafentry.
//
// Return 0 if ???  (looking at original code, it seems that it always returns 0).
// ??? How to inform caller that leafentry is to be destroyed?
// 
// Temporarily declared as static until we are ready to remove wrapper apply_cmd_to_leaf().
// 
int 
apply_msg_to_leafentry(BRT_MSG   msg,		// message to apply to leafentry
		       LEAFENTRY old_leafentry, // NULL if there was no stored data.
		       size_t *new_leafentry_memorysize, 
		       size_t *new_leafentry_disksize, 
		       LEAFENTRY *new_leafentry_p,
		       OMT omt, 
		       struct mempool *mp, 
		       void **maybe_free,
                       OMT snapshot_xids,
                       OMT live_list_reverse) {
    ULE_S ule;
    int rval;

    if (old_leafentry == NULL)           // if leafentry does not exist ...
        msg_init_empty_ule(&ule, msg);   // ... create empty unpacked leaf entry
    else 
        le_unpack(&ule, old_leafentry); // otherwise unpack leafentry 
    msg_modify_ule(&ule, msg);          // modify unpacked leafentry
    if (snapshot_xids && live_list_reverse) {
        garbage_collection(&ule, snapshot_xids, live_list_reverse);
    }
    rval = le_pack(&ule,                // create packed leafentry
        new_leafentry_memorysize, 
        new_leafentry_disksize, 
        new_leafentry_p,
        omt, mp, maybe_free);                       
    ule_cleanup(&ule);
    return rval;
}



/////////////////////////////////////////////////////////////////////////////////
// This layer of abstraction (msg_xxx)
// knows the accessors of msg, but not of leafentry or unpacked leaf entry.
// It makes calls into the lower layer (le_xxx) which handles leafentries.

// Purpose is to init the ule with given key and no transaction records
// 
static void 
msg_init_empty_ule(ULE ule, BRT_MSG msg) {   
    u_int32_t keylen = brt_msg_get_keylen(msg);
    void     *keyp   = brt_msg_get_key(msg);
    ule_init_empty_ule(ule, keylen, keyp);
}


// Purpose is to modify the unpacked leafentry in our private workspace.
//
static void 
msg_modify_ule(ULE ule, BRT_MSG msg) {
    XIDS xids = brt_msg_get_xids(msg);
    invariant(xids_get_num_xids(xids) < MAX_TRANSACTION_RECORDS);
    enum brt_msg_type type = brt_msg_get_type(msg);
    if (type != BRT_OPTIMIZE && type != BRT_OPTIMIZE_FOR_UPGRADE) {
        ule_do_implicit_promotions(ule, xids);
    }
    switch (type) {
    case BRT_INSERT_NO_OVERWRITE: {
        UXR old_innermost_uxr = ule_get_innermost_uxr(ule);
        //If something exists, quit (no overwrite).
        if (uxr_is_insert(old_innermost_uxr)) break;
        //else it is just an insert, so
        //fall through to BRT_INSERT on purpose.
    }
    case BRT_INSERT: ;
        u_int32_t vallen = brt_msg_get_vallen(msg);
        invariant(IS_VALID_LEN(vallen));
        void * valp      = brt_msg_get_val(msg);
        ule_apply_insert(ule, xids, vallen, valp);
        break;
    case BRT_DELETE_ANY:
        ule_apply_delete(ule, xids);
        break;
    case BRT_ABORT_ANY:
    case BRT_ABORT_BROADCAST_TXN:
        ule_apply_abort(ule, xids);
        break;
    case BRT_COMMIT_ANY:
    case BRT_COMMIT_BROADCAST_TXN:
        ule_apply_commit(ule, xids);
        break;
    case BRT_OPTIMIZE:
    case BRT_OPTIMIZE_FOR_UPGRADE:
        ule_optimize(ule, xids);
        break;
    default:
        assert(FALSE /* illegal BRT_MSG.type */);
        break;
    }
}

void 
test_msg_modify_ule(ULE ule, BRT_MSG msg){
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

// Purpose of le_unpack() is to populate our private workspace with the contents of the given le.
void
le_unpack(ULE ule, LEAFENTRY le) {
    //Read the keylen
    ule->keylen = toku_dtoh32(le->keylen);
    uint8_t  type = le->type;
    uint8_t *p;
    uint32_t i;
    switch (type) {
        case LE_CLEAN: {
            ule->uxrs = ule->uxrs_static; //Static version is always enough.
            ule->num_cuxrs = 1;
            ule->num_puxrs = 0;
            ule->keyp   = le->u.clean.key_val;
            UXR uxr     = ule->uxrs;
            uxr->type   = XR_INSERT;
            uxr->vallen = toku_dtoh32(le->u.clean.vallen);
            uxr->valp   = le->u.clean.key_val + ule->keylen;
            uxr->xid    = TXNID_NONE;
            //Set p to immediately after leafentry
            p = le->u.clean.key_val + ule->keylen + uxr->vallen;
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
            ule->keyp = le->u.mvcc.key_xrs;
            p = le->u.mvcc.key_xrs + ule->keylen;

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
            invariant(FALSE);
    }
    
#if ULE_DEBUG
    size_t memsize = le_memsize_from_ule(ule);
    assert(p == ((u_int8_t*)le) + memsize);
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
update_le_status(ULE ule, size_t memsize, LE_STATUS s) {
    if (ule->num_cuxrs > s->max_committed_xr)
	s->max_committed_xr = ule->num_cuxrs;
    if (ule->num_puxrs > s->max_provisional_xr)
	s->max_provisional_xr = ule->num_puxrs;
    if (ule->num_cuxrs > MAX_TRANSACTION_RECORDS)
	s->expanded++;
    if (memsize > s->max_memsize)
	s->max_memsize = memsize;
}

// Purpose is to return a newly allocated leaf entry in packed format, or
// return null if leaf entry should be destroyed (if no transaction records 
// are for inserts).
// Transaction records in packed le are stored inner to outer (first xr is innermost),
// with some information extracted out of the transaction records into the header.
// Transaction records in ule are stored outer to inner (uxr[0] is outermost).
int
le_pack(ULE ule,                            // data to be packed into new leafentry
	size_t *new_leafentry_memorysize, 
	size_t *new_leafentry_disksize, 
	LEAFENTRY * const new_leafentry_p,   // this is what this function creates
	OMT omt, 
	struct mempool *mp, 
	void **maybe_free) {
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
        *new_leafentry_p = NULL;
        rval = 0;
        goto cleanup;
    }
found_insert:;
    memsize = le_memsize_from_ule(ule);
    LEAFENTRY new_leafentry = le_malloc(omt, mp, memsize, maybe_free);
    if (new_leafentry==NULL) {
        rval = ENOMEM;
        goto cleanup;
    }
    //Universal data
    new_leafentry->keylen  = toku_htod32(ule->keylen);

    //p always points to first unused byte after leafentry we are packing
    u_int8_t *p;
    invariant(ule->num_cuxrs>0);
    //Type specific data
    if (ule->num_cuxrs == 1 && ule->num_puxrs == 0) {
        //Pack a 'clean leafentry' (no uncommitted transactions, only one committed value)
        new_leafentry->type = LE_CLEAN;

        uint32_t vallen = ule->uxrs[0].vallen;
        //Store vallen
        new_leafentry->u.clean.vallen  = toku_htod32(vallen);

        //Store actual key
        memcpy(new_leafentry->u.clean.key_val, ule->keyp, ule->keylen);

        //Store actual val immediately after actual key
        memcpy(new_leafentry->u.clean.key_val + ule->keylen, ule->uxrs[0].valp, vallen);

        //Set p to after leafentry
        p = new_leafentry->u.clean.key_val + ule->keylen + vallen;
    }
    else {
        uint32_t i;
        //Pack an 'mvcc leafentry'
        new_leafentry->type = LE_MVCC;

        new_leafentry->u.mvcc.num_cxrs = toku_htod32(ule->num_cuxrs);
        new_leafentry->u.mvcc.num_pxrs = ule->num_puxrs;

        //Store actual key.
        memcpy(new_leafentry->u.mvcc.key_xrs, ule->keyp, ule->keylen);

        p = new_leafentry->u.mvcc.key_xrs + ule->keylen;

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

    size_t bytes_written = (size_t)p - (size_t)new_leafentry;
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
                            &le_copy, NULL, NULL, NULL);
        invariant(r_tmp==0);
        invariant(memsize_verify == memsize);

        invariant(memcmp(new_leafentry, le_copy, memsize)==0);
        toku_free(le_copy);

        ule_cleanup(&ule_tmp);
    }
#endif

    *new_leafentry_p = (LEAFENTRY)new_leafentry;
    *new_leafentry_memorysize = memsize;
    *new_leafentry_disksize   = memsize;
    rval = 0;
cleanup:
    update_le_status(ule, memsize, &status);
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
              +4                    //keylen
              +4                    //vallen
              +ule->keylen          //actual key
              +committed->vallen;   //actual val
    }
    else {
        rval = 1                    //type
              +4                    //num_cuxrs
              +1                    //num_puxrs
              +4                    //keylen
              +ule->keylen          //actual key
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

size_t
leafentry_memsize (LEAFENTRY le) {
    size_t rval = 0;

    uint32_t keylen = toku_dtoh32(le->keylen);
    uint8_t  type = le->type;

    uint8_t *p;
    switch (type) {
        case LE_CLEAN: {
            uint32_t vallen = toku_dtoh32(le->u.clean.vallen);
            rval = LE_CLEAN_MEMSIZE(keylen, vallen);
            break;
        }
        case LE_MVCC: {
            UXR_S uxr;
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;
            size_t   lengths = 0;

            //Position p after the key.
            p = le->u.mvcc.key_xrs + keylen;

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
            rval = (size_t)p - (size_t)le;
            break;
        }
        default:
            invariant(FALSE);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    size_t slow_rval = le_memsize_from_ule(&ule);
    if (slow_rval!=rval) {
        int r = print_leafentry(stderr, le);
        fprintf(stderr, "\nSlow: [%"PRIu64"] Fast: [%"PRIu64"]\n", slow_rval, rval);
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


// le is normally immutable.  This is the only exception.
void
le_clean_xids(LEAFENTRY le,
              size_t *new_leafentry_memorysize, 
              size_t *new_leafentry_disksize) {
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    LEAFENTRY le_copy;
    UXR uxr = ule_get_innermost_uxr(&ule);
    invariant(ule.num_cuxrs > 1 || ule.num_puxrs > 0);
    ule.num_cuxrs = 0;
    ule.num_puxrs = 0;
    ule_push_insert_uxr(&ule, TRUE,
                        TXNID_NONE,
                        uxr->vallen,
                        uxr->valp);
    size_t memsize_old = leafentry_memsize(le);
    size_t memsize_verify;
    int r_tmp = le_pack(&ule, &memsize_verify, &memsize_verify,
                        &le_copy, NULL, NULL, NULL);
    invariant(r_tmp==0);
    ule_cleanup(&ule);
#endif

    invariant(le->type != LE_CLEAN);
    uint32_t keylen;
    uint32_t vallen;
    void *keyp = le_key_and_len(le, &keylen);
    void *valp = le_latest_val_and_len(le, &vallen);
    invariant(valp);
    
    //le->keylen unchanged
    le->type = LE_CLEAN;
    le->u.clean.vallen = toku_htod32(vallen);
    memmove(le->u.clean.key_val, keyp, keylen);
    memmove(le->u.clean.key_val + keylen, valp, vallen);

    size_t memsize = leafentry_memsize(le);
    *new_leafentry_disksize = *new_leafentry_memorysize = memsize;

#if ULE_DEBUG
    invariant(memsize_verify == memsize);
    invariant(memsize_old    >= memsize);
    invariant(!memcmp(le, le_copy, memsize));
#endif
}

BOOL
le_is_clean(LEAFENTRY le) {
    uint8_t  type = le->type;
    uint32_t rval;
    switch (type) {
        case LE_CLEAN:
            rval = TRUE;
            break;
        case LE_MVCC:;
            rval = FALSE;
            break;
        default:
            invariant(FALSE);
    }
    return rval;
}

int le_latest_is_del(LEAFENTRY le) {
    int rval;
    uint32_t keylen = toku_dtoh32(le->keylen);
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

            //Position p after the key.
            p = le->u.mvcc.key_xrs + keylen;

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
            invariant(FALSE);
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

int
le_has_xids(LEAFENTRY le, XIDS xids) {
    //Read num_uxrs
    uint32_t num_xids = xids_get_num_xids(xids);
    invariant(num_xids > 0); //Disallow checking for having TXNID_NONE
    TXNID xid = xids_get_xid(xids, 0);
    invariant(xid!=TXNID_NONE);

    int rval = le_outermost_uncommitted_xid(le) == xid;
    return rval;
}

u_int32_t
le_latest_keylen (LEAFENTRY le) {
    u_int32_t rval;
    rval = le_latest_is_del(le) ? 0 : le_keylen(le);
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    u_int32_t slow_rval;
    if (uxr_is_insert(uxr)) {
        slow_rval = ule.keylen;
    }
    else {
        slow_rval = 0;
    }
    ule_cleanup(&ule);
    invariant(slow_rval == rval);
#endif
    return rval;
}

void*
le_latest_val_and_len (LEAFENTRY le, u_int32_t *len) {
    uint32_t keylen = toku_dtoh32(le->keylen);
    uint8_t  type = le->type;
    void *valp;

    uint8_t *p;
    switch (type) {
        case LE_CLEAN:
            *len = toku_dtoh32(le->u.clean.vallen);
            valp = le->u.clean.key_val + keylen;
            break;
        case LE_MVCC:;
            UXR_S uxr;
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;

            //Position p after the key.
            p = le->u.mvcc.key_xrs + keylen;

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
            invariant(FALSE);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    void     *slow_valp;
    u_int32_t slow_len;
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
u_int32_t
le_latest_vallen (LEAFENTRY le) {
    u_int32_t rval;
    uint32_t keylen = toku_dtoh32(le->keylen);
    uint8_t  type = le->type;
    uint8_t *p;
    switch (type) {
        case LE_CLEAN:
            rval = toku_dtoh32(le->u.clean.vallen);
            break;
        case LE_MVCC:;
            UXR_S uxr;
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            invariant(num_cuxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;

            //Position p after the key.
            p = le->u.mvcc.key_xrs + keylen;

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
            invariant(FALSE);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    u_int32_t slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = uxr->vallen;
    else
        slow_rval = 0;
    ule_cleanup(&ule);
    invariant(slow_rval == rval);
#endif
    return rval;
}

//Return key and keylen unconditionally
void*
le_key_and_len (LEAFENTRY le, u_int32_t *len) {
    *len = toku_dtoh32(le->keylen);
    uint8_t  type = le->type;

    void *keyp;
    switch (type) {
        case LE_CLEAN:
            keyp = le->u.clean.key_val;
            break;
        case LE_MVCC:
            keyp = le->u.mvcc.key_xrs;
            break;
        default:
            invariant(FALSE);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    void     *slow_keyp;
    u_int32_t slow_len;
    slow_keyp = ule.keyp;
    slow_len  = ule.keylen; 
    assert(slow_keyp == le_key(le));
    assert(slow_len == le_keylen(le));
    assert(keyp==slow_keyp);
    assert(*len==slow_len);
    ule_cleanup(&ule);
#endif
    return keyp;
}


//WILL BE DELETED can be slow
void*
le_key (LEAFENTRY le) {
    uint8_t  type = le->type;

    void *rval;
    switch (type) {
        case LE_CLEAN:
            rval = le->u.clean.key_val;
            break;
        case LE_MVCC:
            rval = le->u.mvcc.key_xrs;
            break;
        default:
            invariant(FALSE);
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    void *slow_rval = ule.keyp;
    invariant(slow_rval == rval);
    ule_cleanup(&ule);
#endif
    return rval;
}

u_int32_t
le_keylen (LEAFENTRY le) {
    u_int32_t rval = toku_dtoh32(le->keylen);
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    u_int32_t slow_rval = ule.keylen;
    assert(rval==slow_rval);
    ule_cleanup(&ule);
#endif
    return rval;
}


u_int64_t 
le_outermost_uncommitted_xid (LEAFENTRY le) {
    uint64_t rval = TXNID_NONE;

    uint32_t keylen = toku_dtoh32(le->keylen);
    uint8_t  type = le->type;

    uint8_t *p;
    switch (type) {
        case LE_CLEAN:
            break;
        case LE_MVCC:;
            UXR_S uxr;
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;

            if (num_puxrs) {
                p = le->u.mvcc.key_xrs + keylen;
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
print_leafentry (FILE *outf, LEAFENTRY le) {
    ULE_S ule;
    le_unpack(&ule, le);
    uint32_t i;
    invariant(ule.num_cuxrs > 0);
    UXR uxr;
    if (!le) { printf("NULL"); return 0; }
    fprintf(outf, "{key=");
    toku_print_BYTESTRING(outf, ule.keylen, ule.keyp);
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
            toku_print_BYTESTRING(outf, uxr->vallen, uxr->valp);
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
ule_init_empty_ule(ULE ule, u_int32_t keylen, void * keyp) {
    ule->num_cuxrs = 1;
    ule->num_puxrs = 0;
    ule->keylen    = keylen;
    ule->keyp      = keyp;
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
        invariant(num_xids>0); // TODO: If loader/2440 become MVCC happy (instead of 'errors'/etc) we may need to support committed messages.
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
        ule_push_delete_uxr(ule, TRUE, old_outermost_uncommitted_uxr->xid);
    }
    else {
        ule_push_insert_uxr(ule, TRUE,
                            old_outermost_uncommitted_uxr->xid,
                            old_innermost_uxr->vallen,
                            old_innermost_uxr->valp);
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
        ule_push_delete_uxr(ule, FALSE, new_innermost_xid);
    }
    else {
        ule_push_insert_uxr(ule, FALSE,
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
ule_apply_insert(ULE ule, XIDS xids, u_int32_t vallen, void * valp) {
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
    if (ule_get_innermost_xid(ule) == this_xid)
        ule_remove_innermost_uxr(ule);
    else
        ule_add_placeholders(ule, xids);
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
    if (innermost->xid == this_xid) {
        invariant(ule->num_puxrs>0);
        ule_remove_innermost_uxr(ule);                    
        ule_remove_innermost_placeholders(ule); 
    }
    invariant(ule->num_cuxrs > 0);
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
    if (ule_get_innermost_xid(ule) == this_xid) {
        //3 cases:
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
ule_push_insert_uxr(ULE ule, BOOL is_committed, TXNID xid, u_int32_t vallen, void * valp) {
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
ule_push_delete_uxr(ULE ule, BOOL is_committed, TXNID xid) {
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

static uint32_t
outermost_xid_not_in_ule(ULE ule, XIDS xids) {
    int index = 0;
    invariant(ule->num_puxrs < xids_get_num_xids(xids));
    if (ule->num_puxrs) {
        TXNID ule_xid  = ule_get_innermost_xid(ule); // xid of ica
        index = xids_find_index_of_xid(xids, ule_xid) + 1;
    }
    return index;
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
    TXNID ica_xid  = ule_get_innermost_xid(ule); // xid of ica
    TXNID this_xid = xids_get_innermost_xid(xids); // xid of this transaction
    invariant(this_xid!=TXNID_NONE);
    if (ica_xid != this_xid) {		// if this transaction is the ICA, don't push any placeholders
        int index = outermost_xid_not_in_ule(ule, xids);
	TXNID    current_msg_xid = xids_get_xid(xids, index);
	while (current_msg_xid != this_xid) { // Placeholder for each transaction before this transaction
	    ule_push_placeholder_uxr(ule, current_msg_xid);
	    index++;
	    current_msg_xid = xids_get_xid(xids, index);
	}
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

void *
ule_get_key(ULE ule) {
    return ule->keyp;
}

uint32_t 
ule_get_keylen(ULE ule) {
    return ule->keylen;
}

/////////////////////////////////////////////////////////////////////////////////
//  This layer of abstraction (uxr_xxx) understands uxr and nothing else.
//

static inline BOOL
uxr_type_is_insert(u_int8_t type) {
    BOOL rval = (BOOL)(type == XR_INSERT);
    return rval;
}

BOOL
uxr_is_insert(UXR uxr) {
    return uxr_type_is_insert(uxr->type);
}

static inline BOOL
uxr_type_is_delete(u_int8_t type) {
    BOOL rval = (BOOL)(type == XR_DELETE);
    return rval;
}

BOOL
uxr_is_delete(UXR uxr) {
    return uxr_type_is_delete(uxr->type);
}

static inline BOOL
uxr_type_is_placeholder(u_int8_t type) {
    BOOL rval = (BOOL)(type == XR_PLACEHOLDER);
    return rval;
}

BOOL
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
// associated with the accepted TXNID is not an insert, then set *is_emptyp to TRUE, otherwise FALSE
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
le_iterate_is_del(LEAFENTRY le, LE_ITERATE_CALLBACK f, BOOL *is_delp, TOKUTXN context) {
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
#endif

    //Read the keylen
    uint8_t type = le->type;
    int r;
    BOOL is_del = FALSE;
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
            uint32_t keylen = toku_dtoh32(le->keylen);
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;
            uint8_t *p = le->u.mvcc.key_xrs + keylen;

            uint32_t index;
            uint32_t num_interesting = num_cuxrs + (num_puxrs != 0);
            TXNID *xids = (TXNID*)p;
#if ULE_DEBUG
            ule_verify_xids(&ule, num_interesting, xids);
#endif
            r = le_iterate_get_accepted_index(xids, &index, num_interesting, f, context);
            if (r!=0) goto cleanup;
            invariant(index < num_interesting);

            //Skip TXNIDs
            p += (num_interesting - 1)*sizeof(TXNID);

            uint32_t *length_and_bits  = (uint32_t*)p;
            uint32_t my_length_and_bit = toku_dtoh32(length_and_bits[index]);
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
            invariant(FALSE);
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
le_iterate_val(LEAFENTRY le, LE_ITERATE_CALLBACK f, void** valpp, u_int32_t *vallenp, TOKUTXN context) {
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
#endif

    //Read the keylen
    uint32_t keylen = toku_dtoh32(le->keylen);
    uint8_t type = le->type;
    int r;
    uint32_t vallen = 0;
    void *valp = NULL;
    switch (type) {
        case LE_CLEAN: {
            vallen = toku_dtoh32(le->u.clean.vallen);
            valp   = le->u.clean.key_val + keylen;
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
            uint32_t num_cuxrs = toku_dtoh32(le->u.mvcc.num_cxrs);
            uint32_t num_puxrs = le->u.mvcc.num_pxrs;
            uint8_t *p = le->u.mvcc.key_xrs + keylen;

            uint32_t index;
            uint32_t num_interesting = num_cuxrs + (num_puxrs != 0);
            TXNID *xids = (TXNID*)p;
#if ULE_DEBUG
            ule_verify_xids(&ule, num_interesting, xids);
#endif
            r = le_iterate_get_accepted_index(xids, &index, num_interesting, f, context);
            if (r!=0) goto cleanup;
            invariant(index < num_interesting);

            //Skip TXNIDs
            p += (num_interesting - 1)*sizeof(TXNID);

            UXR_S temp;
            size_t offset = 0;

            uint32_t *length_and_bits  = (uint32_t*)p;
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
            invariant(FALSE);
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

void
le_clean(uint8_t *key, uint32_t keylen,
         uint8_t *val, uint32_t vallen,
         void (*bytes)(struct dbuf *dbuf, const void *bytes, int nbytes),
         struct dbuf *d) {
    struct leafentry le = {
        .type = LE_CLEAN,
        .keylen     = toku_htod32(keylen),
        .u.clean = {
            vallen = toku_htod32(vallen)
        }
    };
    size_t header_size = __builtin_offsetof(struct leafentry, u.clean) + sizeof(le.u.clean);
    invariant(header_size==1+4+4);
    bytes(d, &le, header_size); //Fixed
    bytes(d, key, keylen); //key
    bytes(d, val, vallen); //val
}

void
le_committed_mvcc(uint8_t *key, uint32_t keylen,
                  uint8_t *val, uint32_t vallen,
                  TXNID xid,
                  void (*bytes)(struct dbuf *dbuf, const void *bytes, int nbytes),
                  struct dbuf *d) {
    struct leafentry le = {
        .type = LE_MVCC,
        .keylen     = toku_htod32(keylen),
        .u.mvcc = {
            .num_cxrs = toku_htod32(2), //TXNID_NONE and xid each have committed xrs
            .num_pxrs = 0               //No provisional
        }
    };
    size_t header_size = __builtin_offsetof(struct leafentry, u.mvcc) + sizeof(le.u.mvcc);
    invariant(header_size==1+4+4+1);
    bytes(d, &le, header_size); //Fixed
    bytes(d, key, keylen); //key
    invariant(xid!=TXNID_NONE);
    xid = toku_htod64(xid);
    bytes(d, &xid, 8); //xid of transaction
                       //TXNID_NONE is implicit
    uint32_t insert_length_and_bit = toku_htod32(INSERT_LENGTH(vallen));
    bytes(d, &insert_length_and_bit, 4); //vallen insert
    uint32_t delete_length_and_bit = toku_htod32(DELETE_LENGTH(0));
    bytes(d, &delete_length_and_bit, 4); //committed delete
    bytes(d, val, vallen); //val
}


#ifdef IMPLICIT_PROMOTION_ON_QUERY


#warning be careful about full promotion function
////////////////////////////////////////////////////////////////////////////////
// Functions here are responsible for implicit promotions on queries.
// 
// Purpose is to promote any transactions in this leafentry by detecting if 
// transactions that have modified it have been committed.
// During a query, the read lock for the leaf entry is not necessarily taken.
// (We use a locking regime that tests the lock after the read.)
// If a transaction unrelated to the transaction issuing the query is writing 
// to this leafentry (possible because we didn't take the read lock), then that 
// unrelated transaction is alive and there should be no implicit promotion.
// So any implicit promotions done during the query must be based solely on 
// whether the transactions whose xids are recorded in the leafentry are still
// open.  (An open transaction is one that has not committed or aborted.)
// Our logic is:
// If the innermost transaction in the leafentry is definitely open, then no 
// implicit promotions are necessary (or possible).  This is a fast test.
// Otherwise, scan from inner to outer to find the innermost uncommitted
// transaction.  Then promote the innermost transaction to the transaction
// record of the innermost open (uncommitted) transaction.
// Transaction id of zero is always considered open for this purpose.
leafentry do_implicit_promotions_on_query(le) {
    innermost_xid = le_get_innermost_xid(le);
    // if innermost transaction still open, nothing to promote
    if (!transaction_open(innermost_xid)) {
        ule = unpack(le);
        // scan outward starting with next outer transaction
        for (index = ule->num_uxrs - 2; index > 0; index--) {
            xid = ule_get_xid(ule, index);
            if (transaction_open(xid)) break;
        }
        promote_innermost_to_index(ule, index);
        le = le_pack(ule);
    }
    return le;
}


// Examine list of open transactions, return true if transaction is still open.
// Transaction zero is always open.
//
// NOTE: Old code already does implicit promotion of provdel on query,
//       and that code uses some equivalent of transaction_open().
//

bool transaction_open(TXNID xid) {
    rval = TRUE;
    if (xid != 0) {
        //TODO: Logic
    }
    return rval;
}

#endif

#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif
struct __attribute__ ((__packed__)) leafentry_12 {
    u_int8_t  num_xrs;
    u_int32_t keylen;
    u_int32_t innermost_inserted_vallen;
    union {
        struct __attribute__ ((__packed__)) leafentry_committed_12 {
            u_int8_t key_val[0];     //Actual key, then actual val
        } comm;
        struct __attribute__ ((__packed__)) leafentry_provisional_12 {
            u_int8_t innermost_type;
            TXNID    xid_outermost_uncommitted;
            u_int8_t key_val_xrs[];  //Actual key,
                                     //then actual innermost inserted val,
                                     //then transaction records.
        } prov;
    } u;
};
#if TOKU_WINDOWS
#pragma pack(pop)
#endif

//Requires:
//  Leafentry that ule represents should not be destroyed (is not just all deletes)
static size_t
le_memsize_from_ule_12 (ULE ule) {
    uint32_t num_uxrs = ule->num_cuxrs + ule->num_puxrs;
    assert(num_uxrs);
    size_t rval;
    if (num_uxrs == 1) {
        assert(uxr_is_insert(&ule->uxrs[0]));
        rval = 1                    //num_uxrs
              +4                    //keylen
              +4                    //vallen
              +ule->keylen          //actual key
              +ule->uxrs[0].vallen; //actual val
    }
    else {
        rval = 1                    //num_uxrs
              +4                    //keylen
              +ule->keylen          //actual key
              +1*num_uxrs      //types
              +8*(num_uxrs-1); //txnids
        u_int8_t i;
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

//This function is mostly copied from 4.1.1
// Note, number of transaction records in version 12 has been replaced by separate counters in version 13 (MVCC),
// one counter for committed transaction records and one counter for provisional transaction records.  When 
// upgrading a version 12 le to version 13, the number of committed transaction records is always set to one (1)
// and the number of provisional transaction records is set to the original number of transaction records 
// minus one.  The bottom transaction record is assumed to be a committed value.  (If there is no committed
// value then the bottom transaction record of version 12 is a committed delete.)
// This is the only change from the 4.1.1 code.  The rest of the leafentry is read as is.
static void
le_unpack_12(ULE ule, LEAFENTRY_12 le) {
    //Read num_uxrs
    uint8_t num_xrs = le->num_xrs;
    assert(num_xrs > 0);
    ule->uxrs = ule->uxrs_static; //Static version is always enough.
    ule->num_cuxrs = 1;
    ule->num_puxrs = num_xrs - 1;

    //Read the keylen
    ule->keylen = toku_dtoh32(le->keylen);

    //Read the vallen of innermost insert
    u_int32_t vallen_of_innermost_insert = toku_dtoh32(le->innermost_inserted_vallen);

    u_int8_t *p;
    if (num_xrs == 1) {
        //Unpack a 'committed leafentry' (No uncommitted transactions exist)
        ule->keyp           = le->u.comm.key_val;
        ule->uxrs[0].type   = XR_INSERT; //Must be or the leafentry would not exist
        ule->uxrs[0].vallen = vallen_of_innermost_insert;
        ule->uxrs[0].valp   = &le->u.comm.key_val[ule->keylen];
        ule->uxrs[0].xid    = 0;          //Required.

        //Set p to immediately after leafentry
        p = &le->u.comm.key_val[ule->keylen + vallen_of_innermost_insert];
    }
    else {
        //Unpack a 'provisional leafentry' (Uncommitted transactions exist)

        //Read in type.
        u_int8_t innermost_type = le->u.prov.innermost_type;
        assert(!uxr_type_is_placeholder(innermost_type));

        //Read in xid
        TXNID xid_outermost_uncommitted = toku_dtoh64(le->u.prov.xid_outermost_uncommitted);

        //Read pointer to key
        ule->keyp = le->u.prov.key_val_xrs;

        //Read pointer to innermost inserted val (immediately after key)
        u_int8_t *valp_of_innermost_insert = &le->u.prov.key_val_xrs[ule->keylen];

        //Point p to immediately after 'header'
        p = &le->u.prov.key_val_xrs[ule->keylen + vallen_of_innermost_insert];

        BOOL found_innermost_insert = FALSE;
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
                    uxr->vallen = toku_dtoh32(*(u_int32_t*)p);
                    p += 4;

                    uxr->valp = p;
                    p += uxr->vallen;
                }
                else {
                    //Innermost insert, load the vallen/valp previously read from header
                    uxr->vallen = vallen_of_innermost_insert;
                    uxr->valp   = valp_of_innermost_insert;
                    found_innermost_insert = TRUE;
                }
            }
        }
        assert(found_innermost_insert);
    }
#if ULE_DEBUG
    size_t memsize = le_memsize_from_ule_12(ule);
    assert(p == ((u_int8_t*)le) + memsize);
#endif
}

size_t
leafentry_disksize_12(LEAFENTRY_12 le) {
    ULE_S ule;
    le_unpack_12(&ule, le);
    size_t memsize = le_memsize_from_ule_12(&ule);
    ule_cleanup(&ule);
    return memsize;
}

int 
toku_le_upgrade_12_13(LEAFENTRY_12 old_leafentry,
		      size_t *new_leafentry_memorysize, 
		      size_t *new_leafentry_disksize, 
		      LEAFENTRY *new_leafentry_p) {
    ULE_S ule;
    int rval;
    invariant(old_leafentry);
    le_unpack_12(&ule, old_leafentry);
    rval = le_pack(&ule,                // create packed leafentry
                   new_leafentry_memorysize, 
                   new_leafentry_disksize, 
                   new_leafentry_p,
                   NULL, NULL, NULL); //NULL for omt means that we use malloc instead of mempool
    ule_cleanup(&ule);
    return rval;
}

