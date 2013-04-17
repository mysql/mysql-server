/* -*- mode: C; c-basic-offset: 4 -*- */

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

//This is what we use to initialize uxrs[0] in a new unpacked leafentry.
const UXR_S committed_delete = {
    .type   = XR_DELETE,
    .vallen = 0,
    .xid    = 0,
    .valp   = NULL
};  // static allocation of uxr with type set to committed delete and xid = 0

// Local functions:

static void msg_init_empty_ule(ULE ule, BRT_MSG msg);
static void msg_modify_ule(ULE ule, BRT_MSG msg);
static void ule_init_empty_ule(ULE ule, u_int32_t keylen, void * keyp);
static void ule_do_implicit_promotions(ULE ule, XIDS xids);
static void ule_promote_innermost_to_index(ULE ule, u_int8_t index);
static void ule_apply_insert(ULE ule, XIDS xids, u_int32_t vallen, void * valp);
static void ule_apply_delete(ULE ule, XIDS xids);
static void ule_prepare_for_new_uxr(ULE ule, XIDS xids);
static void ule_apply_abort(ULE ule, XIDS xids);
static void ule_apply_commit(ULE ule, XIDS xids);
static void ule_push_insert_uxr(ULE ule, TXNID xid, u_int32_t vallen, void * valp);
static void ule_push_delete_uxr(ULE ule, TXNID xid);
static void ule_push_placeholder_uxr(ULE ule, TXNID xid);
static UXR ule_get_outermost_uxr(ULE ule);
static UXR ule_get_innermost_uxr(ULE ule);
static UXR ule_get_first_empty_uxr(ULE ule);
static void ule_remove_innermost_uxr(ULE ule);
static TXNID ule_get_innermost_xid(ULE ule);
static TXNID ule_get_xid(ULE ule, u_int8_t index);
static void ule_remove_innermost_placeholders(ULE ule);
static void ule_add_placeholders(ULE ule, XIDS xids);
static inline BOOL uxr_type_is_insert(u_int8_t type);
static inline BOOL uxr_type_is_delete(u_int8_t type);
static inline BOOL uxr_type_is_placeholder(u_int8_t type);
static inline BOOL uxr_is_insert(UXR uxr);
static inline BOOL uxr_is_delete(UXR uxr);
static inline BOOL uxr_is_placeholder(UXR uxr);


static void *
le_malloc(OMT omt, struct mempool *mp, size_t size, void **maybe_free)
{
    if (omt)
	return mempool_malloc_from_omt(omt, mp, size, maybe_free);
    else
	return toku_malloc(size);
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
		       void **maybe_free) {
    ULE_S ule;
    int rval;

    if (old_leafentry == NULL)           // if leafentry does not exist ...
        msg_init_empty_ule(&ule, msg);   // ... create empty unpacked leaf entry
    else 
        le_unpack(&ule, old_leafentry); // otherwise unpack leafentry 
    msg_modify_ule(&ule, msg);          // modify unpacked leafentry
    rval = le_pack(&ule,                // create packed leafentry
		   new_leafentry_memorysize, 
		   new_leafentry_disksize, 
		   new_leafentry_p,
		   omt, mp, maybe_free);                       
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
    assert(xids_get_num_xids(xids) <= MAX_TRANSACTION_RECORDS);
    ule_do_implicit_promotions(ule, xids);
    enum brt_msg_type type = brt_msg_get_type(msg);
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
	void * valp      = brt_msg_get_val(msg);
        ule_apply_insert(ule, xids, vallen, valp);
        break;
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
        ule_apply_delete(ule, xids);
        break;
    case BRT_ABORT_ANY:
    case BRT_ABORT_BOTH:
    case BRT_ABORT_BROADCAST_TXN:
        ule_apply_abort(ule, xids);
        break;
    case BRT_COMMIT_ANY:
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_BROADCAST_TXN:
        ule_apply_commit(ule, xids);
        break;
    default:
	assert(FALSE /* illegal BRT_MSG.type */);
	break;
    }
}


/////////////////////////////////////////////////////////////////////////////////
// This layer of abstraction (le_xxx) understands the structure of the leafentry
// and of the unpacked leafentry.  It is the only layer that understands the
// structure of leafentry.  It has no knowledge of any other data structures.
//
// There are two formats for a packed leaf entry, indicated by the number of 
// transaction records:
// 
// No uncommitted transactions: 
//  num = 1  (one byte)
//  keylen   (4 bytes)
//  vallen   (4 bytes)
//  key      (keylen bytes)
//  val      (vallen bytes)
// 
// At least one uncommitted transaction (maybe a committed value as well):
// 
//  num > 1
//  keylen
//  vallen of innermost insert
//  type of innermost transaction record
//  xid of outermost uncommitted transaction 
//  key
//  val of innermost insert
//  records excluding extracted data above
//   first (innermost) record is missing the type (above)
//   innermost insert record is missing the vallen and val
//   outermost uncommitted record is missing xid
//   outermost record (always committed) is missing xid (implied 0)
//    default record:
//      type = XR_INSERT  or  type = XR_PLACEHOLDER or XR_DELETE
//      xid                   xid
//      vallen
//      val
//     
//

