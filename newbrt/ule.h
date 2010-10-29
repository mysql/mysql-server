/* -*- mode: C; c-basic-offset: 4 -*- */

/* Purpose of this file is to provide the world with everything necessary
 * to use the nested transaction logic and nothing else.  No internal
 * requirements of the nested transaction logic belongs here.
 */

#ifndef TOKU_ULE_H
#define TOKU_ULE_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

//1 does much slower debugging
#define ULE_DEBUG 0

/////////////////////////////////////////////////////////////////////////////////
// Following data structures are the unpacked format of a leafentry. 
//   * ule is the unpacked leaf entry, that contains an array of unpacked
//     transaction records
//   * uxr is the unpacked transaction record
//


//Types of transaction records.
enum {XR_INSERT      = 1,
      XR_DELETE      = 2,
      XR_PLACEHOLDER = 3};

typedef struct {     // unpacked transaction record
    uint8_t   type;     // delete/insert/placeholder
    uint32_t  vallen;   // number of bytes in value
    void *    valp;     // pointer to value  (Where is value really stored?)
    TXNID     xid;      // transaction id
    // Note: when packing ule into a new leafentry, will need
    //       to copy actual data from valp to new leafentry
} UXR_S, *UXR;

// Unpacked Leaf Entry is of fixed size because it's just on the 
// stack and we care about ease of access more than the memory footprint.
typedef struct {     // unpacked leaf entry
    uint32_t  num_puxrs;   // how many of uxrs[] are provisional
    uint32_t  num_cuxrs;   // how many of uxrs[] are committed
    uint32_t  keylen;
    void *    keyp;
    UXR_S     uxrs_static[MAX_TRANSACTION_RECORDS*2];    // uxrs[0] is oldest committed (txn commit time, not txn start time), uxrs[num_cuxrs] is outermost provisional value (if any exist/num_puxrs > 0)
    UXR       uxrs;                                      //If num_cuxrs < MAX_TRANSACTION_RECORDS then &uxrs_static[0].
                                                         //Otherwise we use a dynamically allocated array of size num_cuxrs + 1 + MAX_TRANSATION_RECORD.
} ULE_S, *ULE;

int apply_msg_to_leafentry(BRT_MSG   msg,
			   LEAFENTRY old_leafentry, // NULL if there was no stored data.
			   size_t *new_leafentry_memorysize, 
			   size_t *new_leafentry_disksize, 
			   LEAFENTRY *new_leafentry_p,
			   OMT omt, 
			   struct mempool *mp, 
			   void **maybe_free,
                           OMT snapshot_xids,
                           OMT live_list_reverse);

void 
test_msg_modify_ule(ULE ule, BRT_MSG msg);


//////////////////////////////////////////////////////////////////////////////////////
//Functions exported for test purposes only (used internally for non-test purposes).
void le_unpack(ULE ule,  LEAFENTRY le);
int le_pack(ULE ule,                            // data to be packed into new leafentry
	size_t *new_leafentry_memorysize, 
	size_t *new_leafentry_disksize, 
	LEAFENTRY * const new_leafentry_p,   // this is what this function creates
	OMT omt, 
	struct mempool *mp, 
	void **maybe_free);


size_t le_memsize_from_ule (ULE ule);
void ule_cleanup(ULE ule);

TXNID toku_get_youngest_live_list_txnid_for(TXNID xc, OMT live_list_reverse);
#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif  // TOKU_ULE_H

