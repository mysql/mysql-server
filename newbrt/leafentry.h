#ifndef LEAFENTRY_H
#define LEAFENTRY_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


/* In the past, leaves simply contained key-value pairs.
 * In this implementatoin, leaf values are more complex
 * They can contain a committed value:
 *   - Which can be "not-present",
 *   - Or a key-value pair.
 * They can contain a provisional value, which depends on whether a particular transaction commits or aborts.
 *   - A not-present value
 *   - Or a key-value pair.
 *   - Or there can be no provisional value at all (that is, the value doesn't depend on the transaction.)
 * Note that if both the provisional value and the committed value are not-present, then there is simply no entry in the leaf.
 * So let's enumerate the possibilities:
 *     committed pair                                 A committed pair unaffected by any incomplete transaction.
 *     committed pair and provisional pair            A committed pair to provisionally be replaced by a new pair.
 *     committed pair and provisional delete          A committed pair that will be deleted
 *     provisional pair                               No committed pair, but if a provisional pair to add.
 *
 * In the case of a committed pair and a provisional pair, the key is the same in both cases.  The value can be different.
 *
 * For DUPSORT databases, the key-value pair is everything, so we need only represent the key-value pair once.  So the cases are
 *    committed pair
 *    committed pair provisionally deleted
 *    provisional pair
 * The case of a committed pair and a provisional pair can be represented by a committed pair, since it doesn't matter whether the transction aborts or commits, the value is the same.
 */

#include <toku_portability.h>
#include "rbuf.h"
#include "x1764.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

#if 0
    Memory format of packed nodup leaf entry
    CONSTANTS:
        num_uxrs
        keylen
    Run-time-constants
        voffset of val/vallen??? (for le_any_val) This must be small if it is interpreted as voffset = realoffset_of_val - keylen
            GOOD performance optimization.
            ALSO good for simplicity (no having to scan packed version)
        key[]
    variable length
        
        
    Memory format of packed dup leaf entry
    CONSTANTS:
        num_uxrs
        keylen
        vallen
    Run-time-constants
        key[]
        val[]
#endif
#if TOKU_WINDOWS
#pragma pack(push, 1)
#endif
struct __attribute__ ((__packed__)) leafentry {
    uint8_t  type;
    uint32_t keylen;
    union {
        struct __attribute__ ((__packed__)) leafentry_clean {
            uint32_t vallen;
            uint8_t  key_val[0];     //Actual key, then actual val
        } clean;
        struct __attribute__ ((__packed__)) leafentry_mvcc {
            uint32_t num_cxrs;
            uint8_t  num_pxrs;
            u_int8_t key_xrs[0]; //Actual key,
                                 //then interesting TXNIDs
                                 //then interesting lengths (type bit is MSB of length)
                                 //then interesting data
                                 //then other transaction records
        } mvcc;
    } u;
};
#if TOKU_WINDOWS
#pragma pack(pop)
#endif

enum { LE_CLEAN = 0, LE_MVCC = 1 };
#define LE_CLEAN_MEMSIZE(keylen, vallen)                         \
    (sizeof(((LEAFENTRY)NULL)->type)      /* num_uxrs */   \
    +sizeof(((LEAFENTRY)NULL)->keylen)          /* keylen */     \
    +sizeof(((LEAFENTRY)NULL)->u.clean.vallen)  /* vallen */     \
    +keylen                                     /* actual key */ \
    +vallen)                                    /* actual val */

#define LE_MVCC_COMMITTED_HEADER_MEMSIZE                          \
    (sizeof(((LEAFENTRY)NULL)->type)      /* num_uxrs */    \
    +sizeof(((LEAFENTRY)NULL)->keylen)          /* keylen */      \
    +sizeof(((LEAFENTRY)NULL)->u.mvcc.num_cxrs) /* committed */   \
    +sizeof(((LEAFENTRY)NULL)->u.mvcc.num_pxrs) /* provisional */ \
    +sizeof(TXNID)                              /* transaction */ \
    +sizeof(uint32_t)                           /* length+bit */  \
    +sizeof(uint32_t))                          /* length+bit */ 