#if 0
#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif
//TODO: #1125 Add tests to verify ALL offsets (to verify we used 'pack' right).
//            May need to add extra __attribute__((__packed__)) attributes within the definition
struct __attribute__ ((__packed__)) leafentry {
    u_int8_t  num_xrs;
    u_int32_t keylen;
    u_int32_t innermost_inserted_vallen;
    union {
        struct leafentry_committed {
            u_int8_t key_val[0];     //Actual key, then actual val
        } comm;
        struct leafentry_provisional {
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
#endif


// Purpose of le_unpack() is to populate our private workspace with the contents of the given le.
void
le_unpack(ULE ule, LEAFENTRY le) {
    //Read num_uxrs
    ule->num_uxrs = le->num_xrs;
    assert(ule->num_uxrs > 0);

    //Read the keylen
    ule->keylen = toku_dtoh32(le->keylen);

    //Read the vallen of innermost insert
    u_int32_t vallen_of_innermost_insert = toku_dtoh32(le->innermost_inserted_vallen);

    u_int8_t *p;
    if (ule->num_uxrs == 1) {
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
        for (i = ule->num_uxrs - 1; i >= 0; i--) {
            UXR uxr = &ule->uxrs[i];

            //Innermost's type is in header.
            if (i < ule->num_uxrs - 1) {
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
    size_t memsize = le_memsize_from_ule(ule);
    assert(p == ((u_int8_t*)le) + memsize);
#endif
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
    int rval;
    u_int8_t index_of_innermost_insert;
    void     *valp_innermost_insert = NULL;
    u_int32_t vallen_innermost_insert;
    {
        //If there are no 'insert' entries, return NO leafentry.
        //Loop inner to outer searching for innermost insert.
        //uxrs[0] is outermost (committed)
        int i;
        for (i = ule->num_uxrs - 1; i >= 0; i--) {
            if (uxr_is_insert(&ule->uxrs[i])) {
                index_of_innermost_insert = (u_int8_t) i;
                vallen_innermost_insert   = ule->uxrs[i].vallen;
                valp_innermost_insert     = ule->uxrs[i].valp;
                goto found_insert;
            }
        }
        *new_leafentry_p = NULL;
        rval = 0;
        goto cleanup;
    }
found_insert:;
    size_t memsize = le_memsize_from_ule(ule);
    LEAFENTRY new_leafentry = le_malloc(omt, mp, memsize, maybe_free);
    if (new_leafentry==NULL) {
        rval = ENOMEM;
        goto cleanup;
    }
    //Universal data
    new_leafentry->num_xrs = ule->num_uxrs;
    new_leafentry->keylen  = toku_htod32(ule->keylen);
    new_leafentry->innermost_inserted_vallen  = toku_htod32(vallen_innermost_insert);

    u_int8_t *p;
    //Type (committed/provisional) specific data
    if (ule->num_uxrs == 1) {
        //Pack a 'committed leafentry' (No uncommitted transactions exist)

        //Store actual key.
        memcpy(new_leafentry->u.comm.key_val, ule->keyp, ule->keylen);

        //Store actual val of innermost insert immediately after actual key
        memcpy(&new_leafentry->u.comm.key_val[ule->keylen],
               valp_innermost_insert,
               vallen_innermost_insert);

        //Set p to after leafentry
        p = &new_leafentry->u.comm.key_val[ule->keylen + vallen_innermost_insert];
    }
    else {
        //Pack a 'provisional leafentry' (Uncommitted transactions exist)
        //Store the type of the innermost transaction record
        new_leafentry->u.prov.innermost_type = ule_get_innermost_uxr(ule)->type;

        //uxrs[0] is the committed, uxrs[1] is the outermost non-committed
        //Store the outermost non-committed xid
        new_leafentry->u.prov.xid_outermost_uncommitted = toku_htod64(ule->uxrs[1].xid);

        //Store actual key.
        memcpy(new_leafentry->u.prov.key_val_xrs, ule->keyp, ule->keylen);

        //Store actual val of innermost insert immediately after actual key
        memcpy(&new_leafentry->u.prov.key_val_xrs[ule->keylen],
               valp_innermost_insert,
               vallen_innermost_insert);

        //Set p to after 'header'
        p = &new_leafentry->u.prov.key_val_xrs[ule->keylen + vallen_innermost_insert];

        int i;  //index into ULE
        //Loop inner to outer
        for (i = ule->num_uxrs - 1; i >= 0; i--) {
            UXR uxr = &ule->uxrs[i];

            //Innermost's type is in header.
            if (i < ule->num_uxrs - 1) {
                //Not innermost, so record the type.
                *p = uxr->type;
                p += 1;
            }

            //Committed txn id is implicit (0).  (i==0)
            //Outermost uncommitted txnid is stored in header. (i==1)
            if (i > 1) {
                //Not committed nor outermost uncommitted, so record the xid.
                *((TXNID*)p) = toku_htod64(uxr->xid);
                p += 8;
            }

            //Innermost insert's length and value are stored in header.
            if (uxr_is_insert(uxr) && i != index_of_innermost_insert) {
                //Is an insert, and not the innermost insert, so store length/val
                *((u_int32_t*)p) = toku_htod32(uxr->vallen);
                p += 4;

                memcpy(p, uxr->valp, uxr->vallen); //Store actual val
                p += uxr->vallen;
            }
        }
    }
    //p points to first unused byte after packed leafentry

    size_t bytes_written = (size_t)p - (size_t)new_leafentry;
    assert(bytes_written == memsize);
#if ULE_DEBUG
    if (omt) { //Disable recursive debugging.
        size_t memsize_verify = leafentry_memsize(new_leafentry);
        assert(memsize_verify == memsize);

        ULE_S ule_tmp;
        le_unpack(&ule_tmp, new_leafentry);

        memsize_verify = le_memsize_from_ule(&ule_tmp);
        assert(memsize_verify == memsize);
        //Debugging code inside le_unpack will repack and verify it is the same.

        LEAFENTRY le_copy;

        int r_tmp = le_pack(&ule_tmp, &memsize_verify, &memsize_verify,
                            &le_copy, NULL, NULL, NULL);
        assert(r_tmp==0);
        assert(memsize_verify == memsize);

        assert(memcmp(new_leafentry, le_copy, memsize)==0);
        toku_free(le_copy);
    }
#endif

    *new_leafentry_p = (LEAFENTRY)new_leafentry;
    *new_leafentry_memorysize = memsize;
    *new_leafentry_disksize   = memsize;
    rval = 0;
cleanup:
    return rval;
}

//////////////////////////////////////////////////////////////////////////////////
// Following functions provide convenient access to a packed leafentry.

//Requires:
//  Leafentry that ule represents should not be destroyed (is not just all deletes)
size_t
le_memsize_from_ule (ULE ule) {
    assert(ule->num_uxrs);
    size_t rval;
    if (ule->num_uxrs == 1) {
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
              +1*ule->num_uxrs      //types
              +8*(ule->num_uxrs-1); //txnids
        u_int8_t i;
        for (i = 0; i < ule->num_uxrs; i++) {
            UXR uxr = &ule->uxrs[i];
            if (uxr_is_insert(uxr)) {
                rval += 4;           //vallen
                rval += uxr->vallen; //actual val
            }
        }
    }
    return rval;
}

#define LE_COMMITTED_MEMSIZE(le, keylen, vallen)             \
    (sizeof((le)->num_xrs)                   /* num_uxrs */  \
    +sizeof((le)->keylen)                    /* keylen */    \
    +sizeof((le)->innermost_inserted_vallen) /* vallen */    \
    +keylen                                 /* actual key */ \
    +vallen)                                /* actual val */

size_t
leafentry_memsize (LEAFENTRY le) {
    size_t rval = 0;

    //Read num_uxrs
    u_int8_t num_uxrs = le->num_xrs;
    assert(num_uxrs > 0);

    //Read the keylen
    u_int32_t keylen = toku_dtoh32(le->keylen);

    //Read the vallen of innermost insert
    u_int32_t vallen_of_innermost_insert = toku_dtoh32(le->innermost_inserted_vallen);

    if (num_uxrs == 1) {
        //Committed version (no uncommitted records)
        rval = LE_COMMITTED_MEMSIZE(le, keylen, vallen_of_innermost_insert);
    }
    else {
        //A 'provisional leafentry' (Uncommitted transactions exist)
        //Read in type.
        u_int8_t innermost_type = le->u.prov.innermost_type;
        assert(!uxr_type_is_placeholder(innermost_type));
        //Set p to immediately after key,val (begginning of transaction records)
        u_int8_t *p = &le->u.prov.key_val_xrs[keylen + vallen_of_innermost_insert];

        BOOL found_innermost_insert = FALSE;
        int i; //would be index in ULE.uxrs[] were we to unpack
        //Loop inner to outer
        UXR_S current_uxr;
        UXR uxr = &current_uxr;
        for (i = num_uxrs - 1; i >= 0; i--) {
            //Innermost's type is in header.
            if (i < num_uxrs - 1) {
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
                p += 8;
            }

            if (uxr_is_insert(uxr)) {
                if (found_innermost_insert) {
                    //Not the innermost insert.  Load vallen/valp
                    uxr->vallen = toku_dtoh32(*(u_int32_t*)p);
                    p += 4;
                    p += uxr->vallen;
                }
                else
                    found_innermost_insert = TRUE;
            }
        }
        assert(found_innermost_insert);
        rval = (size_t)p - (size_t)le;
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    size_t slow_rval = le_memsize_from_ule(&ule);
    assert(slow_rval == rval);
#endif
    return rval;
}

size_t
leafentry_disksize (LEAFENTRY le) {
    return leafentry_memsize(le);
}


// le is normally immutable.  This is the only exception.
void
le_full_promotion(LEAFENTRY le,
                  size_t *new_leafentry_memorysize, 
                  size_t *new_leafentry_disksize) {
#if ULE_DEBUG
    // Create a new le ("slow_le") using normal commit message for comparison.
    // Creation of slow_le must be done first, because le is being modified.
    assert(le);
    assert(le->num_xrs > 1); //Not committed
    assert(!le_is_provdel(le));
    TXNID outermost_uncommitted_xid = le_outermost_uncommitted_xid(le);
    assert(outermost_uncommitted_xid != 0);

    size_t old_memsize = leafentry_memsize(le);
    u_int32_t old_keylen;
    u_int32_t old_vallen;
    void *old_key = le_key_and_len(le, &old_keylen);
    void *old_val = le_innermost_inserted_val_and_len(le, &old_vallen);

    assert(old_key    == le_latest_key(le));
    assert(old_keylen == le_latest_keylen(le));
    assert(old_val    == le_latest_val(le));
    assert(old_vallen == le_latest_vallen(le));

    //Save copies for verification.
    old_key = toku_memdup(old_key, old_keylen);
    assert(old_key);
    old_val = toku_memdup(old_val, old_vallen);
    assert(old_val);

    BRT_MSG_S slow_full_promotion_msg = {
        .type = BRT_COMMIT_ANY,
        .u.id = {
            .key = NULL,
            .val = NULL,
        }
    };
    int r_xids = xids_create_child(xids_get_root_xids(),
                                   &slow_full_promotion_msg.xids,
                                   outermost_uncommitted_xid);
    assert(r_xids==0);
    size_t slow_new_memsize;
    size_t slow_new_disksize;
    LEAFENTRY slow_le;
    int r_apply = apply_msg_to_leafentry(&slow_full_promotion_msg,
                                         le,
                                         &slow_new_memsize, &slow_new_disksize,
                                         &slow_le,
                                         NULL, NULL, NULL);
    assert(r_apply == 0);
    assert(slow_new_memsize == slow_new_disksize);
    assert(slow_new_memsize < old_memsize);
    assert(slow_le);
#endif

    //Save keylen for later use.
    u_int32_t keylen = le_keylen(le);
    //Save innermost inserted vallen for later use.
    u_int32_t vallen = le_innermost_inserted_vallen(le);

    //Set as committed.
    le->num_xrs = 1;

    //Keylen is unchanged but we need to extract it.
    //Innermost inserted vallen is unchanged but we need to extract it.

    //Move key and value using memmove. memcpy does not support overlapping memory.

    //Move the key
    memmove(le->u.comm.key_val,          le->u.prov.key_val_xrs, keylen);

    //Move the val
    memmove(&le->u.comm.key_val[keylen], &le->u.prov.key_val_xrs[keylen], vallen);

    size_t new_memsize = LE_COMMITTED_MEMSIZE(le, keylen, vallen);
    *new_leafentry_memorysize = new_memsize;
    *new_leafentry_disksize   = new_memsize;

#if ULE_DEBUG
    // now compare le with "slow_le" created via normal commit message.
    assert(*new_leafentry_memorysize == slow_new_memsize);  //Size same
    assert(*new_leafentry_disksize   == slow_new_disksize); //Size same
    assert(memcmp(le, slow_le, slow_new_memsize) == 0);     //Bitwise the same.
    assert(!le_is_provdel(le));
    assert(le_outermost_uncommitted_xid(le) == 0);

    //Verify key(len), val(len) unchanged.
    u_int32_t new_keylen;
    u_int32_t new_vallen;
    void *new_key = le_key_and_len(le, &new_keylen);
    void *new_val = le_innermost_inserted_val_and_len(le, &new_vallen);
    assert(new_key    == le_latest_key(le));
    assert(new_keylen == le_latest_keylen(le));
    assert(new_val    == le_latest_val(le));
    assert(new_vallen == le_latest_vallen(le));

    assert(new_keylen == old_keylen);
    assert(new_vallen == old_vallen);
    assert(memcmp(new_key, old_key, old_keylen) == 0);
    assert(memcmp(new_val, old_val, old_vallen) == 0);

    xids_destroy(&slow_full_promotion_msg.xids);
    toku_free(slow_le);
    toku_free(old_key);
    toku_free(old_val);
#endif
}

int le_outermost_is_del(LEAFENTRY le) {
    ULE_S ule;
    le_unpack(&ule, le);
    UXR outermost_uxr = ule_get_outermost_uxr(&ule);
    int rval = uxr_is_delete(outermost_uxr);
    return rval;
}

int le_is_provdel(LEAFENTRY le) {
    int rval;
    u_int8_t num_xrs = le->num_xrs;
    if (num_xrs == 1)
        rval = 0;
    else
        rval = uxr_type_is_delete(le->u.prov.innermost_type);
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    int slow_rval = uxr_is_delete(uxr);
    assert((rval==0) == (slow_rval==0));
#endif
    return rval;
}

int
le_has_xids(LEAFENTRY le, XIDS xids) {
    int rval=0;

    //Read num_uxrs
    u_int8_t num_uxrs = le->num_xrs;
    assert(num_uxrs > 0);
    u_int8_t num_xids = xids_get_num_xids(xids);
    assert(num_xids > 1); //Disallow checking for having 'root txn'

    if (num_xids > num_uxrs) {
        //Not enough transaction records in le to have all of xids
        rval = 0;
        goto have_answer;
    }
    if (le_outermost_uncommitted_xid(le) != xids_get_xid(xids, 1)) {
        rval = 0;
        goto have_answer;
    }
    if (num_xids == 2) {
        //Outermost uncommitted xid is the only xid (other than 0).  We're done.
        rval = 1;
        goto have_answer;
    }
    //Hard case:  shares outermost uncommitted xid, but has more in the stack.
    //  Need to unpack iteratively till we reach the right xid.
    //Read the keylen
    u_int32_t keylen = toku_dtoh32(le->keylen);

    //Read the vallen of innermost insert
    u_int32_t vallen_of_innermost_insert = toku_dtoh32(le->innermost_inserted_vallen);

    assert(num_uxrs > 1);
    //A 'provisional leafentry' (Uncommitted transactions exist)
    //Read in type.
    u_int8_t innermost_type = le->u.prov.innermost_type;
    assert(!uxr_type_is_placeholder(innermost_type));
    //Set p to immediately after key,val (begginning of transaction records)
    u_int8_t *p = &le->u.prov.key_val_xrs[keylen + vallen_of_innermost_insert];

    BOOL found_innermost_insert = FALSE;
    u_int8_t i; //would be index in ULE.uxrs[] were we to unpack
    //Loop inner to outer
    UXR_S current_uxr;
    UXR uxr = &current_uxr;
    for (i = num_uxrs - 1; i >= num_xids-1; i--) {
        //Innermost's type is in header.
        if (i < num_uxrs - 1) {
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
        //Not committed nor outermost uncommitted, so load the xid.
        if (i == num_xids-1) {
            //Done.  This is the interesting txn.
            TXNID candidate_txn = toku_dtoh64(*(TXNID*)p);
            TXNID target_txn    = xids_get_innermost_xid(xids);
            rval = candidate_txn == target_txn;
            goto have_answer;
        }
        p += 8;

        if (uxr_is_insert(uxr)) {
            if (found_innermost_insert) {
                //Not the innermost insert.  Load vallen/valp
                uxr->vallen = toku_dtoh32(*(u_int32_t*)p);
                p += 4;
                p += uxr->vallen;
            }
            else
                found_innermost_insert = TRUE;
        }
    }
    assert(FALSE);
have_answer:
#if ULE_DEBUG
    {
        u_int32_t num_xids_slow = xids_get_num_xids(xids);
        int slow_rval = 0;
        ULE_S ule_slow;
        le_unpack(&ule_slow, le);
        if (num_xids_slow > 1 && ule_slow.num_uxrs >= num_xids_slow) {
            u_int32_t idx_slow;
            for (idx_slow = 0; idx_slow < num_xids_slow; idx_slow++) {
                if (xids_get_xid(xids, idx_slow) != ule_get_xid(&ule_slow, idx_slow))
                    break;
            }
            if (idx_slow == num_xids_slow)
                slow_rval = 1;
        }
        assert(slow_rval == rval);
    }
#endif
    return rval;
}

void*
le_outermost_key_and_len (LEAFENTRY le, u_int32_t *len) {
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_outermost_uxr(&ule);
    void     *slow_keyp;
    u_int32_t slow_len;
    if (uxr_is_insert(uxr)) {
        slow_keyp = ule.keyp;
        slow_len  = ule.keylen; 
    }
    else {
        slow_keyp = NULL;
        slow_len  = 0;
    }
    *len = slow_len;
    return slow_keyp;
}

//If le_is_provdel, return (NULL,0)
//Else,             return (key,keylen)
void*
le_latest_key_and_len (LEAFENTRY le, u_int32_t *len) {
    u_int8_t num_xrs = le->num_xrs;
    void *keyp;
    *len = toku_dtoh32(le->keylen);
    if (num_xrs == 1)
        keyp = le->u.comm.key_val;
    else {
        keyp = le->u.prov.key_val_xrs;
        if (uxr_type_is_delete(le->u.prov.innermost_type)) {
            keyp = NULL;
            *len = 0;
        }
    }
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    void     *slow_keyp;
    u_int32_t slow_len;
    if (uxr_is_insert(uxr)) {
        slow_keyp = ule.keyp;
        slow_len  = ule.keylen; 
    }
    else {
        slow_keyp = NULL;
        slow_len  = 0;
    }
    assert(slow_keyp == le_latest_key(le));
    assert(slow_len  == le_latest_keylen(le));
    assert(keyp==slow_keyp);
    assert(*len==slow_len);
#endif
    return keyp;
}

void*
le_latest_key (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    void *rval;
    if (num_xrs == 1)
        rval = le->u.comm.key_val;
    else {
        rval = le->u.prov.key_val_xrs;
        if (uxr_type_is_delete(le->u.prov.innermost_type))
            rval = NULL;
    }
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    void *slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = ule.keyp;
    else
        slow_rval = NULL;
    assert(rval==slow_rval);
#endif
    return rval;
}

u_int32_t
le_latest_keylen (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    u_int32_t rval = toku_dtoh32(le->keylen);
    if (num_xrs > 1 && uxr_type_is_delete(le->u.prov.innermost_type))
        rval = 0;
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    u_int32_t slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = ule.keylen;
    else
        slow_rval = 0;
    assert(rval==slow_rval);
#endif
    return rval;
}

void*
le_outermost_val_and_len (LEAFENTRY le, u_int32_t *len) {
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_outermost_uxr(&ule);
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
    *len = slow_len;
    return slow_valp;
}

void*
le_latest_val_and_len (LEAFENTRY le, u_int32_t *len) {
    u_int8_t num_xrs = le->num_xrs;
    void *valp;
    u_int32_t keylen = toku_dtoh32(le->keylen);
    *len = toku_dtoh32(le->innermost_inserted_vallen);
    if (num_xrs == 1)
        valp = &le->u.comm.key_val[keylen];
    else {
        valp = &le->u.prov.key_val_xrs[keylen];
        if (uxr_type_is_delete(le->u.prov.innermost_type)) {
            valp = NULL;
            *len = 0;
        }
    }
#if ULE_DEBUG
    assert(num_xrs);
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
#endif
    return valp;
}

void*
le_latest_val (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    void *rval;
    u_int32_t keylen = toku_dtoh32(le->keylen);
    if (num_xrs == 1)
        rval = &le->u.comm.key_val[keylen];
    else {
        rval = &le->u.prov.key_val_xrs[keylen];
        if (uxr_type_is_delete(le->u.prov.innermost_type))
            rval = NULL;
    }
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    void *slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = uxr->valp;
    else
        slow_rval = NULL;
    assert(rval==slow_rval);
#endif
    return rval;
}

u_int32_t
le_latest_vallen (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    u_int32_t rval   = toku_dtoh32(le->innermost_inserted_vallen);
    if (num_xrs > 1 && uxr_type_is_delete(le->u.prov.innermost_type))
        rval = 0;
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    UXR uxr = ule_get_innermost_uxr(&ule);
    u_int32_t slow_rval;
    if (uxr_is_insert(uxr))
        slow_rval = uxr->vallen;
    else
        slow_rval = 0;
    assert(rval==slow_rval);
#endif
    return rval;
}

//Return key and keylen unconditionally
void*
le_key_and_len (LEAFENTRY le, u_int32_t *len) {
    u_int8_t num_xrs = le->num_xrs;
    *len = toku_dtoh32(le->keylen);
    void *keyp;
    if (num_xrs == 1)
        keyp = le->u.comm.key_val;
    else
        keyp = le->u.prov.key_val_xrs;
#if ULE_DEBUG
    assert(num_xrs);
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
#endif
    return keyp;
}


void*
le_key (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    void *rval;
    if (num_xrs == 1)
        rval = le->u.comm.key_val;
    else
        rval = le->u.prov.key_val_xrs;
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    void *slow_rval = ule.keyp;
    assert(rval==slow_rval);
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
#endif
    return rval;
}

void*
le_innermost_inserted_val_and_len (LEAFENTRY le, u_int32_t *len) {
    u_int8_t num_xrs = le->num_xrs;
    void *valp;
    u_int32_t keylen = toku_dtoh32(le->keylen);
    *len = toku_dtoh32(le->innermost_inserted_vallen);
    if (num_xrs == 1)
        valp = &le->u.comm.key_val[keylen];
    else
        valp = &le->u.prov.key_val_xrs[keylen];
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    u_int8_t i;
    for (i = ule.num_uxrs; i > 0; i--) {
        if (uxr_is_insert(&ule.uxrs[i-1]))
            break;
    }
    assert(i > 0);
    i--;
    UXR uxr = &ule.uxrs[i];
    void     *slow_valp;
    u_int32_t slow_len;
    slow_valp = uxr->valp;
    slow_len  = uxr->vallen; 
    assert(slow_valp == le_innermost_inserted_val(le));
    assert(slow_len == le_innermost_inserted_vallen(le));
    assert(valp==slow_valp);
    assert(*len==slow_len);
#endif
    return valp;
}


void*
le_innermost_inserted_val (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    void *rval;
    u_int32_t keylen = toku_dtoh32(le->keylen);
    if (num_xrs == 1)
        rval = &le->u.comm.key_val[keylen];
    else
        rval = &le->u.prov.key_val_xrs[keylen];
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    u_int8_t i;
    for (i = ule.num_uxrs; i > 0; i--) {
        if (uxr_is_insert(&ule.uxrs[i-1]))
            break;
    }
    assert(i > 0);
    i--;
    void *slow_rval = ule.uxrs[i].valp;
    assert(rval==slow_rval);
#endif
    return rval;
}

u_int32_t
le_innermost_inserted_vallen (LEAFENTRY le) {
    u_int32_t rval = toku_dtoh32(le->innermost_inserted_vallen);
#if ULE_DEBUG
    ULE_S ule;
    le_unpack(&ule, le);
    u_int8_t i;
    for (i = ule.num_uxrs; i > 0; i--) {
        if (uxr_is_insert(&ule.uxrs[i-1]))
            break;
    }
    assert(i > 0);
    i--;
    u_int32_t slow_rval = ule.uxrs[i].vallen;
    assert(rval==slow_rval);
#endif
    return rval;
}

u_int64_t 
le_outermost_uncommitted_xid (LEAFENTRY le) {
    u_int8_t num_xrs = le->num_xrs;
    TXNID rval;
    if (num_xrs == 1)
        rval = 0;
    else
        rval = toku_dtoh64(le->u.prov.xid_outermost_uncommitted);
#if ULE_DEBUG
    assert(num_xrs);
    ULE_S ule;
    le_unpack(&ule, le);
    TXNID slow_rval = 0;
    if (ule.num_uxrs > 1)
        slow_rval = ule.uxrs[1].xid;
    assert(rval==slow_rval);
#endif
    return rval;
}


//Optimization not required.  This is a debug only function.
//Print a leafentry out in human-readable format
int
print_leafentry (FILE *outf, LEAFENTRY le) {
    ULE_S ule;
    le_unpack(&ule, le);
    u_int8_t i;
    assert(ule.num_uxrs > 0);
    UXR uxr = &ule.uxrs[0];
    if (!le) { printf("NULL"); return 0; }
    fprintf(outf, "{key=");
    toku_print_BYTESTRING(outf, ule.keylen, ule.keyp);
    for (i = 0; i < ule.num_uxrs; i++) {
        fprintf(outf, "\n%*s", i+1, " "); //Nested indenting
        uxr = &ule.uxrs[i];

        if (uxr_is_placeholder(uxr))
            fprintf(outf, "P: xid=%016" PRIx64, uxr->xid);
        else if (uxr_is_delete(uxr))
            fprintf(outf, "D: xid=%016" PRIx64, uxr->xid);
        else {
            assert(uxr_is_insert(uxr));
            fprintf(outf, "I: xid=%016" PRIx64 " val=", uxr->xid);
            toku_print_BYTESTRING(outf, uxr->vallen, uxr->valp);
        }
    }
    fprintf(outf, "}");
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
    ule->keylen   = keylen;
    ule->keyp     = keyp;
    ule->num_uxrs = 1;
    ule->uxrs[0]  = committed_delete;
}

static inline u_int8_t 
min_u8(u_int8_t a, u_int8_t b) {
    u_int8_t rval = a < b ? a : b;
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
    if (ule->num_uxrs > 1) {
        u_int8_t max_index = min_u8(ule->num_uxrs, xids_get_num_xids(xids)) - 1;
        u_int8_t ica_index = max_index;
        u_int8_t index;
        for (index = 1; index <= max_index; index++) { //xids at index 0 are defined to be equal.
            TXNID current_msg_xid = xids_get_xid(xids, index);
            TXNID current_ule_xid = ule_get_xid(ule, index);
            if (current_msg_xid != current_ule_xid) {
                //ica is innermost transaction with matching xids.
                ica_index = index - 1;
                break;
            }
        }

        //If ica is the innermost uxr in the leafentry, no commits are necessary.
        if (ica_index < ule->num_uxrs - 1) {
            ule_promote_innermost_to_index(ule, ica_index);
        }
    }
}

// Purpose is to promote the value (and type) of the innermost transaction
// record to the uxr at the specified index (keeping the txnid of the uxr at
// specified index.)
static void 
ule_promote_innermost_to_index(ULE ule, u_int8_t index) {
    assert(ule->num_uxrs - 1 > index);
    UXR old_innermost_uxr = ule_get_innermost_uxr(ule);
    assert(!uxr_is_placeholder(old_innermost_uxr));
    TXNID new_innermost_xid = ule->uxrs[index].xid;
    ule->num_uxrs  = index; //Discard old uxr at index (and everything inner)
    if (uxr_is_delete(old_innermost_uxr)) {
        ule_push_delete_uxr(ule, new_innermost_xid);
    }
    else {
        ule_push_insert_uxr(ule,
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
    ule_push_insert_uxr(ule, this_xid, vallen, valp);
}

// Purpose is to apply a delete message to this leafentry:
static void 
ule_apply_delete(ULE ule, XIDS xids) {
    ule_prepare_for_new_uxr(ule, xids);
    TXNID this_xid = xids_get_innermost_xid(xids);  // xid of transaction doing this delete
    ule_push_delete_uxr(ule, this_xid);
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
    assert(this_xid!=0);
    if (ule_get_innermost_xid(ule) == this_xid) {
        assert(ule->num_uxrs>1);
        ule_remove_innermost_uxr(ule);                    
        ule_remove_innermost_placeholders(ule); 
    }
    assert(ule->num_uxrs > 0);
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
    assert(this_xid!=0);
    if (ule_get_innermost_xid(ule) == this_xid) { 
        //ule->uxrs[ule->num_uxrs-1] is the innermost (this transaction)
        //ule->uxrs[ule->num_uxrs-2] is the 2nd innermost
        assert(ule->num_uxrs > 1);
        //We want to promote the innermost uxr one level out.
        ule_promote_innermost_to_index(ule, ule->num_uxrs-2);
    }
}

///////////////////
// Helper functions called from the functions above:
//

// Purpose is to record an insert for this transaction (and set type correctly).
static void 
ule_push_insert_uxr(ULE ule, TXNID xid, u_int32_t vallen, void * valp) {
    UXR uxr     = ule_get_first_empty_uxr(ule);
    uxr->xid    = xid;
    uxr->vallen = vallen;
    uxr->valp   = valp;
    uxr->type   = XR_INSERT;
    ule->num_uxrs++;
}

// Purpose is to record a delete for this transaction.  If this transaction
// is the root transaction, then truly delete the leafentry by marking the 
// ule as empty.
static void 
ule_push_delete_uxr(ULE ule, TXNID xid) {
    UXR uxr     = ule_get_first_empty_uxr(ule);
    uxr->xid    = xid;
    uxr->type   = XR_DELETE;
    ule->num_uxrs++;
}

// Purpose is to push a placeholder on the top of the leafentry's transaction stack.
static void 
ule_push_placeholder_uxr(ULE ule, TXNID xid) {
    UXR uxr     = ule_get_first_empty_uxr(ule);
    uxr->xid    = xid;
    uxr->type   = XR_PLACEHOLDER;
    ule->num_uxrs++;
}

// Return innermost transaction record.
static UXR 
ule_get_innermost_uxr(ULE ule) {
    assert(ule->num_uxrs > 0);
    UXR rval = &(ule->uxrs[ule->num_uxrs - 1]);
    return rval;
}

// Return innermost transaction record.
static UXR 
ule_get_outermost_uxr(ULE ule) {
    assert(ule->num_uxrs > 0);
    UXR rval = &(ule->uxrs[0]);
    return rval;
}

// Return first empty transaction record
static UXR 
ule_get_first_empty_uxr(ULE ule) {
    assert(ule->num_uxrs < MAX_TRANSACTION_RECORDS);
    UXR rval = &(ule->uxrs[ule->num_uxrs]);
    return rval;
}

// Remove the innermost transaction (pop the leafentry's stack), undoing
// whatever the innermost transaction did.
static void 
ule_remove_innermost_uxr(ULE ule) {
    //It is possible to remove the committed delete at first insert.
    assert(ule->num_uxrs > 0);
    ule->num_uxrs--;
}

static TXNID 
ule_get_innermost_xid(ULE ule) {
    TXNID rval = ule_get_xid(ule, ule->num_uxrs - 1);
    return rval;
}

static TXNID 
ule_get_xid(ULE ule, u_int8_t index) {
    assert(index < ule->num_uxrs);
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
	assert(ule->num_uxrs > 1);	// outermost is committed, cannot be placeholder
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
    assert(ule->num_uxrs > 0);
    TXNID ica_xid  = ule_get_innermost_xid(ule); // xid of ica
    TXNID this_xid = xids_get_innermost_xid(xids); // xid of this transaction
    if (ica_xid != this_xid) {		// if this transaction is the ICA, don't push any placeholders
	u_int8_t index           = xids_find_index_of_xid(xids, ica_xid) + 1; // Get index of next inner transaction after ICA
	TXNID    current_msg_xid = xids_get_xid(xids, index);
	while (current_msg_xid != this_xid) { // Placeholder for each transaction before this transaction
	    ule_push_placeholder_uxr(ule, current_msg_xid);
	    index++;
	    current_msg_xid = xids_get_xid(xids, index);
	}
    }
}



/////////////////////////////////////////////////////////////////////////////////
//  This layer of abstraction (uxr_xxx) understands uxr and nothing else.
//

static inline BOOL
uxr_type_is_insert(u_int8_t type) {
    BOOL rval = (BOOL)(type == XR_INSERT);
    return rval;
}

static inline BOOL
uxr_is_insert(UXR uxr) {
    return uxr_type_is_insert(uxr->type);
}

static inline BOOL
uxr_type_is_delete(u_int8_t type) {
    BOOL rval = (BOOL)(type == XR_DELETE);
    return rval;
}

static inline BOOL
uxr_is_delete(UXR uxr) {
    return uxr_type_is_delete(uxr->type);
}

static inline BOOL
uxr_type_is_placeholder(u_int8_t type) {
    BOOL rval = (BOOL)(type == XR_PLACEHOLDER);
    return rval;
}

static inline BOOL
uxr_is_placeholder(UXR uxr) {
    return uxr_type_is_placeholder(uxr->type);
}





#ifdef IMPLICIT_PROMOTION_ON_QUERY


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

// Wrapper code to support backwards compatibility with version 10 (until we don't want it).
// These wrappers should be removed if/when we remove support for version 10 leafentries.
#include "backwards_10.h"
void
toku_upgrade_ule_init_empty_ule(ULE ule, u_int32_t keylen, void * keyp) {
    ule_init_empty_ule(ule, keylen, keyp);
}
void
toku_upgrade_ule_remove_innermost_uxr(ULE ule) {
    ule_remove_innermost_uxr(ule);
}
void
toku_upgrade_ule_push_insert_uxr(ULE ule, TXNID xid, u_int32_t vallen, void * valp) {
    ule_push_insert_uxr(ule, xid, vallen, valp);
}
void
toku_upgrade_ule_push_delete_uxr(ULE ule, TXNID xid) {
    ule_push_delete_uxr(ule, xid);
}



