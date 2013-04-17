#if !defined(TOKU_LOCKTREE_INTERNAL_H)
#define TOKU_LOCKTREE_INTERNAL_H

#include <range_tree/rangetree.h>
#include <lth.h>
#include <rth.h>
#include <idlth.h>
#include <newbrt/omt.h>

#define TOKU_LT_USE_BORDERWRITE 1

struct __toku_ltm {
    /** The maximum number of locks allowed for the environment. */
    uint64_t          locks_limit;
    /** The current number of locks for the environment. */
    uint64_t          curr_locks;
    /** The maximum amount of memory for locks allowed for the environment. */
    uint64_t          lock_memory_limit;
    /** The current amount of memory for locks for the environment. */
    uint64_t          curr_lock_memory;
    /** Status / accountability information */
    LTM_STATUS_S       status;
    /** The list of lock trees it manages. */
    toku_lth*          lth;
    /** List of lock-tree DB mappings. Upon a request for a lock tree given
        a DB, if an object for that DB exists in this list, then the lock tree
        is retrieved from this list, otherwise, a new lock tree is created
        and the new mapping of DB and Lock tree is stored here */
    toku_idlth*        idlth;
    /** The panic function */
    int               (*panic)(DB*, int);

    toku_pthread_mutex_t mutex;
    bool mutex_locked;

    struct timeval lock_wait_time;
};

/** \brief The lock tree structure */
struct __toku_lock_tree {
    /** Lock tree manager */
    toku_ltm* mgr;
    // for comparisons
    struct __toku_db fake_db; // dummy db used for comparisons
    DESCRIPTOR_S desc_s;
#if TOKU_LT_USE_BORDERWRITE
    toku_range_tree*    borderwrite; /**< See design document */
#endif
    toku_rth*           rth;         /**< Stores local(read|write)set tables */
    /** Whether lock escalation is allowed. */
    bool                lock_escalation_allowed;
    /** Function to retrieve the key compare function from the database. */
    toku_dbt_cmp compare_fun;
    /** The number of references held by DB instances and transactions to this lock tree*/
    uint32_t          ref_count;
    /** DICTIONARY_ID associated with the lock tree */
    DICTIONARY_ID      dict_id;
    OMT                lock_requests;

    toku_pthread_mutex_t mutex;
    bool mutex_locked;

    /** A temporary area where we store the results of various find on 
        the range trees that this lock tree owns 
    Memory ownership: 
     - tree->buf is an array of toku_range's, which the lt owns
       The contents of tree->buf are volatile (this is a buffer space
       that we pass around to various functions, and every time we
       invoke a new function, its previous contents may become 
       meaningless)
     - tree->buf[i].left, .right are toku_points (ultimately a struct), 
       also owned by lt. We gave a pointer only to this memory to the 
       range tree earlier when we inserted a range, but the range tree
       does not own it!
     - tree->buf[i].{left,right}.key_payload is owned by
       the lt, we made copies from the DB at some point
    */
    toku_range*         buf;      
    uint32_t            buflen;      /**< The length of buf */
    toku_range*         bw_buf;
    uint32_t            bw_buflen;
    toku_range*         verify_buf;
    uint32_t            verify_buflen;
};

toku_range_tree* toku_lt_ifexist_selfread(toku_lock_tree* tree, TXNID txn);

toku_range_tree* toku_lt_ifexist_selfwrite(toku_lock_tree* tree, TXNID txn);

#include "txnid_set.h"

// internal function that finds all transactions that conflict with a given lock request
// for read lock requests
//     conflicts = all transactions in the BWT that conflict with the lock request
// for write lock requests
//     conflicts = all transactions in the GRT that conflict with the lock request UNION
//                 all transactions in the BWT that conflict with the lock request
// adds all of the conflicting transactions to the conflicts transaction set
// returns an error code (0 == success)
int toku_lt_get_lock_request_conflicts(toku_lock_tree *tree, toku_lock_request *lock_request, txnid_set *conflicts);

// returns the lock request state
toku_lock_request_state toku_lock_request_get_state(toku_lock_request *lock_request);

/**

   \brief A 2D BDB-inspired point.

   Observe the toku_point, and marvel! 
   It makes the pair (key, data) into a 1-dimensional point,
   on which a total order is defined by toku_lt_point_cmp.
   Additionally, we have points at +infty and -infty as
   key_payload = (void*) toku_lt_infinity or 
   key_payload = (void*) toku_lt_neg infinity 
 */
struct __toku_point {
    toku_lock_tree* lt;           /**< The lock tree, where toku_lt_point_cmp 
                                       is defined */
    void*           key_payload;  /**< The key ... */
    uint32_t        key_len;      /**< and its length */
};
#if !defined(__TOKU_POINT)
#define __TOKU_POINT
typedef struct __toku_point toku_point;
#endif

int toku_lt_point_cmp(const toku_point* x, const toku_point* y);

#endif