#define LE_MVCC_COMMITTED_MEMSIZE(keylen, vallen)    \
    (LE_MVCC_COMMITTED_HEADER_MEMSIZE                \
    +keylen                        /* actual key */  \
    +vallen)                       /* actual val */


typedef struct leafentry *LEAFENTRY;


u_int32_t toku_le_crc(LEAFENTRY v);

size_t leafentry_memsize (LEAFENTRY le); // the size of a leafentry in memory.
size_t leafentry_disksize (LEAFENTRY le); // this is the same as logsizeof_LEAFENTRY.  The size of a leafentry on disk.
void wbuf_LEAFENTRY(struct wbuf *w, LEAFENTRY le);
void wbuf_nocrc_LEAFENTRY(struct wbuf *w, LEAFENTRY le);
int print_leafentry (FILE *outf, LEAFENTRY v); // Print a leafentry out in human-readable form.

int le_latest_is_del(LEAFENTRY le); // Return true if it is a provisional delete.
uint32_t le_num_xids(LEAFENTRY le); //Return how many xids exist (0 does not count)
int le_has_xids(LEAFENTRY le, XIDS xids); // Return true transaction represented by xids is still provisional in this leafentry (le's xid stack is a superset or equal to xids)
u_int32_t le_latest_keylen (LEAFENTRY le); // Return the latest keylen.
void*     le_latest_val (LEAFENTRY le); // Return the latest val (return NULL for provisional deletes)
u_int32_t le_latest_vallen (LEAFENTRY le); // Return the latest vallen.  Returns 0 for provisional deletes.
void* le_latest_val_and_len (LEAFENTRY le, u_int32_t *len);

 // Return any key or value (even if it's only provisional).
void* le_key (LEAFENTRY le);
u_int32_t le_keylen (LEAFENTRY le);
void* le_key_and_len (LEAFENTRY le, u_int32_t *len);

u_int64_t le_outermost_uncommitted_xid (LEAFENTRY le);

void le_clean_xids(LEAFENTRY le, size_t *new_leafentry_memorysize, size_t *new_leafentry_disksize);
//Effect: Fully promotes le.  Returns new memory/disk size.
//        Reuses the memory of le.
//        Memory size is guaranteed to reduce.
//           result of leafentry_memsize() changes
//        Pointer to le is reused.
//           No need to update omt if it just points to the leafentry.
//        Does not change results of:
//           le_latest_is_del()
//           le_latest_keylen()
//           le_latest_vallen()
//           le_keylen()
//        le_outermost_uncommitted_xid will return 0 after this.
//        Changes results of following pointer functions, but memcmp of old/new answers would say they're the same.
//          Note: You would have to memdup the old answers before calling le_full_promotion, if you want to run the comparison
//           le_latest_val()
//           le_key()
//        le_outermost_uncommitted_xid will return 0 after this
//        key/val pointers will change, but data pointed to by them will be the same
//           as before
//Requires: le is not a provdel
//Requires: le is not marked committed
//Requires: The outermost uncommitted xid in le has actually committed (le was not yet updated to reflect that)

void
le_committed_mvcc(uint8_t *key, uint32_t keylen,
                  uint8_t *val, uint32_t vallen,
                  TXNID xid,
                  void (*bytes)(struct dbuf *dbuf, const void *bytes, int nbytes),
                  struct dbuf *d);
void
le_clean(uint8_t *key, uint32_t keylen,
         uint8_t *val, uint32_t vallen,
         void (*bytes)(struct dbuf *dbuf, const void *bytes, int nbytes),
         struct dbuf *d);



  //Callback contract:
  //  Returns:
  //      0:  Ignore this entry and go on to next one.
  //      TOKUDB_ACCEPT: Quit early, accept this transaction record and return appropriate data
  //      r|r!=0&&r!=TOKUDB_ACCEPT:  Quit early, return r
  typedef int(*LE_ITERATE_CALLBACK)(TXNID id, TOKUTXN context);

  int le_iterate_is_empty(LEAFENTRY le, LE_ITERATE_CALLBACK f, BOOL *is_empty, TOKUTXN context);

  int le_iterate_val(LEAFENTRY le, LE_ITERATE_CALLBACK f, void** valpp, u_int32_t *vallenp, TOKUTXN context);


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

