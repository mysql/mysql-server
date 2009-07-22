/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Purpose of this file is to provide the world with everything necessary
 * to use the nested transaction logic and nothing else.  No internal
 * requirements of the nested transaction logic belongs here.
 */

#ifndef ULE_H
#define ULE_H

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
    u_int8_t   type;     // delete/insert/placeholder
    u_int32_t  vallen;   // number of bytes in value
    void *     valp;     // pointer to value  (Where is value really stored?)
    TXNID      xid;      // transaction id
    // Note: when packing ule into a new leafentry, will need
    //       to copy actual data from valp to new leafentry
} UXR_S, *UXR;

// Unpacked Leaf Entry is of fixed size because it's just on the 
// stack and we care about ease of access more than the memory footprint.
typedef struct {     // unpacked leaf entry
    u_int8_t   num_uxrs;   // how many of uxrs[] are valid
    u_int32_t  keylen;
    void *     keyp;
    UXR_S      uxrs[MAX_TRANSACTION_RECORDS];    // uxrs[0] is outermost, uxrs[num_uxrs-1] is innermost
} ULE_S, *ULE;

int apply_msg_to_leafentry(BRT_MSG   msg,
			   LEAFENTRY old_leafentry, // NULL if there was no stored data.
			   size_t *new_leafentry_memorysize, 
			   size_t *new_leafentry_disksize, 
			   LEAFENTRY *new_leafentry_p,
			   OMT omt, 
			   struct mempool *mp, 
			   void **maybe_free);

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

#endif  // ULE_H

